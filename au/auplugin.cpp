#include "AUEffectBase.h"


class GainKernel : public AUKernelBase {
public:
	GainKernel(AUEffectBase *inAudioUnit) : AUKernelBase(inAudioUnit) {
	}

	virtual void Process(const Float32 *inSourceP, Float32 *inDestP, UInt32 inFramesToProcess, UInt32 inNumChannels, bool &ioSilence) override {
		if (ioSilence)
			return;
		for (int i = 0; i < inFramesToProcess; i++) {
			for (int c = 0; c < inNumChannels; c++) {
				float r = (float) rand() / RAND_MAX;
				r = 2.0 * r - 1.0;
				inDestP[i * inNumChannels + c] = r;
			}
		}
	}
};


class Gain : public AUEffectBase {
public:
	Gain(AudioUnit component) : AUEffectBase(component) {
		CreateElements();

		Globals()->UseIndexedParameters(1);
		SetParameter(0, 0.0);
	}

	AUKernelBase *NewKernel() override {
		return new GainKernel(this);
	}

	bool SupportsTail() override {
		return true;
	}

	ComponentResult GetParameterValueStrings(AudioUnitScope inScope, AudioUnitParameterID inParameterID, CFArrayRef *outStrings) override {
		if (!outStrings)
			return noErr;

		if (inScope == kAudioUnitScope_Global) {
			if (inParameterID == 0) {
				CFStringRef	strings [] = {
					CFSTR("0"), CFSTR("1"), CFSTR("2"), CFSTR("3"), CFSTR("4"), CFSTR("5"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"), CFSTR("0"),
				};

				*outStrings = CFArrayCreate(NULL, (const void**) strings, 64, NULL);
				return noErr;
			}
		}
		return kAudioUnitErr_InvalidParameter;
	}

	ComponentResult GetParameterInfo(AudioUnitScope inScope, AudioUnitParameterID inParameterID, AudioUnitParameterInfo &outParameterInfo) override {
		outParameterInfo.flags = kAudioUnitParameterFlag_IsWritable | kAudioUnitParameterFlag_IsReadable;

		if (inScope == kAudioUnitScope_Global) {
			switch (inParameterID) {
				case 0:
					AUBase::FillInParameterName(outParameterInfo, CFSTR("Channel"), false);
					outParameterInfo.unit = kAudioUnitParameterUnit_Indexed;
					outParameterInfo.minValue = 1.0;
					outParameterInfo.maxValue = 16.0;
					outParameterInfo.defaultValue = 0.0;
					return noErr;
				default:
					return kAudioUnitErr_InvalidParameter;
			}
		}
		return kAudioUnitErr_InvalidParameter;
	}
};



AUDIOCOMPONENT_ENTRY(AUBaseFactory, Gain)
