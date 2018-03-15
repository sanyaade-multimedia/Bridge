#include "client.cpp"



int main(int argc, char *argv[]) {
	BridgeClient client;
	client.setSampleRate(44100);
	for (int i = 0; i < 16; i++) {
		client.setChannel(i);
	}

	const int n = 256;
	float input[2*n] = {};
	float output[2*n];
	for (int i = 0; i < n; i++) {
		float r = (float) rand() / RAND_MAX;
		r = 1.f - 2.f * r;
		r *= 0.01f;
		input[2*i+0] = r;
		input[2*i+1] = r;
	}
	while (1) {
		client.processAudio(input, output, n);
		std::this_thread::sleep_for(std::chrono::microseconds(100000 * n / 44100));
		fprintf(stderr, ".");
	}
}
