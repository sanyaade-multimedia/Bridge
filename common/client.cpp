#include "util/common.hpp"
#include "dsp/ringbuffer.hpp"
#include "bridgeprotocol.hpp"

#include <unistd.h>
#ifdef ARCH_WIN
	#include <winsock2.h>
	#include <ws2tcpip.h>
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netinet/tcp.h>
	#include <fcntl.h>
#endif

#include <thread>


#define NUM_PARAMS 16


using namespace rack;



struct BridgeClient {
	int port = 0;
	float params[NUM_PARAMS] = {};
	int sampleRate = 44100;
	bool audioActive = false;

	int server = -1;
	/** Whether the server is ready to accept public API send() calls */
	bool ready = false;
	/** Whether the client should stop attempting to reconnect permanently */
	bool running = false;

	std::thread runThread;

	BridgeClient() {
		runThread = std::thread(&BridgeClient::run, this);
	}

	~BridgeClient() {
		running = false;
		runThread.join();
	}

	void run() {
		initialize();
		running = true;
		while (running) {
			// Wait before connecting or reconnecting
			std::this_thread::sleep_for(std::chrono::duration<double>(0.1));
			// Connect
			connect();
			welcome();
			ready = true;
			// Wait for server to disconnect
			while (running && server >= 0) {
				std::this_thread::sleep_for(std::chrono::duration<double>(0.1));
			}
			ready = false;
			disconnect();
		}
	}

	void initialize() {
		// Initialize sockets
#ifdef ARCH_WIN
		WSADATA wsaData;
		err = WSAStartup(MAKEWORD(2, 2), &wsaData);
		defer({
			WSACleanup();
		});
		if (err) {
			fprintf(stderr, "Could not initialize Winsock\n");
			return;
		}
#endif
	}

	void connect() {
		int err;

		// Get address
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
#ifdef ARCH_WIN
		InetPton(AF_INET, "127.0.0.1", &addr.sin_addr);
#else
		inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
#endif
		addr.sin_port = htons(5000);

		// Open socket
		server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef ARCH_WIN
		if (server == INVALID_SOCKET)
			server = -1;
#endif
		if (server < 0) {
			fprintf(stderr, "Bridge server socket() failed\n");
			return;
		}
		defer({
			close(server);
		});

		// Avoid SIGPIPE
#ifdef ARCH_MAC
		int flag = 1;
		setsockopt(server, SOL_SOCKET, SO_NOSIGPIPE, &flag, sizeof(int));
#endif

		// Connect socket
		err = ::connect(server, (struct sockaddr*) &addr, sizeof(addr));
		if (err) {
			disconnect();
			return;
		}
	}

	void disconnect() {
		if (server >= 0)
			close(server);
		server = -1;
	}

	/** Returns true if successful */
	bool send(const void *buffer, int length) {
		if (length <= 0)
			return false;
		if (server < 0)
			return false;

#ifdef ARCH_LIN
		int flags = MSG_NOSIGNAL;
#else
		int flags = 0;
#endif
		ssize_t actual = ::send(server, buffer, length, flags);
		if (actual != length) {
			disconnect();
			return false;
		}
		return true;
	}

	template <typename T>
	bool send(T x) {
		return send(&x, sizeof(x));
	}

	/** Returns true if successful */
	bool recv(void *buffer, int length) {
		if (length <= 0)
			return false;
		if (!ready)
			return false;

#ifdef ARCH_LIN
		int flags = MSG_NOSIGNAL;
#else
		int flags = 0;
#endif
		ssize_t actual = ::recv(server, buffer, length, flags);
		if (actual != length) {
			disconnect();
			return false;
		}
		return true;
	}

	template <typename T>
	bool recv(T *x) {
		return recv(x, sizeof(*x));
	}

	void flush() {
		// int err;
		// // Turn off Nagle
		// int flag = 1;
		// err = setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
		// // Turn on Nagle
		// flag = 0;
		// err = setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
		// (void) err;
	}

	// Private API

	void welcome() {
		if (!ready)
			return;

		send<uint32_t>(BRIDGE_HELLO);
		sendSetPort();
	}

	void sendSetPort() {
		if (!ready)
			return;

		send<uint8_t>(PORT_SET_COMMAND);
		send<uint8_t>(port);
		for (int i = 0; i < NUM_PARAMS; i++)
			sendSetParam(i);
		sendSetSampleRate();
		sendSetAudioActive();
	}

	void sendSetSampleRate() {
		if (!ready)
			return;

		send<uint8_t>(AUDIO_SAMPLE_RATE_SET_COMMAND);
		send<uint32_t>(sampleRate);
	}

	void sendSetParam(int i) {
		if (!ready)
			return;

		send<uint8_t>(MIDI_MESSAGE_SEND_COMMAND);
		uint8_t msg[3];
		msg[0] = (0xc << 8) | 0;
		msg[1] = i;
		msg[2] = roundf(params[i] * 0xff);
		send(msg, 3);
	}

	void sendSetAudioActive() {
		if (!ready)
			return;

		if (audioActive)
			send<uint8_t>(AUDIO_ACTIVATE);
		else
			send<uint8_t>(AUDIO_DEACTIVATE);
	}

	// Public API

	void setPort(int port) {
		if (port == this->port)
			return;
		this->port = port;
		sendSetPort();
	}

	void setSampleRate(int sampleRate) {
		if (sampleRate == this->sampleRate)
			return;
		this->sampleRate = sampleRate;
		sendSetSampleRate();
	}

	int getPort() {
		return port;
	}

	void setParam(int i, float param) {
		if (!(0 <= i && i < NUM_PARAMS))
			return;
		if (params[i] == param)
			return;
		params[i] = param;
		sendSetParam(i);
	}

	float getParam(int i) {
		if (0 <= i && i < NUM_PARAMS)
			return params[i];
		else
			return 0.f;
	}

	void processAudio(const float *input, float *output, int frames) {
		if (!ready) {
			memset(output, 0, 2*frames * sizeof(float));
			return;
		}

		send<uint8_t>(AUDIO_PROCESS_COMMAND);
		send<uint32_t>(2 * frames);
		send((const uint8_t*) input, 2*frames * sizeof(float));
		flush();

		// recv((uint8_t*) output, 2*frames * sizeof(float));
		for (int i = 0; i < frames; i++) {
			output[2 * i + 0] = 0.f;
			output[2 * i + 1] = 0.f;
		}
	}

	void setAudioActive(bool audioActive) {
		this->audioActive = audioActive;
		sendSetAudioActive();
	}
};
