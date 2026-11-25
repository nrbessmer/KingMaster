/*
  ==============================================================================
    Mister Master - StreamCheck.h
    (c)(p) 2026 Tully EDM Vibe

    Warns when the master, or the mix that arrives, is too loud for where it is
    going. Pure logic on measured numbers, so it is easy to test: the audio thread
    measures (MasterChain.h), the editor shows the report.

    The loudness references are the figures streaming services and mastering guides
    commonly publish (checked November 2026). Services change them, and listeners
    can switch normalisation off: a warning is advice, never a block.
  ==============================================================================
*/
#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>

namespace kmaster
{
    enum Platform { P_AllStreaming = 0, P_Spotify, P_AppleMusic, P_YouTube, P_Tidal, P_Amazon, P_Deezer, P_Podcast, P_Club, P_Count };
    inline juce::StringArray platformNames() { return { "All Streaming", "Spotify", "Apple Music", "YouTube", "TIDAL", "Amazon Music", "Deezer", "Podcast", "Club / DJ" }; }

    struct Service { float lufs, tp, tpWhenLoud; };
    inline Service service (int p)
    {
        switch (p)
        {
            case P_Spotify:    return { -14.0f, -1.0f, -2.0f };   // Spotify: under -2 dBTP when louder than -14 LUFS
            case P_AppleMusic: return { -16.0f, -1.0f, -1.0f };
            case P_YouTube:    return { -14.0f, -1.0f, -1.0f };
            case P_Tidal:      return { -14.0f, -1.0f, -1.0f };
            case P_Amazon:     return { -14.0f, -2.0f, -2.0f };
            case P_Deezer:     return { -15.0f, -1.0f, -1.0f };
            case P_Podcast:    return { -16.0f, -1.0f, -1.0f };
            case P_Club:       return { -8.0f, -0.1f, -0.1f };
            default:           return { -14.0f, -1.0f, -1.0f };
        }
    }

    // What to set so the master lands on the platform's reference (FIX IT, and the streaming presets).
    struct FixSettings { int loudness; float trim, ceiling; };
    inline FixSettings fixFor (int p)
    {
        switch (p)
        {
            case P_AppleMusic: case P_Podcast: return { 0, -2.0f, -1.0f };
            case P_Deezer:                     return { 0, -1.0f, -1.0f };
            case P_Amazon:                     return { 0, 0.0f, -2.0f };
            case P_Club:                       return { 2, 0.0f, -0.3f };
            default:                           return { 0, 0.0f, -1.0f };
        }
    }

    // Measured by the audio thread. LUFS are integrated (BS.1770); -100 = nothing yet.
    struct StreamStats
    {
        float outLufs = -100.0f, outTp = -100.0f, outSecs = 0.0f;   // the master
        float inLufs = -100.0f, inTp = -100.0f, inSecs = 0.0f;      // the mix that arrives
        int   inClips = 0;                                          // runs of samples stuck at full scale
        bool  learning = false;
    };

    struct StreamReport
    {
        enum Level { Off = 0, Waiting, Ok, Note, Warn, Bad };
        struct Chip { juce::String name; float turnDown = 0.0f; bool over = false; };
        Level level = Off;
        juce::String headline, detail;
        std::vector<Chip> chips;       // how much each service would turn this master down
        float turnDown = 0.0f;         // dB, for the chosen platform
        bool tooLoud = false, tpOver = false, mixTooHot = false, mixClips = false, canFix = false;
        static juce::String name (Level l) { const char* n[] = { "off", "waiting", "ok", "note", "warn", "bad" }; return n[l]; }
    };

    constexpr float kCheckSeconds = 5.0f;      // music needed before a verdict
    constexpr float kMixHot = -11.0f;          // a mix this loud before mastering has been limited already
    constexpr float kMixCrushed = -8.5f;

    inline StreamReport checkStreaming (bool enabled, int platform, const StreamStats& s)
    {
        using R = StreamReport; R r;
        if (! enabled) return r;
        auto db = [] (float v) { return juce::String (std::abs (v) < 0.05f ? 0.0f : v, 1); };
        platform = juce::jlimit (0, (int) P_Count - 1, platform);
        const auto sv = service (platform);
        const juce::String where = platform == P_AllStreaming ? juce::String ("STREAMING") : platform == P_Podcast ? juce::String ("PODCASTS")
                                 : platform == P_Club ? juce::String ("CLUB") : platformNames()[platform].toUpperCase();
        auto raise = [&r] (R::Level l) { if ((int) l > (int) r.level) r.level = l; };
        juce::StringArray more;

        // ---- the mix that arrives (checked all the time, before any mastering)
        juce::String mixText;
        if (s.inClips >= 3) { r.mixClips = true; mixText = "YOUR MIX CLIPS BEFORE MASTERING"; }
        if (s.inSecs >= kCheckSeconds && s.inLufs > kMixHot)
        {
            r.mixTooHot = true;
            if (mixText.isEmpty()) mixText = s.inLufs > kMixCrushed ? "YOUR MIX IS FAR TOO LOUD BEFORE MASTERING" : "YOUR MIX IS TOO LOUD BEFORE MASTERING";
        }
        const juce::String mixHelp = r.mixClips && ! r.mixTooHot ? "Mix peaks hit full scale: lower the mix fader until it stops clipping, then LEARN again."
                                   : "The mix arrives at " + db (s.inLufs) + " LUFS" + (r.mixClips ? " and clips" : "")
                                     + ": take the limiter off the mix bus or lower it, then LEARN again. Lost dynamics can't be put back.";

        // ---- the master
        const bool haveOut = s.outSecs >= kCheckSeconds && s.outLufs > -70.0f;
        if (! haveOut)
        {
            r.level = R::Waiting;
            r.headline = s.learning ? "LEARNING: the streaming check starts when it is done" : "Play your song: checking its loudness for " + where.toLowerCase() + "...";
            if (platform == P_AllStreaming || (platform >= P_Spotify && platform <= P_Deezer)) r.headline = s.learning ? r.headline : juce::String ("Play your song: checking its loudness for streaming...");
        }
        else
        {
            const float d = s.outLufs - sv.lufs;
            r.level = R::Ok;
            if (platform == P_Club)
            {
                r.headline = "CLUB READY  " + db (s.outLufs) + " LUFS";
                if (s.outLufs > -5.5f) { r.tooLoud = true; raise (R::Warn); r.headline = "TOO LOUD EVEN FOR A CLUB MASTER: " + db (s.outLufs) + " LUFS"; }
                else if (s.outLufs < -10.5f) { raise (R::Note); r.headline = "QUIETER THAN MOST CLUB MASTERS: " + db (s.outLufs) + " LUFS"; }
                if (s.outLufs > -13.0f) more.add ("Streaming would turn it down " + db (s.outLufs + 14.0f) + " dB (choose All Streaming to check)");
            }
            else
            {
                r.turnDown = juce::jmax (0.0f, d);
                if (d > 1.0f)
                {
                    r.tooLoud = true; raise (d > 3.0f ? R::Bad : R::Warn);
                    r.headline = (d > 3.0f ? "TOO LOUD FOR " : "A LITTLE TOO LOUD FOR ") + where + ": it will be turned down " + db (d) + " dB";
                    more.add (db (s.outLufs) + " LUFS, reference " + db (sv.lufs));
                }
                else if (d < -3.0f) { raise (R::Note); r.headline = "QUIETER THAN " + where + " BY " + db (-d) + " dB"; more.add ("Not a fault: it keeps its dynamics, and some services turn it up"); }
                else { r.headline = "READY FOR " + where + "  " + db (s.outLufs) + " LUFS, " + db (s.outTp) + " dBTP"; }
                for (int p = P_Spotify; p <= P_Deezer; ++p)
                    if (platform == P_AllStreaming || platform == p)
                    {
                        const float td = s.outLufs - service (p).lufs;
                        r.chips.push_back ({ platformNames()[p], juce::jmax (0.0f, td), td > 1.0f });
                    }
            }
            const float limit = d > 0.5f ? sv.tpWhenLoud : sv.tp;
            if (s.outTp > limit + 0.1f)
            {
                r.tpOver = true; raise (s.outTp > 0.0f ? R::Bad : R::Warn);
                const juce::String t = "True peak " + db (s.outTp) + " dBTP is over " + db (limit) + ": encoding can distort it";
                if (r.tooLoud) more.add (t); else { r.headline = "TRUE PEAK TOO HIGH FOR " + where + ": " + db (s.outTp) + " dBTP (limit " + db (limit) + ")"; more.add ("Lower the CEILING: lossy encoding can clip peaks this high"); }
            }
            r.canFix = r.tooLoud || r.tpOver;
        }

        // a hot mix outranks everything but a worse master warning
        if (mixText.isNotEmpty())
        {
            const R::Level ml = r.mixClips || s.inLufs > kMixCrushed ? R::Bad : R::Warn;
            if ((int) ml > (int) r.level || (r.level == R::Waiting)) { if (haveOut && ! r.tooLoud && ! r.tpOver) more.insert (0, "Master " + db (s.outLufs) + " LUFS"); r.headline = mixText; more.insert (0, mixHelp); }
            else more.add (mixHelp);
            raise (ml); if (r.level == R::Waiting) r.level = ml;
        }
        for (auto& m : more) m = m.trimCharactersAtEnd (".");
        r.detail = more.joinIntoString (".  ");
        return r;
    }

    // ---- where in the song it is too loud: spans of a loudness curve above a limit
    constexpr float kGraphSlot = 0.25f;          // seconds per point
    constexpr float kNoData = -200.0f;
    struct LoudSpan { float start = 0, end = 0, peak = -100.0f; };
    inline std::vector<LoudSpan> loudSpans (const std::vector<float>& v, float limit, float minLen = 1.0f, float bridge = 1.5f)
    {
        std::vector<LoudSpan> out; const float s = kGraphSlot;
        for (size_t i = 0; i < v.size(); ++i)
        {
            if (v[i] <= kNoData + 1.0f || v[i] <= limit) continue;
            const float t0 = (float) i * s, t1 = t0 + s;
            if (! out.empty() && t0 - out.back().end <= bridge) { out.back().end = t1; out.back().peak = juce::jmax (out.back().peak, v[i]); }
            else out.push_back ({ t0, t1, v[i] });
        }
        std::vector<LoudSpan> keep; for (auto& x : out) if (x.end - x.start >= minLen) keep.push_back (x);
        return keep;
    }
    inline juce::String clock (float secs) { const int t = juce::jmax (0, (int) secs); return juce::String (t / 60) + ":" + juce::String (t % 60).paddedLeft ('0', 2); }
    // the limit line for the master and for the mix that arrives
    inline float masterLimit (int platform) { return service (juce::jlimit (0, (int) P_Count - 1, platform)).lufs + (platform == P_Club ? 2.5f : 1.0f); }
    inline float referenceLine (int platform) { return service (juce::jlimit (0, (int) P_Count - 1, platform)).lufs; }

    // "0:31-1:04 (bars 17-33), 1:36-2:10", or "throughout"
    inline juce::String describeSpans (const std::vector<LoudSpan>& spans, const std::vector<float>& v, const std::vector<float>& bars, int maxSpans = 3)
    {
        if (spans.empty()) return {};
        float have = 0, over = 0; for (auto x : v) if (x > kNoData + 1.0f) have += kGraphSlot;
        for (auto& x : spans) over += x.end - x.start;
        if (have > 6.0f && over > 0.92f * have) return "throughout";
        auto bar = [&] (float t) { const size_t i = (size_t) juce::jlimit (0, juce::jmax (0, (int) bars.size() - 1), (int) (t / kGraphSlot)); return i < bars.size() ? (int) bars[i] : 0; };
        std::vector<LoudSpan> s = spans;
        if ((int) s.size() > maxSpans) { std::sort (s.begin(), s.end(), [] (auto& a, auto& b) { return a.end - a.start > b.end - b.start; }); s.resize ((size_t) maxSpans); std::sort (s.begin(), s.end(), [] (auto& a, auto& b) { return a.start < b.start; }); }
        juce::StringArray t;
        for (auto& x : s)
        {
            const int b0 = bar (x.start), b1 = bar (x.end - kGraphSlot);
            t.add (clock (x.start) + "-" + clock (x.end) + (b0 > 0 && b1 > 0 ? (b0 == b1 ? " (bar " + juce::String (b0) + ")" : " (bars " + juce::String (b0) + "-" + juce::String (b1) + ")") : juce::String()));
        }
        return t.joinIntoString (", ") + (spans.size() > s.size() ? " +" + juce::String ((int) (spans.size() - s.size())) + " more" : "");
    }
}
