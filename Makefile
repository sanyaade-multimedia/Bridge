VST2_SDK = VST2_SDK

FLAGS += -Wall -O3 -g -fPIC
FLAGS += -I$(VST2_SDK)
# Modification to compile the VST SDK with GCC
CXXFLAGS += -std=c++11

ifeq ($(ARCH), mac)
	MAC_SDK_FLAGS = -mmacosx-version-min=10.7
	FLAGS += -stdlib=libc++ -arch i386 -arch x86_64 $(MAC_SDK_FLAGS)
	LDFLAGS += -stdlib=libc++ $(MAC_SDK_FLAGS) -bundle
	TARGET = VCVBridge
	BUNDLE = VCVBridge.vst
endif
ifeq ($(ARCH), win)
	LDFLAGS += -shared
	TARGET = VCVBridge.dll
endif
ifeq ($(ARCH), lin)
	# gcc doesn't use this syntax
	FLAGS += -D__cdecl=""
	LDFLAGS += -shared
	TARGET = VCVBridge.so
endif


SOURCES += vstplugin.cpp
SOURCES += $(VST2_SDK)/public.sdk/source/vst2.x/audioeffect.cpp
SOURCES += $(VST2_SDK)/public.sdk/source/vst2.x/audioeffectx.cpp
SOURCES += $(VST2_SDK)/public.sdk/source/vst2.x/vstplugmain.cpp

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
	cp Info.plist $(BUNDLE)/Contents/
	cp PkgInfo $(BUNDLE)/Contents/
	cp $(TARGET) $(BUNDLE)/Contents/MacOS/
endif

install: dist
ifeq ($(ARCH), mac)
	sudo cp -R $(BUNDLE) /Library/Audio/Plug-Ins/VST/
endif

uninstall:
ifeq ($(ARCH), mac)
	sudo rm -rf /Library/Audio/Plug-Ins/VST/$(BUNDLE)
endif
