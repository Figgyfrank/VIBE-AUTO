#pragma once

#include "SharedBus.h"
#include "Analyzer.h"

class DripMixProcessor final : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    DripMixProcessor();
    ~DripMixProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DRIPMIX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void updateTrackProperties (const TrackProperties& properties) override;

    // ---- used by the editor ----
    int getSlotIndex() const noexcept { return slotIndex; }
    drip::Role getResolvedRole() const noexcept;
    juce::String getDisplayName() const;
    juce::String getCustomName() const;
    void setCustomName (const juce::String&);
    juce::String getHostTrackName() const;

    juce::AudioProcessorValueTreeState apvts;

    int savedEditorWidth = 1180, savedEditorHeight = 680;

private:
    void timerCallback() override;
    void publishIdentity();
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    int slotIndex = -1;
    drip::Analyzer analyzer;
    std::atomic<float>* roleParam = nullptr;

    mutable juce::CriticalSection nameLock;
    juce::String hostTrackName, customName;
    juce::uint32 hostColour = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DripMixProcessor)
};
