#include <JuceHeader.h>

class LoopXFadeToolApplication : public juce::JUCEApplicationBase
{
public:
    LoopXFadeToolApplication() {}

    const juce::String getApplicationName() override { return "LoopXFade Tool"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        juce::StringArray args = juce::JUCEApplicationBase::getCommandLineParameterArray();

        if (args.size() != 4)
        {
            showUsage();
            quit();
            return;
        }

        juce::File inputFile(args[0]);
        int loopStart = args[1].getIntValue();
        int loopEnd = args[2].getIntValue();
        int crossfadeAmount = args[3].getIntValue();

        processFile(inputFile, loopStart, loopEnd, crossfadeAmount);
        quit();
    }

    void shutdown() override {}

    void anotherInstanceStarted (const juce::String& commandLine) override {}

    void systemRequestedQuit() override
    {
        quit();
    }

    void suspended() override {}

    void resumed() override {}

    void unhandledException(const std::exception*, const juce::String&, int) override
    {
        juce::Logger::writeToLog("Unhandled exception!");
        std::terminate();
    }

private:
    void showUsage()
    {
        juce::Logger::writeToLog("Usage: LoopXFadeTool <input_wav_file> <loop_start> <loop_end> <crossfade_amount>");
    }

    void processFile (const juce::File& inputFile, int loopStart, int loopEnd, int crossfadeAmount)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (inputFile));
        if (reader == nullptr)
        {
            juce::Logger::writeToLog ("Failed to open input file.");
            return;
        }

        int numChannels = reader->numChannels;
        int sampleRate = reader->sampleRate;

        // Create buffer to read the original file
        juce::AudioBuffer<float> buffer (numChannels, loopEnd + crossfadeAmount);
        reader->read (&buffer, 0, loopEnd + crossfadeAmount, 0, true, true);

        // Create output buffer with the correct size
        int outputLength = loopStart + (loopEnd - loopStart - crossfadeAmount) + crossfadeAmount;
        juce::AudioBuffer<float> outputBuffer (numChannels, outputLength);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            int index = 0;
            int chunk1Length = loopStart;
            // Copy the beginning of the file up to loopStart
            outputBuffer.copyFrom (channel, index, buffer, channel, 0, chunk1Length);
            index += chunk1Length;

            // Copy the portion of the loop before the crossfade starts
            int chunk2Length = loopEnd - loopStart - crossfadeAmount;
            outputBuffer.copyFrom (channel, index, buffer, channel, loopStart, chunk2Length);
            index += chunk2Length;

            // Perform the crossfade
            for (int i = 0; i < crossfadeAmount; ++i)
            {
                float fadeOut = std::cos ((float)i / crossfadeAmount * juce::MathConstants<float>::halfPi);
                float fadeIn = std::sin ((float)i / crossfadeAmount * juce::MathConstants<float>::halfPi);
                outputBuffer.setSample (channel, index + i,
                                        buffer.getSample (channel, index + i) * fadeOut +
                                        buffer.getSample (channel, (loopStart - crossfadeAmount) + i) * fadeIn);
            }
        }

        juce::File outputFile = inputFile.getSiblingFile (inputFile.getFileNameWithoutExtension() + "_XFade.wav");
        if(outputFile.existsAsFile()) {
            outputFile.deleteFile();
        }
        juce::WavAudioFormat wavFormat;
        juce::StringPairArray metadata;

        // Set Loop0Start and Loop0End in metadata
        metadata.set("Loop0Start", juce::String(loopStart));
        metadata.set("Loop0End", juce::String(loopEnd));
        metadata.set("Loop0Identifier",juce::String(0));
        metadata.set("Loop0Type", juce::String(0));
        metadata.set("NumSampleLoops", "1");

        std::unique_ptr<juce::FileOutputStream> fileStream (outputFile.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> writer (wavFormat.createWriterFor (fileStream.release(), sampleRate, numChannels, 16, metadata, 0));
        if (writer == nullptr)
        {
            juce::Logger::writeToLog ("Failed to create output file.");
            return;
        }

        writer->writeFromAudioSampleBuffer (outputBuffer, 0, outputBuffer.getNumSamples());
        juce::Logger::writeToLog ("Output file created: " + outputFile.getFullPathName());
    }
};

START_JUCE_APPLICATION (LoopXFadeToolApplication)
