#include "bridge.hpp"
#include "dsp/ringbuffer.hpp"

#include <thread>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>


using namespace rack;


static const int RECV_BUFFER_SIZE = (1<<12);
static const int RECV_QUEUE_SIZE = (1<<15);


struct BridgeClientConnection {
	int client;
	AppleRingBuffer<uint8_t, RECV_QUEUE_SIZE, 2*RECV_QUEUE_SIZE> queue;
	BridgeCommand currentCommand = START_COMMAND;
	bool closeRequested = false;
	int channel = -1;
	int sampleRate = -1;
	int audioChannels = 0;
	bool audioBufferReceiving = false;
	int audioBufferRemaining = 0;
	FILE *audioOutputFile;

	BridgeClientConnection() {
		audioOutputFile = fopen("out.f32", "w");
		assert(audioOutputFile);
	}

	~BridgeClientConnection() {
		fclose(audioOutputFile);
	}

	std::string getIp() {
		int err;
		// Get client address
		struct sockaddr_in addr;
		socklen_t clientAddrLen = sizeof(addr);
		err = getpeername(client, (struct sockaddr*) &addr, &clientAddrLen);
		assert(!err);

		// Get client IP address
		struct in_addr ipAddr = addr.sin_addr;
		char ipBuffer[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &ipAddr, ipBuffer, INET_ADDRSTRLEN);
		return ipBuffer;
	}

	/** Does not check if the queue has enough data. You must do that yourself. */
	template <typename T>
	T shift() {
		const uint8_t *buf = queue.startData();
		T x = *(T*) buf;
		queue.startIncr(sizeof(T));
		return x;
	}

	/** Steps the state machine
	Returns true if step() should be called again
	*/
	bool step() {
		switch (currentCommand) {
			case NO_COMMAND: {
				if (queue.size() >= 1) {
					// Read command type
					uint8_t c = shift<uint8_t>();
					assert(c < NUM_COMMANDS);
					currentCommand = (BridgeCommand) c;
					return true;
				}
			} break;

			case START_COMMAND: {
				// To prevent other TCP protocols from connecting, require a "password" on startup to continue the connection.
				const int password = 0xff00fefd;
				if (queue.size() >= 4) {
					int p = shift<uint32_t>();
					if (p == password) {
						currentCommand = NO_COMMAND;
						return true;
					}
					else {
						closeRequested = true;
					}
				}
			} break;

			case QUIT_COMMAND: {
				closeRequested = true;
				currentCommand = NO_COMMAND;
				printf("Quitting!\n");
			} break;

			case CHANNEL_SET_COMMAND: {
				if (queue.size() >= 1) {
					channel = shift<uint8_t>();
					printf("Set channel %d\n", channel);
					currentCommand = NO_COMMAND;
					return true;
				}
			} break;

			case AUDIO_SAMPLE_RATE_SET_COMMAND: {
				if (queue.size() >= 4) {
					sampleRate = shift<uint32_t>();
					printf("Set sample rate %d\n", sampleRate);
					currentCommand = NO_COMMAND;
					return true;
				}
			} break;

			case AUDIO_CHANNELS_SET_COMMAND: {
				if (queue.size() >= 1) {
					audioChannels = shift<uint8_t>();
					printf("Set audio channels %d\n", channel);
					currentCommand = NO_COMMAND;
					return true;
				}
			} break;

			case AUDIO_BUFFER_SEND_COMMAND: {
				if (!audioBufferReceiving) {
					if (queue.size() >= 4) {
						audioBufferRemaining = shift<uint32_t>();
						audioBufferReceiving = true;
						return true;
					}
				}
				else {
					int available = queue.size() / sizeof(float);
					if (available > 0) {
						available = min(available, audioBufferRemaining);
						float *audioBuffer = (float*) queue.startData();
						// TODO Do something with the data
						fwrite(audioBuffer, sizeof(float), available, audioOutputFile);
						printf(".");
						queue.startIncr(available * sizeof(float));
						audioBufferRemaining -= available;
					}

					if (audioBufferRemaining <= 0) {
						audioBufferReceiving = false;
						currentCommand = NO_COMMAND;
						return true;
					}
				}
			} break;

			default: {
				// This shouldn't happen
				printf("Bad command\n");
				closeRequested = true;
			} break;
		}
		return false;
	}

	void handle() {
		printf("%s connected\n", getIp().c_str());

		while (1) {
			char buffer[RECV_BUFFER_SIZE];
#ifdef ARCH_MAC
			ssize_t received = recv(client, buffer, sizeof(buffer), 0);
#else
			ssize_t received = recv(client, buffer, sizeof(buffer), MSG_NOSIGNAL);
#endif
			if (received <= 0)
				return;

			// Make sure we can fill the buffer before filling it
			assert((ssize_t) queue.capacity() >= received);

			uint8_t *queueBuffer = queue.endData(received);
			memcpy(queueBuffer, buffer, received);
			queue.endIncr(received);

			// Loop the state machine until it returns false
			while (step()) {}

			if (closeRequested)
				return;

			// ssize_t written = send(client, buffer, received, MSG_NOSIGNAL);
			// if (written <= 0)
			// 	break;
		}
	}
};


void handleClient(int client) {
	int err;

	BridgeClientConnection connection;
	connection.client = client;
	connection.handle();

	printf("Closing client\n");
	err = close(client);
	assert(!err);
}


void doServer() {
	int err;

	// Open socket
	int server = socket(AF_INET, SOCK_STREAM, 0);
	assert(server >= 0);

	// Bind to 127.0.0.1 on port 5000
	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
	serverAddr.sin_port = htons(5000);
	err = bind(server, (struct sockaddr*) &serverAddr, sizeof(serverAddr));
	assert(!err);

	// Listen for clients
	err = listen(server, 20);
	assert(!err);
	printf("Server started\n");

	while (1) {
		// Accept client socket
		int client = accept(server, NULL, NULL);
		assert(client >= 0);

		// Launch client thread
		std::thread clientThread(handleClient, client);
		clientThread.detach();
	}

	// Cleanup
	err = close(server);
}


int main(int argc, char *argv[]) {
	doServer();
}