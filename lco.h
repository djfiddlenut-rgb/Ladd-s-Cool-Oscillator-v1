#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <memory>
#include <cmath>

// ---------------------------------------------------------------- envelope
struct Env
{
    enum Stage { Idle, Att, Dec, Sus, Rel } stage = Idle;
    float level = 0.f;

    void trigger() { stage = Att; }
    void release() { if (stage != Idle) stage = Rel; }

    float next (float a, float d, float s, float r, float sr)
    {
        switch (stage)
        {
            case Att:
                level += 1.f / (juce::jmax (a, 0.0005f) * sr);
                if (level >= 1.f) { level = 1.f; stage = Dec; }
                break;
            case Dec:
                level += (s - level) * (1.f - std::exp (-4.f / (juce::jmax (d, 0.001f) * sr)));
                if (level <= s + 0.0005f) stage = Sus;
                break;
            case Sus:
                level = s;
                break;
            case Rel:
                level -= level * (1.f - std::exp (-4.f / (juce::jmax (r, 0.001f) * sr)));
                if (level < 0.0003f) { level = 0.f; stage = Idle; }
                break;
            default: break;
        }
        return level;
    }
};

// ------------------------------------------------------------------ voice
struct Voice
{
    bool active = false, gate = false;
    int note = 0;
    float vel = 0.f, curNote = 60.f;
    uint32_t age = 0;
    float ph[3][7] {};
    Env aenv, fenv;
    float ic1[2] {}, ic2[2] {};
};

// ------------------------------------------------------------- parameters
struct OscP
{
    bool on = true;
    int wave = 2, oct = 0, semi = 0, uni = 1;
    float fine = 0, level = 1, pan = 0, pw = 0.5f, phase = 0, det = 0, spread = 0;
};

struct Params
{
    OscP osc[3];
    float fm = 0, ring = 0, drive = 0, glideMs = 0, master = 0.7f;
    float cutoff = 20000.f, res = 0, fenvAmt = 0, keytrack = 0;
    int ftype = 0;
    float aA = 0.005f, aD = 0.3f, aS = 0.7f, aR = 0.2f;
    float fA = 0.005f, fD = 0.3f, fS = 0.f, fR = 0.2f;
    float lfoRate = 4.f, lfoPitch = 0, lfoCut = 0, lfoAmp = 0;
    int lfoShape = 0;
};

// -------------------------------------------------------------- processor
class TriOscProcessor : public juce::AudioProcessor
{
public:
    TriOscProcessor();

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Ladds Cool Oscillator"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int size) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void readParams();
    void handleMidi (const juce::MidiMessage& m);
    void noteOn (int note, float vel);
    void noteOff (int note);
    float wave (int w, float t, float dt, float pw);

    Params P;
    Voice voices[16];
    double sr = 44100.0;
    float lfoPhase = 0.f, sh = 0.f, bend = 0.f;
    int lastNote = -1;
    uint32_t counter = 0;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriOscProcessor)
};

// ----------------------------------------------------------------- editor
class TriOscEditor : public juce::AudioProcessorEditor
{
public:
    explicit TriOscEditor (TriOscProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Cell
    {
        std::unique_ptr<juce::Component> comp;
        std::unique_ptr<juce::Label> label;
        std::shared_ptr<void> att; // destroyed first
        bool isSlider = false;
    };
    struct Row
    {
        juce::String title;
        std::vector<std::unique_ptr<Cell>> cells;
    };

    void addRow (const juce::String& title, const std::vector<juce::String>& ids);

    TriOscProcessor& proc;
    std::vector<Row> rows;
};
