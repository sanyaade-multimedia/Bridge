# VCV Bridge


## Building

Clone this repository in the root directory of the [Rack](https://github.com/VCVRack/Rack) repository. Or, clone this repository anywhere and set the `RACK_DIR` environment variable.

For the VST plugin, obtain `VST2_SDK` from Steinberg, and place it in the `vst/` directory.

For the AU plugin, obtain `AudioUnitExamplesAudioUnitEffectGeneratorInstrumentMIDIProcessorandOffline` from Apple, and place it in the `au/` directory.

Run `make dist` in each desired directory. The plugins are placed in `dist/`.

## License

Source code licensed under [BSD-3-Clause](LICENSE.txt) by [Andrew Belt](https://andrewbelt.name/)
