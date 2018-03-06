#include "client.cpp"



int main(int argc, char *argv[]) {
	Bridge bridge;
	bridge.setSampleRate(44100);
	for (int i = 0; i < 16; i++) {
		bridge.setChannel(i);
	}

	float input[2*100] = {};
	float output[2*100];
	for (int i = 0; i < 100; i++) {
		bridge.processAudio(input, output, 100);
	}
	std::this_thread::sleep_for(std::chrono::duration<double>(1.0));
}
