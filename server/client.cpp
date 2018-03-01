#include "bridge.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/time.h>


using namespace rack;


double getTime() {
	struct timeval now;
	gettimeofday(&now, NULL);
	return now.tv_sec + 1e-6 * now.tv_usec;
}


struct BridgeServerConnection {
	int server;
	bool closeRequested = false;

	void send(const uint8_t *buffer, int length) {
		ssize_t written = ::send(server, buffer, length, MSG_NOSIGNAL);
		if (written <= 0)
			closeRequested = true;
	}

	template <typename T>
	void send(T x) {
		send((uint8_t*) &x, sizeof(x));
	}

	template <typename T>
	void sendBuffer(const T *p, int length) {
		send((uint8_t*) p, length * sizeof(*p));
	}

	void flush() {
		int err;
		// Turn off Nagle
		int flag = 1;
		err = setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
		assert(!err);
		// Turn on Nagle
		flag = 0;
		err = setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
		assert(!err);
	}
};


void testCommands(BridgeServerConnection &connection) {
	const int password = 0xff00fefd;
	connection.send<uint32_t>(password);

	connection.send<uint8_t>(CHANNEL_SET_COMMAND);
	connection.send<uint8_t>(15);

	connection.send<uint8_t>(AUDIO_SAMPLE_RATE_SET_COMMAND);
	connection.send<uint32_t>(44100);

	int n = 256*2;
	float samples[n];
	for (int i = 0; i < n; i++) {
		samples[i] = powf(2.f, i);
	}
	for (int i = 0; i < 1000000; i++) {
		connection.send<uint8_t>(AUDIO_BUFFER_SEND_COMMAND);
		connection.send<uint32_t>(n);
		connection.sendBuffer<float>(samples, n);
		connection.flush();
	}

	connection.send<uint8_t>(QUIT_COMMAND);
}


void doClient() {
	int err;

	// Open socket
	int server = socket(AF_INET, SOCK_STREAM, 0);
	assert(server >= 0);

	// Connect to 127.0.0.1 on port 5000
	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
	serverAddr.sin_port = htons(5000);
	err = connect(server, (struct sockaddr*) &serverAddr, sizeof(serverAddr));
	if (err) {
		printf("Could not connect to server\n");
		return;
	}

	BridgeServerConnection connection;
	connection.server = server;
	testCommands(connection);

	err = close(server);
}


int main(int argc, char *argv[]) {
	doClient();
}
