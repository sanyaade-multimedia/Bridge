#include "util/common.hpp"
#include "dsp/ringbuffer.hpp"

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


enum BridgeCommand {
	NO_COMMAND = 0,
	START_COMMAND,
	QUIT_COMMAND,
	PORT_SET_COMMAND,
	MIDI_MESSAGE_SEND_COMMAND,
	AUDIO_SAMPLE_RATE_SET_COMMAND,
	AUDIO_CHANNELS_SET_COMMAND,
	AUDIO_BUFFER_SEND_COMMAND,
	AUDIO_ACTIVATE,
	AUDIO_DEACTIVATE,
	NUM_COMMANDS
};


struct BridgeClient {
	int port = 0;
	int sampleRate = 44100;
	float params[NUM_PARAMS] = {};

	RingBuffer<uint8_t, (1<<15)> sendQueue;
	int server = -1;
	/** Whether the public API pushes to the queue */
	bool serverOpen = false;
	/** Whether the client has requested to shut down permanently */
	bool quitRequested = false;
	/** Whether the client has requested to close temporarily */
	bool closeRequested = false;
	bool audioActive = false;

	std::thread bridgeThread;
	std::mutex bridgeMutex;
	std::condition_variable bridgeCv;

	BridgeClient() {
		bridgeThread = std::thread(&BridgeClient::run, this);
	}

	~BridgeClient() {
		quitRequested = true;
		bridgeThread.join();
	}

	// Bridge Thread methods

	void connect() {
		int err;

		// Initialize sockets
#ifdef ARCH_WIN
		WSADATA wsaData;
		err = WSAStartup(MAKEWORD(2,2), &wsaData);
		defer({
			WSACleanup();
		});
		if (err) {
			fprintf(stderr, "Could not initialize Winsock\n");
			return;
		}
#endif


		// Get address
#ifdef ARCH_WIN
		struct addrinfo hints;
		struct addrinfo *result = NULL;
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;
		err = getaddrinfo("127.0.0.1", "5000", &hints, &result);
		if (err) {
			fprintf(stderr, "Could not get Bridge client address\n");
			return;
		}
		defer({
			freeaddrinfo(result);
		});
#else
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
		addr.sin_port = htons(5000);
#endif

		// Open socket
#ifdef ARCH_WIN
		server = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
#else
		server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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

#ifdef ARCH_WIN
		err = ::connect(server, result->ai_addr, (int)result->ai_addrlen);
#else
		err = ::connect(server, (struct sockaddr*) &serverAddr, sizeof(serverAddr));
#endif
		if (err) {
			disconnect();
			return;
		}
	}

	void disconnect() {
		close(server);
		server = -1;
	}

	void flush() {
		// Length might be 0
		size_t sendLength = sendQueue.size();
		uint8_t sendBuffer[sendLength];
		if (sendLength > 0)
			sendQueue.shiftBuffer(sendBuffer, sendLength);
#ifdef ARCH_LIN
		int sendFlags = MSG_NOSIGNAL;
#else
		int sendFlags = 0;
#endif
		ssize_t written = ::send(server, (const char*) sendBuffer, sendLength, 0);
		if (written < 0)
			serverOpen = false;

		// // Turn off Nagle
		// int flag = 1;
		// err = setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
		// // Turn on Nagle
		// flag = 0;
		// err = setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
		// (void) err;
	}

	void run() {
		while (!quitRequested) {
			// Wait before connecting or reconnecting
			std::this_thread::sleep_for(std::chrono::duration<double>(0.5));
			// Connect
			connect();
			if (server < 0)
				continue;
			welcome();
			serverOpen = true;
			// Flush buffer in a loop
			while (!quitRequested && serverOpen) {
				std::unique_lock<std::mutex> lock(bridgeMutex);
				auto timeout = std::chrono::duration<double>(10e-3);
				auto cond = [&] {
					return !sendQueue.empty();
				};
				if (bridgeCv.wait_for(lock, timeout, cond)) {
				}
				else {
					// Do nothing if timed out, just loop around again
				}
				flush();
			}
			// Clear queue and disconnect
			serverOpen = false;
			sendQueue.clear();
			disconnect();
		}
	}

	void welcome() {
		pushPassword();
		pushSetPort();
		pushSetSampleRate();
		pushSetAudioActive();
	}

	// Send queue methods

	void push(const uint8_t *buffer, int length) {
		if (sendQueue.capacity() < (size_t) length) {
			closeRequested = true;
			return;
		}
		sendQueue.pushBuffer(buffer, length);
		bridgeCv.notify_one();
	}

	template <typename T>
	void push(T x) {
		push((uint8_t*) &x, sizeof(x));
	}

	void pushPassword() {
		const int password = 0xff00fefd;
		push<uint32_t>(password);
	}

	void pushSetPort() {
		push<uint8_t>(PORT_SET_COMMAND);
		push<uint8_t>(port);
		for (int i = 0; i < NUM_PARAMS; i++)
			pushSetParam(i);
	}

	void pushSetSampleRate() {
		push<uint8_t>(AUDIO_SAMPLE_RATE_SET_COMMAND);
		push<uint32_t>(sampleRate);
	}

	void pushSetParam(int i) {
		push<uint8_t>(MIDI_MESSAGE_SEND_COMMAND);
		uint8_t msg[3];
		msg[0] = (0xc << 8) | 0;
		msg[1] = i;
		msg[2] = roundf(params[i] * 0xff);
		push(msg, 3);
	}

	void pushSetAudioActive() {
		if (audioActive)
			push<uint8_t>(AUDIO_ACTIVATE);
		else
			push<uint8_t>(AUDIO_DEACTIVATE);
	}

	// Public API

	void setPort(int port) {
		if (port == this->port)
			return;
		this->port = port;
		if (!serverOpen)
			return;
		pushSetPort();
	}

	void setSampleRate(int sampleRate) {
		if (sampleRate == this->sampleRate)
			return;
		this->sampleRate = sampleRate;
		if (!serverOpen)
			return;
		pushSetSampleRate();
	}

	int getPort() {
		return port;
	}

	void setParam(int i, float param) {
		if (i < 0 || NUM_PARAMS <= i)
			return;
		if (params[i] == param)
			return;
		params[i] = param;
		if (!serverOpen)
			return;
		pushSetParam(i);
	}

	float getParam(int i) {
		if (0 <= i && i < NUM_PARAMS)
			return params[i];
		else
			return 0.f;
	}

	void processAudio(const float *input, float *output, int frames) {
		for (int i = 0; i < frames; i++) {
			output[2*i + 0] = 0.f;
			output[2*i + 1] = 0.f;
		}

		if (!serverOpen)
			return;
		push<uint8_t>(AUDIO_BUFFER_SEND_COMMAND);
		push<uint32_t>(2*frames);
		push((uint8_t*) input, 2*frames * sizeof(float));
	}

	void setAudioActive(bool audioActive) {
		this->audioActive = audioActive;
		pushSetAudioActive();
	}
};
