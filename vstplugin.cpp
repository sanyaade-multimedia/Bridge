#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "public.sdk/source/vst2.x/audioeffectx.h"


struct Bridge {
	int channel = 0;
	void setChannel(int _channel) {
		channel = _channel;
	}
	int getChannel() {
		return channel;
	}
};


class Gain : public AudioEffectX {
public:
	Gain(audioMasterCallback audioMaster) : AudioEffectX(audioMaster, 0, 1) {
		setNumInputs(2);
		setNumOutputs(2);
		setUniqueID('VCVB');
		canProcessReplacing();
	}

	~Gain() {}

	void processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) override {
		for (int i = 0; i < sampleFrames; i++) {
			float r = (float) rand() / RAND_MAX;
			r = 2.0 * r - 1.0;
			outputs[0][i] = r; //inputs[0][i];
			outputs[1][i] = r; //inputs[1][i];
		}
	}

	void setParameter(VstInt32 index, float value) override {
		if (index == 0) {
			bridge.setChannel((int) roundf(value * 63.0));
		}
	}

	float getParameter(VstInt32 index) override {
		if (index == 0) {
			return bridge.getChannel() / 63.0;
		}
		return 0.0;
	}

	void getParameterLabel(VstInt32 index, char *label) override {
		if (index == 0) {
			snprintf(label, kVstMaxParamStrLen, "");
		}
	}

	void getParameterDisplay(VstInt32 index, char *text) override {
		if (index == 0) {
			snprintf(text, kVstMaxParamStrLen, "%d", bridge.getChannel());
		}
	}

	void getParameterName(VstInt32 index, char *text) override {
		if (index == 0) {
			snprintf(text, kVstMaxParamStrLen, "Channel");
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
		snprintf(text, kVstMaxVendorStrLen, "ProductString");
		return true;
	}

	VstInt32 getVendorVersion() override {
		return 0;
	}

private:
	Bridge bridge;
};


AudioEffect *createEffectInstance (audioMasterCallback audioMaster) {
	return new Gain(audioMaster);
}
