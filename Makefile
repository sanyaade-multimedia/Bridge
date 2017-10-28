ARCH = mac
TYPE = au

ifndef ARCH
	$(error ARCH not defined)
endif

ifndef TYPE
	$(error TYPE not defined)
endif

# Generic flags

FLAGS += -Wall -O3 -g -fPIC
CXXFLAGS += -std=c++11

# Plugin type flags

ifeq ($(TYPE), vst)
	VST2_SDK = VST2_SDK
	FLAGS += -I$(VST2_SDK)
endif

ifeq ($(TYPE), au)
	AU_SDK = AudioUnitExamplesAudioUnitEffectGeneratorInstrumentMIDIProcessorandOffline
endif

# Arch flags

ifeq ($(ARCH), mac)
	MAC_SDK_FLAGS = -mmacosx-version-min=10.7
	FLAGS += -stdlib=libc++ -arch i386 -arch x86_64 $(MAC_SDK_FLAGS)
ifeq ($(TYPE), vst)
	BUNDLE = VCVBridge.vst
endif
ifeq ($(TYPE), au)
	FLAGS += -I$(AU_SDK)/AUPublic/AUBase -I$(AU_SDK)/AUPublic/OtherBases -I$(AU_SDK)/AUPublic/Utility -I$(AU_SDK)/PublicUtility
	LDFLAGS += -framework CoreServices -framework AudioUnit -framework AudioToolbox
	BUNDLE = VCVBridge.component
endif
	LDFLAGS += -stdlib=libc++ $(MAC_SDK_FLAGS) -bundle
	TARGET = VCVBridge
endif
ifeq ($(ARCH), win)
	LDFLAGS += -shared
	TARGET = VCVBridge.dll
endif
ifeq ($(ARCH), lin)
	# Modification to compile the VST SDK with GCC
	FLAGS += -D__cdecl=""
	LDFLAGS += -shared
	TARGET = VCVBridge.so
endif

# Sources

ifeq ($(TYPE), vst)
	SOURCES += vstplugin.cpp
	SOURCES += $(VST2_SDK)/public.sdk/source/vst2.x/audioeffect.cpp
	SOURCES += $(VST2_SDK)/public.sdk/source/vst2.x/audioeffectx.cpp
	SOURCES += $(VST2_SDK)/public.sdk/source/vst2.x/vstplugmain.cpp
endif

ifeq ($(TYPE), au)
	SOURCES += auplugin.cpp
	SOURCES += $(AU_SDK)/AUPublic/OtherBases/AUEffectBase.cpp
	SOURCES += $(AU_SDK)/AUPublic/Utility/AUBaseHelper.cpp
	SOURCES += $(AU_SDK)/AUPublic/Utility/AUBuffer.cpp
	SOURCES += $(AU_SDK)/AUPublic/AUBase/AUBase.cpp
	SOURCES += $(AU_SDK)/AUPublic/AUBase/AUDispatch.cpp
	SOURCES += $(AU_SDK)/AUPublic/AUBase/AUInputElement.cpp
	SOURCES += $(AU_SDK)/AUPublic/AUBase/AUOutputElement.cpp
	SOURCES += $(AU_SDK)/AUPublic/AUBase/AUPlugInDispatch.cpp
	SOURCES += $(AU_SDK)/AUPublic/AUBase/AUScopeElement.cpp
	SOURCES += $(AU_SDK)/AUPublic/AUBase/ComponentBase.cpp
	SOURCES += $(AU_SDK)/PublicUtility/CAAudioChannelLayout.cpp
	SOURCES += $(AU_SDK)/PublicUtility/CABufferList.cpp
	SOURCES += $(AU_SDK)/PublicUtility/CAHostTimeBase.cpp
	SOURCES += $(AU_SDK)/PublicUtility/CAMutex.cpp
	SOURCES += $(AU_SDK)/PublicUtility/CAStreamBasicDescription.cpp
	SOURCES += $(AU_SDK)/PublicUtility/CAVectorUnit.cpp
endif

OBJECTS += $(patsubst %, %.o, $(SOURCES))


all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) $(CFLAGS) -c -o $@ $<

%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(FLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rfv $(OBJECTS) $(TARGET) $(BUNDLE)

dist: all
ifeq ($(ARCH), mac)
	mkdir -p $(BUNDLE)/Contents
	mkdir -p $(BUNDLE)/Contents/MacOS
ifeq ($(TYPE), vst)
	cp Info_VST.plist $(BUNDLE)/Contents/Info.plist
endif
ifeq ($(TYPE), au)
	cp Info_AU.plist $(BUNDLE)/Contents/Info.plist
endif
	cp PkgInfo $(BUNDLE)/Contents/
	cp $(TARGET) $(BUNDLE)/Contents/MacOS/
endif

install: dist
ifeq ($(ARCH), mac)
ifeq ($(TYPE), vst)
	sudo cp -R $(BUNDLE) /Library/Audio/Plug-Ins/VST/
endif
ifeq ($(TYPE), au)
	sudo cp -R $(BUNDLE) /Library/Audio/Plug-Ins/Components/
endif
endif

uninstall:
ifeq ($(ARCH), mac)
ifeq ($(TYPE), vst)
	sudo rm -rfv /Library/Audio/Plug-Ins/VST/$(BUNDLE)
endif
ifeq ($(TYPE), au)
	sudo rm -rfv /Library/Audio/Plug-Ins/Components/$(BUNDLE)
endif
endif
