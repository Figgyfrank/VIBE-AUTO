DRIPMIX - Trap Mix Coach (VST3 for Ableton Live on Windows)
=============================================================

INSTALL (one time)
  1. Unzip this folder somewhere short, like C:\DripMix
  2. Double-click INSTALL.bat
     - If Windows says "Windows protected your PC": More info > Run anyway
     - If you don't have Microsoft's C++ build tools yet, it offers to install them (free, one-time)
     - First build downloads JUCE and takes a few minutes. Click YES on the admin prompt at the end.
  3. In Ableton: Options > Preferences (Ctrl+,) > Plug-ins
     turn ON "Use VST3 Plug-in System Folders" and hit Rescan
  4. Browser > Plug-ins > VST3 > DripLabs > DRIPMIX

HOW TO USE IT
  - Put DRIPMIX LAST in the device chain on every track you care about:
    Kick, 808, Snare/Clap, Hats, Melody, Lead Vocal, Ad-libs... and on the MASTER.
  - Every instance talks to every other one. Open ANY of them and you see the whole board.
  - Role: picked automatically from the track name ("808", "Kick", "Vox", "Hats"...).
    If it guesses wrong or says "???", pick the role in the dropdown at the top.
  - Name box: type a name if Ableton doesn't pass the track name through.
  - Play the loudest part of the song (the hook) and read the MIX COACH column.
  - Clip lights latch. Click one to reset it, or hit RESET CLIPS for all.
  - Drag the bottom-right corner to resize. The size is saved with your set.

WHAT THE NUMBERS MEAN
  MAX PEAK  highest sample since reset (over 0.0 = clipping)
  RMS       average level
  LUFS-S    perceived loudness over the last 3 seconds (used to compare tracks)
  PUNCH     peak-to-average in dB. Low = squashed, high = very dynamic
  SUB       how much of the track's energy is under ~120 Hz
  WIDTH     stereo correlation. 1.00 = mono, near 0 = very wide, below 0 = phase problems
  DRIP SCORE  100 minus points for each problem found on this track (on the Master: the whole mix)

GOOD TO KNOW
  - Ableton's track fader comes AFTER plugins, so DRIPMIX reads pre-fader on that track.
    To change what a track reads, use clip Gain or a Utility placed before DRIPMIX.
    Track faders DO change what the Master instance reads.
  - The coach uses trap starting points (mix peaks around -6 dBFS before mastering, vocal on top,
    kick and 808 hitting together, hats under the vocal, low end mono). They're guides - trust your ears.
  - DRIPMIX never changes your audio. It only listens.

REBUILD / UPDATE
  Run INSTALL.bat again (close Ableton first).
