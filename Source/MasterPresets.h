/*
  ==============================================================================
    King Master - MasterPresets.h
    (c)(p) 2026 Tully EDM Vibe

    Starting points by genre. Loading one keeps what LEARN found about your
    song; King Master then re-checks the output level for the new settings.
  ==============================================================================
*/
#pragma once
#include "SynthParams.h"

namespace kmaster
{
    inline const std::vector<tsyn::Preset>& presets()
    {
        static const std::vector<tsyn::Preset> p = {
            { "Balanced Master", "Start Here", "Neutral, modern master at medium loudness. A great first try for any genre.",
              "style=Balanced loudness=Medium" },
            { "Gentle Polish", "Start Here", "Light touch: a little glue and tone match, streaming-friendly loudness.",
              "style=Balanced loudness=Low comp=0.2 warmth=0.1 smart_eq=0.4" },
            { "Streaming -14 LUFS", "Start Here", "Matches Spotify / YouTube / Apple loudness so nothing gets turned down.",
              "style=Balanced loudness=Low ceiling=-1" },

            { "EDM Festival", "EDM & Dance", "Loud, bright and wide for main-stage drops.",
              "style=Open loudness=High comp=0.55 warmth=0.35 width=1.15 eq_low=0.5 eq_air=1" },
            { "Big Room Loud", "EDM & Dance", "Maximum impact for big-room and electro. Pushes the limiter hard.",
              "style=Balanced loudness=High loud_trim=1 comp=0.6 warmth=0.5 ceiling=-0.5" },
            { "House Groove", "EDM & Dance", "Warm, round low end that keeps the groove bouncing.",
              "style=Warm loudness=Medium comp=0.45 warmth=0.3 eq_low=1" },
            { "Techno Punch", "EDM & Dance", "Tight, driving and loud: solid kick, controlled mids.",
              "style=Balanced loudness=High comp=0.6 warmth=0.4 eq_mid=-0.5" },
            { "Trance Anthem", "EDM & Dance", "Open and airy so supersaws and vocals shine.",
              "style=Open loudness=High comp=0.5 warmth=0.3 eq_air=1.5 width=1.2" },
            { "Dubstep & Bass", "EDM & Dance", "Heavy, saturated low end that still translates on small speakers.",
              "style=Warm loudness=High comp=0.65 warmth=0.5 eq_low=1.5 width=1.1" },
            { "Drum & Bass", "EDM & Dance", "Fast, crisp and forward with snappy transients.",
              "style=Open loudness=High comp=0.55 warmth=0.35 eq_pres=1" },
            { "Future Bass", "EDM & Dance", "Glossy, very wide and sparkly.",
              "style=Open loudness=Medium comp=0.45 width=1.25 eq_air=1.5" },

            { "Hip-Hop Heavy", "Hip-Hop & R&B", "Big 808s and a warm, punchy low end.",
              "style=Warm loudness=High comp=0.5 warmth=0.4 eq_low=1.5" },
            { "Trap 808", "Hip-Hop & R&B", "Loud trap master: deep 808, crisp hats, smooth vocals.",
              "style=Balanced loudness=High comp=0.5 warmth=0.35 eq_low=1 deess=0.35 width=1.1" },
            { "Boom Bap Vinyl", "Hip-Hop & R&B", "Dusty, warm and saturated like a vinyl pressing.",
              "style=Warm loudness=Medium comp=0.45 warmth=0.6 eq_air=-1" },
            { "R&B Smooth", "Hip-Hop & R&B", "Silky top, soft S's and a warm bottom.",
              "style=Warm loudness=Medium comp=0.4 warmth=0.3 deess=0.35 eq_air=0.5" },

            { "Pop Radio", "Pop & Acoustic", "Bright, polished and competitive with radio pop.",
              "style=Balanced loudness=High comp=0.5 warmth=0.3 eq_pres=1 eq_air=1" },
            { "Pop Streaming", "Pop & Acoustic", "Polished pop at streaming loudness: more dynamics, same shine.",
              "style=Balanced loudness=Low comp=0.4 eq_pres=0.5 eq_air=0.8" },
            { "Acoustic Natural", "Pop & Acoustic", "Transparent and dynamic for guitars, piano and voice.",
              "style=Warm loudness=Low comp=0.25 warmth=0.15 smart_eq=0.4" },
            { "Singer-Songwriter", "Pop & Acoustic", "Clear vocal focus with gentle de-essing.",
              "style=Balanced loudness=Low comp=0.3 deess=0.3 eq_pres=0.5" },

            { "Rock Punch", "Rock & Metal", "Driving rock: forward guitars, punchy drums.",
              "style=Balanced loudness=High comp=0.55 warmth=0.45 eq_mid=0.5 eq_pres=1" },
            { "Metal Wall", "Rock & Metal", "Dense, aggressive and loud, with a tight low end.",
              "style=Open loudness=High comp=0.7 warmth=0.4 eq_low=-0.5 eq_pres=1" },
            { "Indie Warm", "Rock & Metal", "Warm, tape-like and relaxed.",
              "style=Warm loudness=Medium comp=0.4 warmth=0.45" },

            { "Lo-Fi Chill", "Chill & Cinematic", "Soft, warm and dark for lo-fi beats.",
              "style=Warm loudness=Low comp=0.35 warmth=0.55 eq_air=-2 width=0.9" },
            { "Ambient Wide", "Chill & Cinematic", "Spacious and dynamic with a huge stereo image.",
              "style=Open loudness=Low comp=0.2 warmth=0.15 width=1.35" },
            { "Cinematic", "Chill & Cinematic", "Wide dynamic range and extra headroom for film and trailers.",
              "style=Balanced loudness=Low comp=0.25 width=1.2 ceiling=-1.5" },

            { "Club Master", "Utility", "Loud master for DJ sets and club systems.",
              "style=Balanced loudness=High loud_trim=1 comp=0.55 warmth=0.4 ceiling=-0.3" },
            { "Podcast Voice", "Utility", "Spoken word: even level, soft S's, -16 LUFS.",
              "style=Balanced loudness=Low loud_trim=-2 comp=0.5 deess=0.4 warmth=0.1" },
        };
        return p;
    }
}
