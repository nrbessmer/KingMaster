# King Master
(c)(p) 2026 Tully EDM Vibe - info@tullyedmvibe.com - www.tullyedmvibe.com

Style-based mastering. Choose a STYLE (Warm, Balanced, Open) and a LOUDNESS (Low -14, Medium -11, High -8 LUFS),
play the loudest part of your song and King Master LEARNs: it measures the spectrum, band levels and loudness,
sets a Smart EQ tone match, 3-band glue compression and the gain, then checks its own output and corrects itself.

Chain: Smart EQ > style tilt + Low / Mid / Presence / Air > 3-band compression + de-ess > width + mono bass
> loudness gain > warmth soft-clip > true-peak lookahead limiter (ceiling in dBTP).
EBU R128 / BS.1770 meter (integrated, short-term, momentary, true peak) and a loudness-matched COMPARE.
27 genre presets. AU, VST3 and standalone app for macOS 12+ (Apple Silicon + Intel). Latency 5 ms (reported to the host).

Build: cmake -S . -B build -G Xcode && cmake --build build --config Release
Tests: cmake -S . -B build-test -DKM_TESTS=ON && cmake --build build-test --target KMTest
       KMTest mix_unmastered.wav outDir
