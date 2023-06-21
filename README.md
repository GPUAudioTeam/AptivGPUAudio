# AptivGPUAudio

**Aptiv GPU Audio** is a project implementing three model examples of audio processing VST3 plugins using GPU platform. It uses [OpenCL](https://www.khronos.org/opencl/) library, enabling application acceleration by parallel computation using GPU units, combined with [AMD TrueAudioNext](https://gpuopen.com/true-audio-next/) which provides some methods for redirecting complex calculations to GPU (eg. FFT or signal convolution).

Aptiv GPU Audio project implements following audio effects:
+ **SpectrumPlugin**

    Graphical signal frequency content analyzer using the TAN FFT implementation (*TANCreateFFT()*). The plugin is equipped with an additional ability to control the size of the data frame taken for a single FFT operation (smaller or higher resolution of the displayed signal spectrum).

+ **ReverbPlugin**

     Reverb based on a convolution with the impulse response of several different acoustic scenes (IR type). Uses TAN kernel for the convolution calculation (*TANCreateConvolution()*) and a separate OpenCL kernel for calculating the ratio between the unprocessed (*Dry*) and processed (*Wet*) output signals. An additional option is to choose how data is transferred between the CPU and GPU (Data flow - *Push-pull* or *Pull-push*)

+ **Biquad5Plugin**

    Bandpass frequency filtering based on a set of five second-order IIR filters, realized by directly using the OpenCL interface. The filters have fixed parameters, except for adjustable gain.

## Installation

### Requirements

**1. JUCE**

Download the proper version of the JUCE framework from:
    
    https://juce.com/download/


**2. True Audio Next**

Clone the repository to AptivGPUAudio directory from:  

    https://github.com/GPUOpen-LibrariesAndSDKs/TAN.git

Follow the instructions for setup and installation from the document:  
***True Audio Next SDK Build Instructions.pdf***


**3. OpenCL SDK**

Install proper OpenCL SDK for your hardware.

For Intel, install:  

    https://www.intel.com/content/www/us/en/developer/tools/opencl-sdk/choose-download.html


*Note: if another SDK is installed, changing Library Search Paths in the Projucer files configuration will be needed.*

  
  
After building True Audio Next, copy the below libraries:


>TAN\tan\build\cmake\\**\{SELECTED_BUILD_TARGET\}**\\cmake-TAN-bin\Release\TrueAudioNext.lib -> TAN\tan\build\cmake\bin

>TAN\tan\build\cmake\\**\{SELECTED_BUILD_TARGET\}**\\cmake-amf-bin\Release\amf.lib -> TAN\tan\build\cmake\bin


### Building VST plugin:

1. Open Projucer file of chosen audio effect

2. Create a new export target if needed *(default: Visual Studio 2017)*

3. Build VST plugin in your IDE


## License

Refer to LICENSE.txt file in plugin directory for licensing information.