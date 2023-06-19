#include "FFTDataGenerator.h"

FFTDataGenerator::FFTDataGenerator(GPU_VSTAudioProcessor& audioProcessor)
    : mAudioProcessor(audioProcessor),
    mChannelsNo(audioProcessor.getChannelsNo())
{
    std::vector<float> data;
    Fifo<std::vector<float>> fifo;
    for (int i = 0; i < mChannelsNo; i++)
    {
        mFFTData.push_back(data);
        mFFTDataFifo.push_back(std::unique_ptr<Fifo<std::vector<float>>>(new Fifo<std::vector<float>>));
    }
}

FFTDataGenerator::~FFTDataGenerator()
{
}

void FFTDataGenerator::produceFFTDataForRendering(const juce::AudioBuffer<float>& audioData,
    const float negativeInfinity)
{
    cl_int ret;
    const auto fftSize = getFFTSize();
    static float mData[2 * MAX_FFT_SIZE];

	if (mAudioProcessor.isOpenCLDevice())
	{
		cl_command_queue command_queue = mAudioProcessor.getTANContext()->GetOpenCLGeneralQueue();
		cl_kernel& modulusKernel = mAudioProcessor.getModulusKernel();

		for (int i = 0; i < mChannelsNo; i++)
		{
			mFFTData[i].assign(mFFTData[i].size(), 0);
			auto* readIndex = audioData.getReadPointer(i);
			memset(mData, 0, sizeof(float) * 2 * fftSize);

			// convert to complex
			for (int j = 0; j < fftSize; j++)
			{
				mData[j * 2] = readIndex[j];
			}
			ret = clEnqueueWriteBuffer(command_queue, mAudioProcessor.clInBuff[i], CL_TRUE, 0, sizeof(float)*fftSize * 2, mData, 0, NULL, NULL);
		}

		amf::TANFFT *pTANFFT = mAudioProcessor.getTANFFT();
		AMF_RESULT res = pTANFFT->Transform(amf::TAN_FFT_TRANSFORM_DIRECTION_FORWARD,
			mFFTOrder,
			mChannelsNo,
			mAudioProcessor.clInBuff,
			mAudioProcessor.clOutBuff);
		juce::ignoreUnused(res);

		for (int i = 0; i < mChannelsNo; i++)
		{
			// calculate magnitudes with openCL kernel using FFT output buffer on GPU
			ret = clSetKernelArg(modulusKernel, 0, sizeof(cl_mem), (void *)&mAudioProcessor.clOutBuff[i]);
			ret = clSetKernelArg(modulusKernel, 1, sizeof(cl_mem), (void *)&mAudioProcessor.clInBuff[i]);
			size_t global_item_size = fftSize;
			ret = clEnqueueNDRangeKernel(command_queue, modulusKernel, 1, NULL, &global_item_size, NULL, 0, NULL, NULL);
			juce::ignoreUnused(ret);

			// prepare half of the magnitude spectrum for presentation
			std::vector<float> magnitude;

			ret = clEnqueueReadBuffer(command_queue, mAudioProcessor.clInBuff[i], CL_TRUE, 0, sizeof(float)*fftSize / 2, mData, 0, NULL, NULL);

			for (int j = 0; j < fftSize / 2; j++)
			{
				magnitude.push_back(mData[j]);
			}
			mFFTData[i] = magnitude;

			// normalize the FFT values and convert to dB
			int numBins = static_cast<int>(fftSize) / 2;
			for (int j = 0; j < numBins; j++)
			{
				auto v = mFFTData[i][j];
				if (!std::isinf(v) && !std::isnan(v))
				{
					v /= float(numBins);
				}
				else
				{
					v = 0.f;
				}
				mFFTData[i][j] = juce::Decibels::gainToDecibels(v, negativeInfinity);
			}

			mFFTDataFifo[i]->push(mFFTData[i]);
		}
	}
	else
	{
		for (int i = 0; i < mChannelsNo; i++)
		{
			mFFTData[i].assign(mFFTData[i].size(), 0);
			auto* readIndex = audioData.getReadPointer(i);
			memset(mData, 0, sizeof(float) * 2 * fftSize);

			// convert to complex
			for (int j = 0; j < fftSize; j++)
			{
				mData[j * 2] = readIndex[j];
			}
			float* pmData[] = {mData};

			amf::TANFFT *pTANFFT = mAudioProcessor.getTANFFT();
			AMF_RESULT res = pTANFFT->Transform(amf::TAN_FFT_TRANSFORM_DIRECTION_FORWARD,
				mFFTOrder,
				1,
				pmData,
				pmData);

			juce::ignoreUnused(res);

			// prepare half of the magnitude spectrum for presentation
			std::vector<float> magnitude;
		
			for (int j = 0; j < fftSize; j+=2)
			{
				magnitude.push_back(sqrtf(mData[j]*mData[j] + mData[j+1]*mData[j+1]));
			}
			mFFTData[i] = magnitude;

			// normalize the FFT values and convert to dB
			int numBins = static_cast<int>(fftSize) / 2;
			for (int j = 0; j < numBins; j++)
			{
				auto v = mFFTData[i][j];
				if (!std::isinf(v) && !std::isnan(v))
				{
					v /= float(numBins);
				}
				else
				{
					v = 0.f;
				}
				mFFTData[i][j] = juce::Decibels::gainToDecibels(v, negativeInfinity);
			}

			mFFTDataFifo[i]->push(mFFTData[i]);
		}
	}
}

void FFTDataGenerator::changeOrder(FFTOrder newOrder)
{
    mFFTOrder = newOrder;
    auto fftSize = getFFTSize();

    for (int i = 0; i < mChannelsNo; i++)
    {
        mFFTData[i].clear();
        mFFTData[i].resize(fftSize * 2, 0);
        mFFTDataFifo[i]->prepare(mFFTData[i].size());
    }
}

int FFTDataGenerator::getFFTSize() const
{
    return 1 << mFFTOrder;
}

int FFTDataGenerator::getNumAvailableFFTDataBlocks(int channel) const
{
    return mFFTDataFifo[channel]->getNumAvailableForReading();
}

bool FFTDataGenerator::getFFTData(int channel, std::vector<float>& fftData)
{
    return mFFTDataFifo[channel]->pull(fftData);
}
