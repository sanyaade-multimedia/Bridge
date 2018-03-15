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
	int channel = 0;
	int sampleRate = 44100;
	float params[NUM_PARAMS] = {};

	RingBuffer<uint8_t, (1<<15)> sendQueue;
	int server = -1;
	bool serverOpen = false;
	bool quitRequested = false;
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
		disconnect();

		// Open socket
		server = socket(AF_INET, SOCK_STREAM, 0);
		if (server < 0)
			return;

#ifndef ARCH_LIN
		// Avoid SIGPIPE
		int flag = 1;
		setsockopt(server, SOL_SOCKET, SO_NOSIGPIPE, &flag, sizeof(int));
#endif

		// Connect to 127.0.0.1 on port 5000
		struct sockaddr_in serverAddr;
		memset(&serverAddr, 0, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
		serverAddr.sin_port = htons(5000);
		err = ::connect(server, (struct sockaddr*) &serverAddr, sizeof(serverAddr));
		if (err) {
			disconnect();
			return;
		}
	}

	void disconnect() {
		int err;
		err = close(server);
		(void) err;
		server = -1;
	}

	void flush() {
		int err;

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
		ssize_t written = ::send(server, sendBuffer, sendLength, sendFlags);
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
		pushSetChannel();
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

	void pushSetChannel() {
		push<uint8_t>(CHANNEL_SET_COMMAND);
		push<uint8_t>(channel);
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

	void setChannel(int channel) {
		if (channel == this->channel)
			return;
		this->channel = channel;
		if (!serverOpen)
			return;
		pushSetChannel();
	}

	void setSampleRate(int sampleRate) {
		if (sampleRate == this->sampleRate)
			return;
		this->sampleRate = sampleRate;
		if (!serverOpen)
			return;
		pushSetSampleRate();
	}

	int getChannel() {
		return channel;
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
