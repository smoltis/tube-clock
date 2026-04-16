<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2020 Stan -->

# Changelog

## 2026-04-16

### Controls: button handling rework
- Replaced `MD_UISwitch` with `AceButton` for more reliable button events and debounce handling.
- Added configurable button timing constants for debounce, long-press, and repeat behavior.
- Implemented centralized button event handler:
  - `Pressed` -> standard control action (`SET`, `UP`, `DOWN`)
  - `RepeatPressed` -> hold-to-repeat on `UP`/`DOWN`
  - `LongPressed` on `SET` -> jump to `SAVE`
- Switched button wiring logic to `INPUT_PULLUP` initialization.
- Moved button polling to the top of `loop()` for better responsiveness.

### Save flow: responsiveness improvement
- Refactored `saveSettings()` from blocking delay-based flow to non-blocking millis-based flow.
- Removed `delay(1000)` in save state to avoid input stalls while saving.

### Alarm: buzzer melody feature
- Added passive buzzer support on `BUZZER_PIN` (`12`) using Arduino `tone()`/`noTone()`.
- Added short looping alarm melody (custom note + duration arrays).
- Implemented non-blocking melody engine:
  - `startAlarmMelody()`
  - `updateAlarmMelody()`
  - `stopAlarmMelody()`
- Integrated melody with alarm lifecycle:
  - Starts when alarm becomes active
  - Stops when alarm is dismissed/stopped
- Added buzzer init in `setup()` and melody update call in `loop()`.

### Time-setting accuracy fix
- Fixed time drift when adjusting settings for extended periods before pressing save.
- Added time edit session tracking:
  - session start timestamp (`millis`) and RTC start second
  - session active flag
- Save now compensates elapsed edit duration by applying:
  - edited datetime + elapsed seconds `TimeSpan`
- Session is properly closed on save completion and on idle timeout back to `TIME`.

### Display: VFD-style digit transition effect
- Added non-blocking retro glitch/shuffle transition for changing time digits in `printTime()`.
- Each changed digit now briefly flickers old/new value, then rapidly shuffles random numerals, and settles on target.
- Effect runs only in normal time/date display flow and does not affect settings screens.
- Added feature flag `vfdGlitchEffectEnabled` to quickly enable/disable the effect.

### Notes
- New dependency: `AceButton` Arduino library.
