#include "client.cpp"



int main(int argc, char *argv[]) {
	Bridge bridge;
	bridge.setChannel(4);

	float input[2*100] = {};
	float output[2*100];
	bridge.processAudio(input, output, 100);
	std::this_thread::sleep_for(std::chrono::duration<double>(0.5));
}
