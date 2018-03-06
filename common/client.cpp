#include "bridge.hpp"
#include "dsp/ringbuffer.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <thread>


#define NUM_PARAMS 16


using namespace rack;


struct BridgeClient {
	int server = -1;
	bool closeRequested = false;
	RingBuffer<uint8_t, (1<<15)> sendQueue;

	/** Starts the Bridge Client Thread */
	void connect() {
		int err;

		// Open socket
		server = socket(AF_INET, SOCK_STREAM, 0);
		assert(server >= 0);

		// Connect to 127.0.0.1 on port 5000
		struct sockaddr_in serverAddr;
		memset(&serverAddr, 0, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
		serverAddr.sin_port = htons(5000);
		err = ::connect(server, (struct sockaddr*) &serverAddr, sizeof(serverAddr));
		if (err) {
			printf("Could not connect to server\n");
			return;
		}
	}

	void flush() {
		int err;

		size_t sendLength = sendQueue.size();
		if (sendLength == 0)
			return;
		printf("%d\n", sendLength);
		return;

		uint8_t sendBuffer[sendLength];
		sendQueue.shiftBuffer(sendBuffer, sendLength);
		ssize_t written = ::send(server, sendBuffer, sendLength, 0);
		// ssize_t written = ::send(server, buffer, length, MSG_NOSIGNAL);
		if (written <= 0)
			closeRequested = true;

		// // Turn off Nagle
		// int flag = 1;
		// err = setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
		// assert(!err);
		// // Turn on Nagle
		// flag = 0;
		// err = setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
		// assert(!err);
	}

	void disconnect() {
		int err;
		err = close(server);
	}

	void run() {
		while (!closeRequested) {
			std::this_thread::sleep_for(std::chrono::duration<double>(0.5));
			connect();
			printf("connected\n");
			while (!closeRequested) {
				std::this_thread::sleep_for(std::chrono::duration<double>(10e-6));
				flush();
			}
			disconnect();
			printf("disconnected\n");
		}
	}

	void push(const uint8_t *buffer, int length) {
		if (sendQueue.capacity() >= (size_t) length) {
			sendQueue.pushBuffer(buffer, length);
		}
	}

	template <typename T>
	void push(T x) {
		push((uint8_t*) &x, sizeof(x));
	}

	template <typename T>
	void pushBuffer(const T *p, int length) {
		push((uint8_t*) p, length * sizeof(*p));
	}
};


struct Bridge {
	BridgeClient client;
	int channel = -1;
	float params[NUM_PARAMS] = {};
	std::thread bridgeThread;

	Bridge() {
		bridgeThread = std::thread(&BridgeClient::run, &client);

		const int password = 0xff00fefd;
		client.push<uint32_t>(password);
	}

	~Bridge() {
		client.push<uint8_t>(QUIT_COMMAND);
		client.closeRequested = true;
		bridgeThread.join();
	}

	void setChannel(int channel) {
		if (channel != this->channel) {
			client.push<uint8_t>(CHANNEL_SET_COMMAND);
			client.push<uint8_t>(channel);
			// client.push<uint8_t>(AUDIO_SAMPLE_RATE_SET_COMMAND);
			// client.push<uint32_t>(44100);
		}
		this->channel = channel;
	}

	int getChannel() {
		return channel;
	}

	void setParam(int i, float param) {
		if (0 <= i && i < NUM_PARAMS)
			params[i] = param;
	}

	float getParam(int i) {
		if (0 <= i && i < NUM_PARAMS)
			return params[i];
		else
			return 0.f;
	}

	void processAudio(const float *input, float *output, int frames) {
		client.push<uint8_t>(AUDIO_BUFFER_SEND_COMMAND);
		client.push<uint32_t>(2*frames);
		client.pushBuffer<float>(input, 2*frames);

		for (int i = 0; i < frames; i++) {
			output[2*i + 0] = 0.f;
			output[2*i + 1] = 0.f;
		}
	}
};
