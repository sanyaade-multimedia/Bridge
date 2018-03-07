#include "client.cpp"



int main(int argc, char *argv[]) {
	BridgeClient client;
	client.setSampleRate(44100);
	for (int i = 0; i < 16; i++) {
		client.setChannel(i);
	}

	float input[2*100] = {};
	float output[2*100];
	for (int i = 0; i < 100; i++) {
		client.processAudio(input, output, 100);
		std::this_thread::sleep_for(std::chrono::duration<double>(1.0));
		printf(".\n");
	}
}
