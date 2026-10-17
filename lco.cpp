#include "TriOsc.h"

using namespace juce;

static constexpr float kPi = MathConstants<float>::pi;
static constexpr float kTwoPi = MathConstants<float>::twoPi;

static float blep (float t, float dt)
{
    if (t < dt)         { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt)   { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

// ------------------------------------------------------------- parameters
AudioProcessorValueTreeState::ParameterLayout TriOscProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto F = [&] (String id, String name, float lo, float hi, float def, float skew = 1.f)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                     NormalisableRange<float> (lo, hi, 0.f, skew), def));
    };
    auto I = [&] (String id, String name, int lo, int hi, int def)
    {
        p.push_back (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def));
    };
    auto C = [&] (String id, String name, StringArray items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto B = [&] (String id, String name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    for (int i = 1; i <= 3; ++i)
    {
        String s = "o" + String (i) + "_";
        String n = "O" + String (i) + " ";
        B (s + "on", n + "On", i < 3);
        C (s + "wave", n + "Wave", { "Sine", "Triangle", "Saw", "Square", "Noise" }, i == 3 ? 3 : 2);
        I (s + "oct", n + "Octave", -3, 3, i == 3 ? -1 : 0);
        I (s + "semi", n + "Semi", -12, 12, 0);
        F (s + "fine", n + "Fine", -100.f, 100.f, i == 2 ? 7.f : 0.f);
        F (s + "level", n + "Level", 0.f, 1.f, i == 1 ? 0.8f : (i == 2 ? 0.6f : 0.5f));
        F (s + "pan", n + "Pan", -1.f, 1.f, 0.f);
        F (s + "pw", n + "PW", 0.05f, 0.95f, 0.5f);
        F (s + "phase", n + "Phase", 0.f, 1.f, 0.f);
        I (s + "uni", n + "Voices", 1, 7, 1);
        F (s + "det", n + "Detune", 0.f, 100.f, 15.f);
        F (s + "spread", n + "Spread", 0.f, 1.f, 0.5f);
    }

    F ("fm", "FM O2>O1", 0.f, 1.f, 0.f);
    F ("ring", "Ring Mod", 0.f, 1.f, 0.f);
    F ("drive", "Drive", 0.f, 1.f, 0.f);
    F ("glide", "Glide ms", 0.f, 1000.f, 0.f, 0.4f);
    F ("master", "Master", 0.f, 1.f, 0.7f);
    C ("ftype", "Filter", { "Lowpass", "Highpass", "Bandpass", "Off" }, 0);
    F ("cutoff", "Cutoff", 20.f, 20000.f, 20000.f, 0.3f);
    F ("res", "Resonance", 0.f, 1.f, 0.f);
    F ("fenv", "Filt Env Amt", -1.f, 1.f, 0.f);
    F ("keytrack", "Key Track", 0.f, 1.f, 0.f);

    F ("a_a", "Amp Attack", 0.0005f, 10.f, 0.005f, 0.3f);
    F ("a_d", "Amp Decay", 0.001f, 10.f, 0.3f, 0.3f);
    F ("a_s", "Amp Sustain", 0.f, 1.f, 0.7f);
    F ("a_r", "Amp Release", 0.001f, 10.f, 0.2f, 0.3f);
    F ("f_a", "Flt Attack", 0.0005f, 10.f, 0.005f, 0.3f);
    F ("f_d", "Flt Decay", 0.001f, 10.f, 0.3f, 0.3f);
    F ("f_s", "Flt Sustain", 0.f, 1.f, 0.f);
    F ("f_r", "Flt Release", 0.001f, 10.f, 0.2f, 0.3f);

    F ("lfo_rate", "LFO Rate", 0.05f, 30.f, 4.f, 0.4f);
    C ("lfo_shape", "LFO Shape", { "Sine", "Triangle", "Saw", "Square", "S&H" }, 0);
    F ("lfo_pitch", "LFO>Pitch", 0.f, 12.f, 0.f);
    F ("lfo_cut", "LFO>Cutoff", 0.f, 1.f, 0.f);
    F ("lfo_amp", "LFO>Amp", 0.f, 1.f, 0.f);

    return { p.begin(), p.end() };
}

TriOscProcessor::TriOscProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

void TriOscProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& v : voices) v = Voice();
}

void TriOscProcessor::readParams()
{
    auto g = [this] (const String& id) { return apvts.getRawParameterValue (id)->load(); };

    for (int i = 0; i < 3; ++i)
    {
        String s = "o" + String (i + 1) + "_";
        auto& o = P.osc[i];
        o.on = g (s + "on") > 0.5f;
        o.wave = (int) g (s + "wave");
        o.oct = (int) g (s + "oct");
        o.semi = (int) g (s + "semi");
        o.fine = g (s + "fine");
        o.level = g (s + "level");
        o.pan = g (s + "pan");
        o.pw = g (s + "pw");
        o.phase = g (s + "phase");
        o.uni = jlimit (1, 7, (int) g (s + "uni"));
        o.det = g (s + "det");
        o.spread = g (s + "spread");
    }
    P.fm = g ("fm");           P.ring = g ("ring");       P.drive = g ("drive");
    P.glideMs = g ("glide");   P.master = g ("master");   P.ftype = (int) g ("ftype");
    P.cutoff = g ("cutoff");   P.res = g ("res");         P.fenvAmt = g ("fenv");
    P.keytrack = g ("keytrack");
    P.aA = g ("a_a"); P.aD = g ("a_d"); P.aS = g ("a_s"); P.aR = g ("a_r");
    P.fA = g ("f_a"); P.fD = g ("f_d"); P.fS = g ("f_s"); P.fR = g ("f_r");
    P.lfoRate = g ("lfo_rate");   P.lfoShape = (int) g ("lfo_shape");
    P.lfoPitch = g ("lfo_pitch"); P.lfoCut = g ("lfo_cut"); P.lfoAmp = g ("lfo_amp");
}

// ------------------------------------------------------------------- midi
void TriOscProcessor::noteOn (int note, float vel)
{
    Voice* v = nullptr;
    for (auto& x : voices) if (! x.active) { v = &x; break; }
    if (v == nullptr)
    {
        v = &voices[0];
        for (auto& x : voices) if (x.age < v->age) v = &x;
    }

    v->active = true;
    v->gate = true;
    v->note = note;
    v->vel = vel;
    v->age = ++counter;
    v->curNote = (P.glideMs > 0.f && lastNote >= 0) ? (float) lastNote : (float) note;
    lastNote = note;

    for (int i = 0; i < 3; ++i)
        for (int u = 0; u < 7; ++u)
        {
            float ph = P.osc[i].phase + (P.osc[i].uni > 1 ? rng.nextFloat() : 0.f);
            v->ph[i][u] = ph - std::floor (ph);
        }

    for (int c = 0; c < 2; ++c) v->ic1[c] = v->ic2[c] = 0.f;
    v->aenv.trigger();
    v->fenv.trigger();
}

void TriOscProcessor::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && v.gate && v.note == note)
        {
            v.gate = false;
            v.aenv.release();
            v.fenv.release();
        }
}

void TriOscProcessor::handleMidi (const MidiMessage& m)
{
    if (m.isNoteOn())            noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())      noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices) { v.gate = false; v.aenv.release(); v.fenv.release(); }
    }
    else if (m.isPitchWheel())   bend = (m.getPitchWheelValue() - 8192) / 8192.f;
}

// -------------------------------------------------------------------- dsp
float TriOscProcessor::wave (int w, float t, float dt, float pw)
{
    switch (w)
    {
        case 0: return std::sin (kTwoPi * t);
        case 1: return 4.f * std::abs (t - 0.5f) - 1.f;
        case 2: return 2.f * t - 1.f - blep (t, dt);
        case 3:
        {
            float v = t < pw ? 1.f : -1.f;
            v += blep (t, dt);
            float t2 = t + 1.f - pw;
            t2 -= std::floor (t2);
            v -= blep (t2, dt);
            return v;
        }
        default: return rng.nextFloat() * 2.f - 1.f;
    }
}

void TriOscProcessor::processBlock (AudioBuffer<float>& buf, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buf.clear();
    readParams();

    const int ns = buf.getNumSamples();
    float* outL = buf.getWritePointer (0);
    float* outR = buf.getWritePointer (1);
    const float fsr = (float) sr;
    const float glideCoef = P.glideMs > 0.f ? 1.f - std::exp (-1.f / (P.glideMs * 0.001f * fsr)) : 1.f;
    const float drive = 1.f + P.drive * 8.f;
    const float maxCut = 0.45f * fsr;

    auto renderOsc = [&] (int i, Voice& v, float baseSemis, float fmOff, float& oL, float& oR) -> float
    {
        const auto& o = P.osc[i];
        oL = oR = 0.f;
        if (! o.on) return 0.f;

        const int uni = o.uni;
        const float norm = 1.f / std::sqrt ((float) uni);
        float mono = 0.f;

        for (int u = 0; u < uni; ++u)
        {
            const float pos = uni > 1 ? ((float) u / (float) (uni - 1)) * 2.f - 1.f : 0.f;
            const float semis = baseSemis + (float) (o.oct * 12 + o.semi) + o.fine * 0.01f + pos * o.det * 0.01f;
            const float freq = 440.f * std::exp2 ((semis - 69.f) / 12.f);
            const float dt = jlimit (1.0e-5f, 0.45f, freq / fsr);

            float t = v.ph[i][u] + fmOff;
            t -= std::floor (t);
            const float s = wave (o.wave, t, dt, o.pw);

            v.ph[i][u] += dt;
            if (v.ph[i][u] >= 1.f) v.ph[i][u] -= 1.f;

            const float pn = jlimit (-1.f, 1.f, o.pan + pos * o.spread);
            const float ang = (pn + 1.f) * 0.25f * kPi;
            oL += s * std::cos (ang) * 1.4142f * norm;
            oR += s * std::sin (ang) * 1.4142f * norm;
            mono += s * norm;
        }
        oL *= o.level;
        oR *= o.level;
        return mono;
    };

    auto it = midi.begin();

    for (int n = 0; n < ns; ++n)
    {
        while (it != midi.end() && (*it).samplePosition <= n)
        {
            handleMidi ((*it).getMessage());
            ++it;
        }

        // LFO
        lfoPhase += P.lfoRate / fsr;
        if (lfoPhase >= 1.f) { lfoPhase -= 1.f; sh = rng.nextFloat() * 2.f - 1.f; }
        float lfo;
        switch (P.lfoShape)
        {
            case 0:  lfo = std::sin (kTwoPi * lfoPhase); break;
            case 1:  lfo = 1.f - 4.f * std::abs (lfoPhase - 0.5f); break;
            case 2:  lfo = 2.f * lfoPhase - 1.f; break;
            case 3:  lfo = lfoPhase < 0.5f ? 1.f : -1.f; break;
            default: lfo = sh; break;
        }

        float accL = 0.f, accR = 0.f;

        for (auto& v : voices)
        {
            if (! v.active) continue;

            v.curNote += ((float) v.note - v.curNote) * glideCoef;
            const float baseSemis = v.curNote + bend * 2.f + lfo * P.lfoPitch;

            float ae = v.aenv.next (P.aA, P.aD, P.aS, P.aR, fsr);
            float fe = v.fenv.next (P.fA, P.fD, P.fS, P.fR, fsr);
            if (v.aenv.stage == Env::Idle) { v.active = false; continue; }

            ae *= 1.f - P.lfoAmp * (0.5f + 0.5f * lfo);

            float l0, r0, l1, r1, l2, r2;
            const float m1 = renderOsc (1, v, baseSemis, 0.f, l1, r1);
            const float m0 = renderOsc (0, v, baseSemis, P.fm * m1 * 2.f, l0, r0);
            renderOsc (2, v, baseSemis, 0.f, l2, r2);

            float xl = l0 + l1 + l2;
            float xr = r0 + r1 + r2;
            const float rm = P.ring * m0 * m1;
            xl += rm;
            xr += rm;

            if (P.ftype != 3)
            {
                float cut = P.cutoff * std::exp2 (P.fenvAmt * 5.f * fe
                                                  + P.lfoCut * 4.f * lfo
                                                  + P.keytrack * (v.curNote - 60.f) / 12.f);
                cut = jlimit (20.f, maxCut, cut);
                const float g = std::tan (kPi * cut / fsr);
                const float k = 2.f - 1.95f * P.res;
                const float a1 = 1.f / (1.f + g * (g + k));
                const float a2 = g * a1;
                const float a3 = g * a2;

                auto filt = [&] (float x, int ch)
                {
                    const float v3 = x - v.ic2[ch];
                    const float v1 = a1 * v.ic1[ch] + a2 * v3;
                    const float v2 = v.ic2[ch] + a2 * v.ic1[ch] + a3 * v3;
                    v.ic1[ch] = 2.f * v1 - v.ic1[ch];
                    v.ic2[ch] = 2.f * v2 - v.ic2[ch];
                    if (P.ftype == 0) return v2;
                    if (P.ftype == 1) return x - k * v1 - v2;
                    return v1 * k;
                };
                xl = filt (xl, 0);
                xr = filt (xr, 1);
            }

            const float amp = ae * v.vel;
            accL += xl * amp;
            accR += xr * amp;
        }

        outL[n] = std::tanh (accL * 0.3f * drive) * P.master;
        outR[n] = std::tanh (accR * 0.3f * drive) * P.master;
    }
    midi.clear();
}

// ------------------------------------------------------------------ state
void TriOscProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void TriOscProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* TriOscProcessor::createEditor() { return new TriOscEditor (*this); }

// ----------------------------------------------------------------- editor
static constexpr int kCellW = 78, kRowH = 100, kLeft = 74;

void TriOscEditor::addRow (const String& title, const std::vector<String>& ids)
{
    Row row;
    row.title = title;

    for (auto& id : ids)
    {
        auto* param = proc.apvts.getParameter (id);
        if (param == nullptr) continue;

        auto cell = std::make_unique<Cell>();

        if (auto* ch = dynamic_cast<AudioParameterChoice*> (param))
        {
            auto cb = std::make_unique<ComboBox>();
            cb->addItemList (ch->choices, 1);
            cell->att = std::make_shared<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *cb);
            cell->comp = std::move (cb);
        }
        else if (dynamic_cast<AudioParameterBool*> (param) != nullptr)
        {
            auto tb = std::make_unique<ToggleButton> ("On");
            cell->att = std::make_shared<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *tb);
            cell->comp = std::move (tb);
        }
        else
        {
            auto s = std::make_unique<Slider> (Slider::RotaryHorizontalVerticalDrag, Slider::TextBoxBelow);
            s->setTextBoxStyle (Slider::TextBoxBelow, false, 70, 16);
            cell->att = std::make_shared<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *s);
            cell->comp = std::move (s);
            cell->isSlider = true;
        }

        String name = param->getName (30);
        if (name.length() > 3 && name[0] == 'O' && CharacterFunctions::isDigit (name[1]) && name[2] == ' ')
            name = name.substring (3);

        cell->label = std::make_unique<Label> ("", name);
        cell->label->setJustificationType (Justification::centred);
        cell->label->setFont (Font (FontOptions (12.f)));

        addAndMakeVisible (*cell->comp);
        addAndMakeVisible (*cell->label);
        row.cells.push_back (std::move (cell));
    }
    rows.push_back (std::move (row));
}

TriOscEditor::TriOscEditor (TriOscProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    for (int i = 1; i <= 3; ++i)
    {
        String s = "o" + String (i) + "_";
        addRow ("OSC " + String (i), { s + "on", s + "wave", s + "oct", s + "semi", s + "fine", s + "level",
                                       s + "pan", s + "pw", s + "phase", s + "uni", s + "det", s + "spread" });
    }
    addRow ("MIX / FLT", { "fm", "ring", "drive", "glide", "master", "ftype", "cutoff", "res", "fenv", "keytrack" });
    addRow ("ENVELOPES", { "a_a", "a_d", "a_s", "a_r", "f_a", "f_d", "f_s", "f_r" });
    addRow ("LFO", { "lfo_shape", "lfo_rate", "lfo_pitch", "lfo_cut", "lfo_amp" });

    setSize (kLeft + 12 * kCellW + 10, (int) rows.size() * kRowH + 16);
}

void TriOscEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff1b1d22));
    g.setColour (Colour (0xff2a2d35));
    for (size_t r = 0; r < rows.size(); ++r)
        g.fillRoundedRectangle (4.f, (float) r * kRowH + 8.f, (float) getWidth() - 8.f, (float) kRowH - 4.f, 6.f);

    g.setColour (Colour (0xffffa94d));
    g.setFont (Font (FontOptions (14.f, Font::bold)));
    for (size_t r = 0; r < rows.size(); ++r)
        g.drawText (rows[r].title, 10, (int) r * kRowH + 8, kLeft - 10, kRowH - 4, Justification::centredLeft);
}

void TriOscEditor::resized()
{
    for (size_t r = 0; r < rows.size(); ++r)
        for (size_t c = 0; c < rows[r].cells.size(); ++c)
        {
            auto& cell = *rows[r].cells[c];
            const int x = kLeft + (int) c * kCellW;
            const int y = (int) r * kRowH + 10;

            cell.label->setBounds (x, y, kCellW, 14);
            if (cell.isSlider)
                cell.comp->setBounds (x, y + 14, kCellW, 78);
            else
                cell.comp->setBounds (x + 4, y + 30, kCellW - 8, 24);
        }
}
