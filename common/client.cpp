#include "util/common.hpp"
#include "dsp/ringbuffer.hpp"
#include "bridgeprotocol.hpp"

#include <stdarg.h>
#include <unistd.h>
#ifdef ARCH_WIN
	#include <winsock2.h>
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netinet/tcp.h>
	#include <fcntl.h>
#endif

#include <thread>
#include <mutex>
#include <chrono>


using namespace rack;


void rack::debug(const char *format, ...) {
	va_list args;
	va_start(args, format);
	fprintf(stderr, "[debug] ");
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	fflush(stderr);
	va_end(args);
}


struct BridgeClient {
	int server = -1;
	/** Whether the server is ready to accept public API send() calls */
	bool ready = false;
	/** Whether the client should stop attempting to reconnect */
	bool running = false;

	int port = 0;
	float params[BRIDGE_NUM_PARAMS] = {};
	int sampleRate = 44100;
	bool audioActive = false;

	std::thread runThread;
	std::recursive_mutex mutex;

	BridgeClient() {
		running = true;
		runThread = std::thread(&BridgeClient::run, this);
	}

	~BridgeClient() {
		running = false;
		runThread.join();
	}

	void run() {
		initialize();
		while (running) {
			// Wait before connecting or reconnecting
			std::this_thread::sleep_for(std::chrono::duration<double>(0.1));
			// Connect
			connect();
			if (server < 0)
				continue;
			debug("Connected");
			welcome();
			ready = true;
			// Wait for server to disconnect
			while (running && ready) {
				std::this_thread::sleep_for(std::chrono::duration<double>(0.1));
			}
			disconnect();
			debug("Disconnected");
		}
	}

	void initialize() {
		int err;
		(void) err;

		// Initialize sockets
#ifdef ARCH_WIN
		WSADATA wsaData;
		err = WSAStartup(MAKEWORD(2, 2), &wsaData);
		defer({
			WSACleanup();
		});
		if (err) {
			debug("Could not initialize Winsock");
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
		addr.sin_port = htons(BRIDGE_PORT);
		addr.sin_addr.s_addr = inet_addr(BRIDGE_HOST);

		// Open socket
		server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (server < 0) {
			debug("Bridge client socket() failed");
			return;
		}

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
		ready = false;
		if (server >= 0)
			close(server);
		server = -1;
	}

	/** Returns true if successful */
	bool send(const void *buffer, int length) {
		if (length <= 0)
			return false;

#ifdef ARCH_LIN
		int flags = MSG_NOSIGNAL;
#else
		int flags = 0;
#endif
		ssize_t actual = ::send(server, (const char*) buffer, length, flags);
		if (actual != length) {
			ready = false;
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

#ifdef ARCH_LIN
		int flags = MSG_NOSIGNAL;
#else
		int flags = 0;
#endif
		ssize_t actual = ::recv(server, (char*) buffer, length, flags);
		if (actual != length) {
			ready = false;
			return false;
		}
		return true;
	}

	template <typename T>
	bool recv(T *x) {
		return recv(x, sizeof(*x));
	}

	void flush() {
		int err;
		// Turn off Nagle
		int flag = 1;
		err = setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
		// Turn on Nagle
		flag = 0;
		err = setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
		(void) err;
	}

	// Private API

	void welcome() {
		std::lock_guard<std::recursive_mutex> lock(mutex);
		send<uint32_t>(BRIDGE_HELLO);
		sendSetPort();
	}

	void sendSetPort() {
		std::lock_guard<std::recursive_mutex> lock(mutex);
		send<uint8_t>(PORT_SET_COMMAND);
		send<uint8_t>(port);
		for (int i = 0; i < BRIDGE_NUM_PARAMS; i++)
			sendSetParam(i);
		sendSetSampleRate();
		sendSetAudioActive();
	}

	void sendSetSampleRate() {
		std::lock_guard<std::recursive_mutex> lock(mutex);
		send<uint8_t>(AUDIO_SAMPLE_RATE_SET_COMMAND);
		send<uint32_t>(sampleRate);
	}

	void sendSetParam(int i) {
		std::lock_guard<std::recursive_mutex> lock(mutex);
		send<uint8_t>(MIDI_MESSAGE_SEND_COMMAND);
		uint8_t msg[3];
		msg[0] = (0xb << 4) | 0;
		msg[1] = i;
		msg[2] = roundf(params[i] * 0x7f);
		send(msg, 3);
		flush();
	}

	void sendSetAudioActive() {
		std::lock_guard<std::recursive_mutex> lock(mutex);
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
		if (ready)
			sendSetPort();
	}

	void setSampleRate(int sampleRate) {
		if (sampleRate == this->sampleRate)
			return;
		this->sampleRate = sampleRate;
		if (ready)
			sendSetSampleRate();
	}

	int getPort() {
		return port;
	}

	void setParam(int i, float param) {
		if (!(0 <= i && i < BRIDGE_NUM_PARAMS))
			return;
		if (params[i] == param)
			return;
		params[i] = param;
		if (ready)
			sendSetParam(i);
	}

	float getParam(int i) {
		if (0 <= i && i < BRIDGE_NUM_PARAMS)
			return params[i];
		else
			return 0.f;
	}

	void processAudio(const float *input, float *output, int frames) {
		memset(output, 0, BRIDGE_OUTPUTS * frames * sizeof(float));
		if (!ready) {
			return;
		}

		std::lock_guard<std::recursive_mutex> lock(mutex);
		auto startTime = std::chrono::high_resolution_clock::now();
		send<uint8_t>(AUDIO_PROCESS_COMMAND);
		send<uint32_t>(frames);
		send(input, BRIDGE_INPUTS * frames * sizeof(float));
		// flush();

		recv(output, BRIDGE_OUTPUTS * frames * sizeof(float));
		auto endTime = std::chrono::high_resolution_clock::now();
		double time = std::chrono::duration<double>(endTime - startTime).count();
		float p = time / ((float) frames / 44100);
		if (p > 0.75)
			debug("%d frames, %f ms, %f%%", frames, 1000.0*time, p);
	}

	void setAudioActive(bool audioActive) {
		this->audioActive = audioActive;
		if (ready)
			sendSetAudioActive();
	}
};
