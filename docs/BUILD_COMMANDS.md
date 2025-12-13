# Build Commands for ScratchEverywhere

## macOS / Linux (SDL2 Desktop Build)

### Quick Build & Run

```bash
cd /Users/kalomaze/Documents/claude_zone/scratch_anywhere_section/ScratchEverywhere/build_local

# Clean, build, and set up executable
rm -rf se_exe scratch-everywhere && make -j8 && mv scratch-everywhere se_exe && mkdir scratch-everywhere && cp project.sb3 scratch-everywhere/

# Run
./se_exe
```

### Step-by-Step Breakdown

1. **Navigate to build directory:**
   ```bash
   cd /path/to/ScratchEverywhere/build_local
   ```

2. **Clean previous build artifacts:**
   ```bash
   rm -rf se_exe scratch-everywhere
   ```

3. **Build with parallel jobs:**
   ```bash
   make -j8
   ```
   (Adjust `-j8` based on your CPU cores)

4. **Rename executable:**
   ```bash
   mv scratch-everywhere se_exe
   ```

5. **Create project directory and copy project:**
   ```bash
   mkdir scratch-everywhere
   cp project.sb3 scratch-everywhere/
   ```

6. **Run:**
   ```bash
   ./se_exe
   ```

### With Debug Output

```bash
# Run with stderr visible
./se_exe

# Filter for specific debug messages
./se_exe 2>&1 | grep COSTUME
./se_exe 2>&1 | grep BROADCAST
./se_exe 2>&1 | grep "\[SHOW\]\|\[HIDE\]"
```

## PowerPC (Wii/GameCube) Build

### Prerequisites
- devkitPPC toolchain installed
- DEVKITPRO environment variable set

### Build Command

```bash
cd /path/to/ScratchEverywhere

# Configure for Wii
cmake -B build_wii -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Wii.cmake

# Build
cmake --build build_wii -j8
```

### For GameCube

```bash
cmake -B build_gc -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/GameCube.cmake
cmake --build build_gc -j8
```

## Initial CMake Configuration (First Time Only)

If `build_local` doesn't exist or needs reconfiguration:

```bash
cd /path/to/ScratchEverywhere

# Create build directory and configure
cmake -B build_local -DCMAKE_BUILD_TYPE=Debug

# Or for release build
cmake -B build_local -DCMAKE_BUILD_TYPE=Release
```

## Troubleshooting

### "Error: could not load cache"
You're in the wrong directory or the build directory doesn't exist. Make sure to:
1. Be in the correct `build_local` directory
2. Run cmake configuration first if needed

### Linker warnings about duplicate libraries
These are harmless warnings from SDL2 dependencies and can be ignored:
```
ld: warning: ignoring duplicate libraries: '_deps/sdl2-build/libSDL2.a', ...
```

### Project not loading
Make sure `project.sb3` exists in the `scratch-everywhere/` subdirectory relative to the executable.
