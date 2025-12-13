# IMA ADPCM Audio Loading Bug in ScratchEverywhere

## Summary

ScratchEverywhere fails to load and play IMA ADPCM-encoded WAV files from Scratch projects. This affects background music and sound effects in many Scratch projects, as Scratch commonly uses IMA ADPCM compression for audio assets.

## Symptoms

1. Audio file loads with error: `"Unknown WAVE data format"`
2. Music plays for approximately 1 second then stops abruptly
3. Game continues running normally (sprites animate) but audio is silent

## Affected Audio Format

- **Format**: IMA ADPCM (Interactive Multimedia Association Adaptive Differential Pulse Code Modulation)
- **WAV Format Tag**: `0x0011` (17 decimal)
- **Location in WAV header**: Bytes 20-21 contain the format tag
- **Common in Scratch**: Many Scratch projects use IMA ADPCM for compressed audio

Example from DELTARUNE Lancer Battle project:
```
9657a545b10607839d38826e424ed451.wav
- Format: IMA ADPCM
- Sample Rate: 22050 Hz
- Channels: Mono
- Size: ~5.8 MB
```

## Root Cause #1: SDL_mixer RWops Bug

### The Problem

SDL_mixer's `Mix_LoadWAV_RW()` function fails to decode IMA ADPCM audio when loaded from memory via `SDL_RWFromMem()`. However, the same function succeeds when loading the identical file from disk via `Mix_LoadWAV()`.

### Where It Happens

**File**: `source/sdl2/audio/audio.cpp`
**Function**: `SoundPlayer::loadSoundFromSB3()`
**Lines**: ~131-156

```cpp
// Audio is extracted from ZIP to memory
void *file_data = mz_zip_reader_extract_to_heap(zip, i, &file_size, 0);

// RWops created from memory buffer
SDL_RWops *rw = SDL_RWFromMem(file_data, (int)file_size);

// This FAILS for IMA ADPCM with "Unknown WAVE data format"
chunk = Mix_LoadWAV_RW(rw, 1);
```

### Why It Happens

SDL2's internal WAV decoder has full IMA ADPCM support (confirmed by examining `SDL_wave.c` which contains `IMA_ADPCM_Decode()` and related functions). The symbols exist in the compiled library.

However, there appears to be a bug in how SDL handles WAV decoding when the source is an in-memory RWops versus a file-based RWops. The exact cause within SDL is unclear, but the behavior is reproducible:

| Load Method | IMA ADPCM Result |
|-------------|------------------|
| `Mix_LoadWAV("/path/to/file.wav")` | **Success** |
| `Mix_LoadWAV_RW(SDL_RWFromMem(...), 1)` | **Failure** |
| `Mix_LoadWAV_RW(SDL_RWFromFile(...), 1)` | **Success** |

This was verified by:
1. Extracting the ADPCM file to `/tmp/`
2. Loading via `Mix_LoadWAV()` from that path - **works**
3. Loading the same bytes via `SDL_RWFromMem()` - **fails**

### PCM Audio Unaffected

Standard PCM WAV files (format tag `0x0001`) load correctly from memory. Only ADPCM formats exhibit this bug.

## Root Cause #2: Channel Collision

### The Problem

Even after successfully loading ADPCM audio, the music stops playing after ~1 second while the game continues running.

### Where It Happens

**File**: `source/sdl2/audio/audio.cpp`
**Function**: `SoundPlayer::stopSound()`
**Lines**: ~430-452

### Why It Happens

1. **Channel Assignment**: SDL_mixer assigns channels dynamically. When `Mix_PlayChannel(-1, chunk, loops)` is called, SDL picks an available channel (often channel 0).

2. **Multiple Sounds, Same Channel**: When Sound A plays, it gets channel 0. When Sound B plays shortly after, it may also get channel 0 (taking over from Sound A). Both sounds store `channelId = 0` in their `SDL_Audio` objects.

3. **Stop Halts Wrong Sound**: When the game logic calls `stopSound("SoundA")`, the code looks up Sound A's stored `channelId` (0) and calls `Mix_HaltChannel(0)`. This halts whatever is **currently** playing on channel 0 - which is now Sound B (the music), not Sound A.

```cpp
void SoundPlayer::stopSound(const std::string &soundId) {
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        int channel = soundFind->second->channelId;  // Gets stored channel (0)
        Mix_HaltChannel(channel);  // Halts channel 0, killing the music!
    }
}
```

### Sequence of Events

```
1. Game starts
2. Sound effect "c5e1eda..." plays on channel 0
3. Music "9657a545..." plays on channel 0 (takes over)
4. Both SDL_Audio objects now have channelId = 0
5. Game logic calls stopSound("c5e1eda...")
6. Code looks up channelId for "c5e1eda..." -> 0
7. Mix_HaltChannel(0) is called
8. Music stops (it was on channel 0)
9. Game continues but audio is silent
```

### Evidence

Debug logging showed repeated `stopSound` calls halting channel 0:
```
Successfully loaded audio!
Successfully loaded audio!
DEBUG stopSound: c5e1eda379225c23640987493d87996a.wav
DEBUG halting ch 0    <-- This kills the music!
DEBUG stopSound: c5e1eda379225c23640987493d87996a.wav
DEBUG halting ch 0    <-- Called again
...
```

## Affected Code Paths

### Loading (Root Cause #1)
- `source/sdl2/audio/audio.cpp` - `loadSoundFromSB3()`
- Called when: Any audio file is loaded from a `.sb3` project archive

### Stopping (Root Cause #2)
- `source/sdl2/audio/audio.cpp` - `stopSound()`
- `source/scratch/blocks/control.cpp` - "stop other scripts in sprite" block
- `source/scratch/blocks/sound.cpp` - "stop all sounds" block

## Detection

To identify if a WAV file uses IMA ADPCM:

```cpp
unsigned char* bytes = (unsigned char*)file_data;
bool isADPCM = (file_size > 22 && bytes[20] == 0x11 && bytes[21] == 0x00);
```

Or via command line:
```bash
file audio.wav
# Output: RIFF (little-endian) data, WAVE audio, IMA ADPCM, mono 22050 Hz
```

## Related Files

- `source/sdl2/audio/audio.cpp` - Main SDL2 audio implementation
- `source/sdl2/audio/audio.hpp` - SDL_Audio class definition
- `source/scratch/audio.hpp` - SoundPlayer interface
- `build_local/_deps/sdl2-src/src/audio/SDL_wave.c` - SDL's WAV decoder (has ADPCM support)
