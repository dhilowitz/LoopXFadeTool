#include <JuceHeader.h>

class LoopXFadeToolApplication : public juce::JUCEApplicationBase
{
public:
    LoopXFadeToolApplication() {}

    const juce::String getApplicationName() override { return "LoopXFade Tool by David Hilowitz"; }
    const juce::String getApplicationVersion() override { return "0.2"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        juce::StringArray args = juce::JUCEApplicationBase::getCommandLineParameterArray();

        if (args.size() != 5)
        {
            showUsage();
            quit();
            return;
        }

        juce::File inputFile(args[0]);
        juce::File outputFile(args[1]);
        int loopStart = args[2].getIntValue();
        int loopEnd = args[3].getIntValue();
        int loopCrossfade = args[4].getIntValue();

        processFile(inputFile, outputFile, loopStart, loopEnd, loopCrossfade);
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
        juce::Logger::writeToLog("Usage: LoopXFadeTool <input_wav_file> <output_wav_file> <loop_start> <loop_end> <crossfade_amount>");
    }

    void processFile (const juce::File& inputFile, const juce::File& outputFile, int loopStart, int loopEnd, int loopCrossfade)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (inputFile));
        if (reader == nullptr)
        {
            juce::Logger::writeToLog ("ERROR: Failed to open input file: " + inputFile.getFullPathName());
            return;
        }

        int numChannels = reader->numChannels;
        int sampleRate = reader->sampleRate;
        int bitsPerSample = reader->bitsPerSample;
        int sourceFileLength = (int) reader->lengthInSamples;
        
        if(loopEnd > sourceFileLength) {
            juce::Logger::writeToLog ("ERROR: The specified loop end " + juce::String(loopEnd) + " is past the files endpoint of " + juce::String(sourceFileLength));
            return;
        }
        
        if(loopStart > loopEnd) {
            juce::Logger::writeToLog ("ERROR: The specified loop start " + juce::String(loopStart) + " is past the loop end " + juce::String(loopEnd));
            return;
        }
        
        if(loopStart < loopCrossfade) {
            juce::Logger::writeToLog ("ERROR: The specified loop start " + juce::String(loopStart) + " minus the loop crossfade " + juce::String(loopCrossfade) + " is a negative number. Given where the loop start point is, the largest possible crossfade is " + juce::String(juce::jmin(loopStart,loopEnd - loopStart)));
            return;
        }
        
        if(loopCrossfade > (loopEnd - loopStart)) {
            juce::Logger::writeToLog ("ERROR: The specified loop crossfade " + juce::String(loopCrossfade) + " is longer than the length of the loop region " + juce::String(loopEnd - loopStart) + ".");
            return;
        }

        // Create buffer to read the original file
        juce::AudioBuffer<float> buffer (numChannels, loopEnd + loopCrossfade);
        reader->read (&buffer, 0, juce::jmin(loopEnd + 1, sourceFileLength), 0, true, true);

        // Create output buffer with the correct size
        int outputLength = loopEnd + 1;
        
        juce::AudioBuffer<float> outputBuffer (numChannels, outputLength);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            int index = 0;
            int chunk1Length = loopStart;
            // Copy the beginning of the file up to loopStart
            outputBuffer.copyFrom (channel, index, buffer, channel, 0, chunk1Length);
            index += chunk1Length;
            
            // Copy the portion of the loop before the crossfade starts
            int chunk2Length = loopEnd - loopStart - loopCrossfade;
            outputBuffer.copyFrom (channel, index, buffer, channel, loopStart, chunk2Length);
            index += chunk2Length;
            
           // Perform the crossfade
           for (int i = 0; i <= loopCrossfade; ++i)
           {
               float fadeOut = std::cos ((float)i / loopCrossfade * juce::MathConstants<float>::halfPi);
               float fadeIn = std::sin ((float)i / loopCrossfade * juce::MathConstants<float>::halfPi);
               
               float sampleToFadeOut = buffer.getSample (channel, (loopEnd - loopCrossfade) + i);
               float sampleToFadeIn = buffer.getSample (channel, (loopStart - loopCrossfade) + i);
               
               outputBuffer.setSample (channel, index + i,
                                       sampleToFadeOut * fadeOut +
                                       sampleToFadeIn * fadeIn);
           }
        }

        if(outputFile.existsAsFile()) {
            juce::Logger::writeToLog ("Output file exists already.");
            return;
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
        std::unique_ptr<juce::AudioFormatWriter> writer (wavFormat.createWriterFor (fileStream.release(), sampleRate, numChannels, bitsPerSample, metadata, 0));
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
