FLAGS += -Wall -O3 -g -fPIC
FLAGS += -I$(HOME)/pkg/VST_SDK/VST2_SDK
# Modification to compile the VST SDK with GCC
FLAGS += -D__cdecl=""
# FLAGS += -S -save-temps
CXXFLAGS += $(FLAGS) -std=c++11
LDFLAGS += -shared


SOURCES += vstplugin.cpp
SOURCES += $(HOME)/pkg/VST_SDK/VST2_SDK/public.sdk/source/vst2.x/audioeffect.cpp
SOURCES += $(HOME)/pkg/VST_SDK/VST2_SDK/public.sdk/source/vst2.x/audioeffectx.cpp
SOURCES += $(HOME)/pkg/VST_SDK/VST2_SDK/public.sdk/source/vst2.x/vstplugmain.cpp

OBJECTS += $(patsubst %, %.o, $(SOURCES))


vstplugin.so: $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) $(CFLAGS) -c -o $@ $<

%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(FLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rfv *.o
