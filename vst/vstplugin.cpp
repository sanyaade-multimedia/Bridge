#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-override"
#include "public.sdk/source/vst2.x/audioeffectx.h"
#pragma GCC diagnostic pop


#include "../common/client.cpp"


class BridgeEffect : public AudioEffectX {
private:
	BridgeClient *client;

public:
	BridgeEffect(audioMasterCallback audioMaster) : AudioEffectX(audioMaster, 0, 1 + NUM_PARAMS) {
		setNumInputs(2);
		setNumOutputs(2);
		setUniqueID('VCVB');
		canProcessReplacing();
		client = new BridgeClient();
	}

	~BridgeEffect() {
		delete client;
	}

	void processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) override {
		// Interleave samples
		float input[2 * sampleFrames];
		float output[2 * sampleFrames];
		for (int i = 0; i < sampleFrames; i++) {
			input[2*i + 0] = inputs[0][i];
			input[2*i + 1] = inputs[1][i];
		}
		client->processAudio(input, output, sampleFrames);
		for (int i = 0; i < sampleFrames; i++) {
			// To prevent the DAW from pausing the processReplacing() calls, add a noise floor so the DAW thinks audio is still being processed.
			float r = (float) rand() / RAND_MAX;
			r = 1.f - 2.f * r;
			// Ableton Live's threshold is 1e-5 or -100dB
			r *= 1.5e-5f; // -96dB
			outputs[0][i] = output[2*i + 0] + r;
			outputs[1][i] = output[2*i + 1] + r;
		}
	}

	void setParameter(VstInt32 index, float value) override {
		if (index == 0) {
			client->setPort((int) roundf(value * 15.0));
		}
		else if (index > 0) {
			client->setParam(index - 1, value);
		}
	}

	float getParameter(VstInt32 index) override {
		if (index == 0) {
			return client->getPort() / 15.0;
		}
		else if (index > 0) {
			return client->getParam(index - 1);
		}
		return 0.f;
	}

	void getParameterLabel(VstInt32 index, char *label) override {
		if (index == 0) {
			snprintf(label, kVstMaxParamStrLen, "");
		}
		else if (index > 0) {
			snprintf(label, kVstMaxParamStrLen, "");
		}
	}

	void getParameterDisplay(VstInt32 index, char *text) override {
		if (index == 0) {
			snprintf(text, kVstMaxParamStrLen, "%d", client->getPort() + 1);
		}
		else if (index > 0) {
			snprintf(text, kVstMaxParamStrLen, "%0.2f V", 10.f * client->getParam(index - 1));
		}
	}

	void getParameterName(VstInt32 index, char *text) override {
		if (index == 0) {
			// Port selector
			snprintf(text, kVstMaxParamStrLen, "Port");
		}
		else if (index > 0) {
			// Automation parameters
			snprintf(text, kVstMaxParamStrLen, "#%d", index - 1);
		}
	}

	bool getEffectName(char* name) override {
		snprintf(name, kVstMaxEffectNameLen, "VCV Bridge");
		return true;
	}

	bool getVendorString(char* text) override {
		snprintf(text, kVstMaxProductStrLen, "VCV");
		return true;
	}

	bool getProductString(char* text) override {
		snprintf(text, kVstMaxVendorStrLen, "VCV Bridge");
		return true;
	}

	VstInt32 getVendorVersion() override {
		return 0;
	}

	void open() override {
		fprintf(stderr, "\n=============open============\n");
	}
	void close() override {
		fprintf(stderr, "\n=============close============\n");
	}
	void suspend() override {
		fprintf(stderr, "\n=============suspend============\n");
		client->setAudioActive(false);
	}
	void resume() override {
		fprintf(stderr, "\n=============resume============\n");
		client->setAudioActive(true);
	}
};


AudioEffect *createEffectInstance (audioMasterCallback audioMaster) {
	return new BridgeEffect(audioMaster);
}
