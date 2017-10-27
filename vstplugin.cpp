#include <stdio.h>
#include "public.sdk/source/vst2.x/audioeffectx.h"


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
			outputs[0][i] = inputs[0][i] * gain;
			outputs[1][i] = inputs[1][i] * gain;
		}
	}

	void setParameter(VstInt32 index, float value) override {
		if (index == 0) {
			gain = value;
		}
	}

	float getParameter(VstInt32 index) override {
		if (index == 0) {
			return gain;
		}
		return 0.0;
	}

	void getParameterLabel(VstInt32 index, char *label) override {
		if (index == 0) {
			snprintf(label, kVstMaxParamStrLen, "Label");
		}
	}

	void getParameterDisplay(VstInt32 index, char *text) override {
		if (index == 0) {
			snprintf(text, kVstMaxParamStrLen, "Display");
		}
	}

	void getParameterName(VstInt32 index, char *text) override {
		if (index == 0) {
			snprintf(text, kVstMaxParamStrLen, "Name");
		}
	}

	bool getEffectName(char* name) {
		snprintf(name, kVstMaxEffectNameLen, "EffectName");
		return true;
	}

	bool getVendorString(char* text) {
		snprintf(text, kVstMaxProductStrLen, "VendorString");
		return true;
	}

	bool getProductString(char* text) {
		snprintf(text, kVstMaxVendorStrLen, "ProductString");
		return true;
	}

	VstInt32 getVendorVersion() {
		return 0;
	}

private:
	float gain = 0.0;
};


AudioEffect *createEffectInstance (audioMasterCallback audioMaster) {
	return new Gain(audioMaster);
}
