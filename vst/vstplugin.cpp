#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "public.sdk/source/vst2.x/audioeffectx.h"


#include "../common/client.cpp"


class BridgeEffect : public AudioEffectX {
private:
	Bridge bridge;

public:
	BridgeEffect(audioMasterCallback audioMaster) : AudioEffectX(audioMaster, 0, 1 + NUM_PARAMS) {
		setNumInputs(2);
		setNumOutputs(2);
		setUniqueID('VCVB');
		canProcessReplacing();
	}

	~BridgeEffect() {}

	void processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) override {
		// Interleave samples
		float input[2 * sampleFrames];
		float output[2 * sampleFrames];
		for (int i = 0; i < sampleFrames; i++) {
			input[2*i + 0] = inputs[0][i];
			input[2*i + 1] = inputs[1][i];
		}
		bridge.processAudio(input, output, sampleFrames);
		for (int i = 0; i < sampleFrames; i++) {
			outputs[0][i] = output[2*i + 0];
			outputs[1][i] = output[2*i + 1];
		}
	}

	void setParameter(VstInt32 index, float value) override {
		if (index == 0) {
			bridge.setChannel((int) roundf(value * 15.0));
		}
		else if (index > 0) {
			bridge.setParam(index - 1, value);
		}
	}

	float getParameter(VstInt32 index) override {
		if (index == 0) {
			return bridge.getChannel() / 15.0;
		}
		else if (index > 0) {
			return bridge.getParam(index - 1);
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
			snprintf(text, kVstMaxParamStrLen, "%d", bridge.getChannel() + 1);
		}
		else if (index > 0) {
			snprintf(text, kVstMaxParamStrLen, "%0.3f V", 10.f * bridge.getParam(index - 1));
		}
	}

	void getParameterName(VstInt32 index, char *text) override {
		if (index == 0) {
			// Channel selector
			snprintf(text, kVstMaxParamStrLen, "Channel");
		}
		else if (index > 0) {
			// Automation parameters
			// One-indexed, but offset by 1
			snprintf(text, kVstMaxParamStrLen, "#%d", index - 1 + 1);
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
};


AudioEffect *createEffectInstance (audioMasterCallback audioMaster) {
	return new BridgeEffect(audioMaster);
}
