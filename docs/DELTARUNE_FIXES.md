# DELTARUNE Battle Softlock Fixes

This document summarizes the investigation and fixes for issues in the DELTARUNE Scratch project.

## Summary of Changes

| Fix | Branch | Status |
|-----|--------|--------|
| Broadcast case-insensitive matching | `broadcast-case-insensitive` | ✅ Merged to main |
| Deterministic execution order (soundOrder/blockChainOrder) | `deterministic-execution` | 🔄 PR pending review |
| IMA ADPCM audio decoder | `adpcm-fix` | 🔄 PR pending review |
| Procedure definition lookup fix | `combined-fixes` (local) | 🔧 Fixed locally |
| Pen stamp coordinate fix | `combined-fixes` (local) | 🔧 Fixed locally |

---

## Issues Found & Fixed

### 1. Broadcast Case Sensitivity
**Branch:** `broadcast-case-insensitive` → **Merged to main**

**Problem:** Broadcast name matching was case-sensitive. The project sends `"choices"` (lowercase) but the receiver listens for `"Choices"` (capital C), so the broadcast was never received.

**Fix:** Normalize both broadcast names to lowercase before comparison:
```cpp
std::transform(broadcastToRun.begin(), broadcastToRun.end(), broadcastToRun.begin(), ::tolower);
// Also normalize receiverName before comparison
```

### 2. Sound Index Lookup Order
**Branch:** `deterministic-execution` → **PR pending**

**Files:**
- `source/scratch/sprite.hpp` - Added `soundOrder` vector
- `source/scratch/interpret.cpp` - Populate `soundOrder` during parsing
- `source/scratch/blocks/sound.cpp` - Use `soundOrder` for index lookup

**Problem:** Sounds were stored in `std::map<std::string, Sound>` which orders alphabetically by name. When looking up sound by numeric index (e.g., "1"), the map iteration order didn't match project order. This caused wrong sounds to play (e.g., "explosion" instead of "text noise").

**Why std::map doesn't work:** `std::map` iterates in alphabetical key order, NOT insertion order. For example, Menu sprite sounds in project order are `['hurt noise', 'select', 'move selection']` but std::map gives `['hurt noise', 'move selection', 'select']`. Testing showed 43% of sprites got wrong sounds with alphabetical ordering.

**Fix:** Added `std::vector<std::string> soundOrder` to preserve project order:
```cpp
// In sprite.hpp
std::vector<std::string> soundOrder;  // Preserves project order for index-based lookup

// In interpret.cpp (during parsing)
newSprite->soundOrder.push_back(newSound.name);

// In sound.cpp (during lookup)
const std::string& soundName = sprite->soundOrder[soundIndex];
auto it = sprite->sounds.find(soundName);
```

### 3. Non-Deterministic Script Execution Order
**Branch:** `deterministic-execution` → **PR pending**

**Files:**
- `source/scratch/sprite.hpp` - Added `blockChainOrder` vector
- `source/scratch/interpret.cpp` - Populate `blockChainOrder` during parsing
- `source/scratch/blockExecutor.cpp` - Use `blockChainOrder` for iteration

**Problem:** Block chains were stored in `std::unordered_map<std::string, BlockChain>` which has non-deterministic iteration order. This caused scripts to execute in random order each run, leading to visual inconsistencies (menu animations flickering, frame gaps).

**Fix:** Added `std::vector<std::string> blockChainOrder` to preserve insertion order:
```cpp
// In sprite.hpp
std::vector<std::string> blockChainOrder;  // Preserves project order for deterministic execution

// In interpret.cpp (during parsing)
currentSprite->blockChains[outID] = chain;
currentSprite->blockChainOrder.push_back(outID);

// In blockExecutor.cpp (runRepeatBlocks)
for (const std::string &chainId : sprite->blockChainOrder) {
    auto it = sprite->blockChains.find(chainId);
    if (it == sprite->blockChains.end()) continue;
    // ... process block chain
}
```

### 4. ADPCM Audio Decoding
**Branch:** `adpcm-fix` → **PR pending**

**Files:**
- `source/sdl2/audio/audio.cpp` - IMA ADPCM decoder
- `source/sdl2/audio/audio.hpp` - Added `chunk` pointer for collision detection

**Problem:** SDL_mixer doesn't natively decode IMA ADPCM WAV files, causing audio playback failures.

**Fix:** Added IMA ADPCM→PCM decoder that converts in memory before passing to SDL_mixer.

### 5. Procedure Definition Lookup Fix
**Status:** Fixed locally (not yet PR'd)

**File:** `source/scratch/blockExecutor.cpp` (runCustomBlock function)

**Problem:** The code tried to find the procedure definition by looking up the prototype's parent:
```cpp
Block *customBlockDefinition = &sprite->blocks[sprite->blocks[data.blockId].parent];
```
But the prototype block's `parent` field is often `null`/empty, causing the lookup to fail silently. Procedures would be called but never execute their body.

**Fix:** Search for `procedures_definition` blocks that contain the matching prototype ID:
```cpp
Block *customBlockDefinition = nullptr;
for (auto &[bid, blk] : sprite->blocks) {
    if (blk.opcode == "procedures_definition") {
        if (!blk.parsedInputs) continue;
        auto it = blk.parsedInputs->find("custom_block");
        if (it != blk.parsedInputs->end()) {
            std::string prototypeId = it->second.inputType == ParsedInput::LITERAL
                ? it->second.literalValue.asString()
                : it->second.blockId;
            if (prototypeId == data.blockId) {
                customBlockDefinition = &sprite->blocks[bid];
                break;
            }
        }
    }
}
```

### 6. Pen Stamp Coordinate Fix
**Status:** Fixed locally (not yet PR'd)

**File:** `source/sdl2/render.cpp` (penStamp function)

**Problem:** The original code did a double coordinate conversion using `screenToScratchCoords` which caused stamps to render in wrong positions.

**Fix:** Calculate pen position directly from Scratch coordinates:
```cpp
double scale = sprite->size / (isSVG ? 100.0 : 200.0);
double rcxOffset = sprite->rotationCenterX * scale;
double rcyOffset = sprite->rotationCenterY * scale;
image->renderRect.x = static_cast<int>(sprite->xPosition + Scratch::projectWidth / 2.0 - rcxOffset);
image->renderRect.y = static_cast<int>(Scratch::projectHeight / 2.0 - sprite->yPosition - rcyOffset);
```

---

## How Text Streaming Works in This Project

The "Letters" sprite uses pen stamping to render text:
1. Procedure `%s with delay %s sound %s` is called with text, delay, and sound parameters
2. A repeat loop iterates through each character
3. For each character:
   - Switch costume to the matching letter
   - `pen_stamp` to render the letter to the pen layer
   - Move X position for next letter
   - Play sound (conditionally)
4. After loop: wait for delay duration

---

## Known Issues (Unresolved)

### Battle Text Apostrophe Issue
**Status:** Under investigation

**Symptom:** In battle dialogue, apostrophe characters appear where spaces should be. Example: "So'what'are" instead of "So what are".

**Notes:**
- Bottom streamed text renders correctly
- Battle text uses different rendering path or font sprite
- Likely a costume selection issue, not pen positioning

### X Button Menu Flash
**Status:** Unresolved

**Symptom:** When pressing the X button (back button) to return to the main menu, there's a brief flash/flicker visible for a split second.

**Investigation Notes:**
- Does NOT happen on the original Scratch web runtime
- Tried moving `Input::getInput()` outside the frame rate conditional - didn't help
- Tried draining broadcast queue before render - didn't help
- Logging shows SHOW/HIDE/DELETE sequences happening correctly
- Likely a timing issue with how broadcasts or clones are processed vs rendered

---

## Do Not Port

### Sound Overlap Prevention
**File:** `source/scratch/blocks/sound.cpp`

Code that prevents sounds from playing if already playing:
```cpp
else if (!SoundPlayer::isSoundPlaying(soundFullName))
    SoundPlayer::playSound(soundFullName);
```

**Do not include.** Real Scratch allows overlapping sounds - calling `play sound` multiple times starts multiple instances. This was a bandaid that breaks correct Scratch behavior.

### Debug Logging
**Files:** `control.cpp`, `events.cpp`, `looks.cpp`

`std::cerr` debug logging for clone deletion, broadcasts, show/hide/costume switches.

**Do not include.** Development aids only.

---

## Related PRs/Branches
- **Merged:** Broadcast case sensitivity (`broadcast-case-insensitive`)
- **Pending:** Deterministic execution (`deterministic-execution`)
- **Pending:** ADPCM audio (`adpcm-fix`)
- **Already in main:** Effect name case sensitivity fix (`c58cc8b`)
