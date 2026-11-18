#include "PluginProcessor.h"
#include "PluginEditor.h"

DripMixProcessor::DripMixProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DRIPMIX", createLayout())
{
    roleParam = apvts.getRawParameterValue ("role");
    slotIndex = drip::SharedBus::get().acquire();
    publishIdentity();
    startTimerHz (4);
}

DripMixProcessor::~DripMixProcessor()
{
    stopTimer();
    drip::SharedBus::get().release (slotIndex);
}

juce::AudioProcessorValueTreeState::ParameterLayout DripMixProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "role", 1 }, "Track Role",
                                                              drip::roleNames(), 0));
    return layout;
}

bool DripMixProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void DripMixProcessor::prepareToPlay (double sampleRate, int)
{
    analyzer.prepare (sampleRate);
}

void DripMixProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Pure meter: audio passes through untouched.
    const int numIn = getTotalNumInputChannels();
    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (auto* s = drip::SharedBus::get().slot (slotIndex))
        analyzer.process (buffer, juce::jmin (numIn, buffer.getNumChannels()), *s);
}

// ---------------------------------------------------------------------------
drip::Role DripMixProcessor::getResolvedRole() const noexcept
{
    const int chosen = roleParam != nullptr ? (int) roleParam->load() : 0;
    if (chosen > 0 && chosen < (int) drip::Role::NumRoles)
        return (drip::Role) chosen;
    return drip::guessRoleFromName (getDisplayName());
}

juce::String DripMixProcessor::getDisplayName() const
{
    const juce::ScopedLock sl (nameLock);
    return customName.isNotEmpty() ? customName : hostTrackName;
}

juce::String DripMixProcessor::getCustomName() const
{
    const juce::ScopedLock sl (nameLock);
    return customName;
}

juce::String DripMixProcessor::getHostTrackName() const
{
    const juce::ScopedLock sl (nameLock);
    return hostTrackName;
}

void DripMixProcessor::setCustomName (const juce::String& n)
{
    {
        const juce::ScopedLock sl (nameLock);
        customName = n.trim().substring (0, 40);
    }
    publishIdentity();
}

void DripMixProcessor::updateTrackProperties (const TrackProperties& properties)
{
    {
        const juce::ScopedLock sl (nameLock);
        if (properties.name.has_value())
            hostTrackName = *properties.name;
        if (properties.colourARGB.has_value())
            hostColour = *properties.colourARGB;
    }
    // the 4 Hz timer publishes the new name/colour to the shared bus
}

void DripMixProcessor::publishIdentity()
{
    if (auto* s = drip::SharedBus::get().slot (slotIndex))
    {
        s->setName (getDisplayName());
        s->role = (int) getResolvedRole();
        juce::uint32 c;
        {
            const juce::ScopedLock sl (nameLock);
            c = hostColour;
        }
        s->colourARGB = c;
    }
}

void DripMixProcessor::timerCallback()
{
    publishIdentity();
}

// ---------------------------------------------------------------------------
void DripMixProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("customName", getCustomName(), nullptr);
    state.setProperty ("uiW", savedEditorWidth, nullptr);
    state.setProperty ("uiH", savedEditorHeight, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DripMixProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.isValid() && tree.hasType (apvts.state.getType()))
        {
            {
                const juce::ScopedLock sl (nameLock);
                customName = tree.getProperty ("customName").toString();
            }
            savedEditorWidth  = juce::jlimit (760, 3000, (int) tree.getProperty ("uiW", savedEditorWidth));
            savedEditorHeight = juce::jlimit (440, 2000, (int) tree.getProperty ("uiH", savedEditorHeight));
            apvts.replaceState (tree);
            publishIdentity();
        }
    }
}

juce::AudioProcessorEditor* DripMixProcessor::createEditor()
{
    return new DripMixEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DripMixProcessor();
}
