# 24MB Video Compressor

A high-performance, batch video compressor for Windows designed to easily compress large video files to meet upload size limits for various platforms (Discord, WhatsApp, Telegram, Email attachments, and X/Twitter). Built with C++23, Raylib, ImGui, and FFmpeg (utilizing hardware-accelerated Windows Media Foundation codecs).

---

## Features

- **Social Media Target Presets**: Instantly choose upload size thresholds for popular platforms:
  - **10 MB** (Discord Free / WeChat Video)
  - **16 MB** (WhatsApp Video Standard)
  - **24 MB** (Discord Classic / Email Attachment - Safe Limit)
  - **50 MB** (Discord Nitro Basic / Telegram)
  - **100 MB** (Discord Nitro / WhatsApp HD / TikTok)
  - **512 MB** (X/Twitter Free Video Limit)
- **Custom Targets**: Toggle to a custom limit slider matching any file boundary from `1.0 MB` to `1000.0 MB`.
- **Hardware Accelerated Encoding**: Uses the Windows Media Foundation `h264_mf` video encoder inside FFmpeg for fast, GPU-accelerated single-pass encoding.
- **Multithreading Processing**: Dynamic worker ThreadPool allowing configurable concurrent video encoding tasks (1 to 8 concurrent threads).
- **Interactive GUI**: Fully responsive dark-themed dashboard built on Immediate Mode GUI (ImGui) and Raylib.
- **Native File Dialogs**: Choose input files or select a custom output folder using standard Windows explorer selection sheets.
- **Drag & Drop**: Easily drop video files directly into the window to queue them.
- **One-Click Playback**: Completed tasks display an "Open" button to instantly play the video in your default media player.
- **Sticky Settings**: Saves and loads your custom output folder path automatically.

---

## Technical Stack

- **Language**: C++23 (standard MSVC / `/std:c++latest`)
- **Graphics & Windowing**: Raylib (v6.0 compiled statically)
- **User Interface**: Dear ImGui via `rlImGui`
- **Dependencies**: FFmpeg 8.x (installed via `vcpkg`)
- **Build System**: CMake

---

## Directory Structure

```
24mb-video-compressor/
├── CMakeLists.txt       # Build system configuration
├── CMakePresets.json    # Visual Studio CMake build presets
├── vcpkg.json           # Declarative dependencies manifest
├── include/             # Header files
│   ├── dialogs.hpp      # Isolated native Win32 dialog/settings utilities
│   ├── FFmpegRunner.hpp # Subprocess wrapper and stderr progress parser
│   ├── ThreadPool.hpp   # Concurrency thread pool queue
│   ├── TaskQueue.hpp    # Task synchronization block queue
│   └── TaskStatus.hpp   # Task states and structs definitions
└── src/                 # Source implementation files
    ├── main.cpp         # Main execution loop and ImGui dashboard
    ├── dialogs.cpp      # Native Windows COM file/folder wrappers
    ├── FFmpegRunner.cpp # Bitrate calculation and FFmpeg child launcher
    ├── ThreadPool.cpp   # Worker thread pool manager
    └── TaskQueue.cpp    # Thread-safe task manager
```

---

## Building from Source

### Prerequisites

1. **Visual Studio 2022** with "Desktop development with C++" workload installed.
2. **vcpkg** (C++ Package Manager) installed and configured on your system. Make sure `VCPKG_ROOT` is set in your environment variables.

### Build Instructions

1. **Clone the repository**:
   ```bash
   git clone https://github.com/sedatsan/24mb-video-compressor.git
   cd 24mb-video-compressor
   ```

2. **Configure CMake**:
   Make sure the CMake toolchain path points to your `vcpkg` directory (or use the configured presets in Visual Studio).
   ```bash
   cmake --preset x64-debug
   ```

3. **Build the executable**:
   ```bash
   cmake --build out/build/x64-debug
   ```

4. **Run the program**:
   The output binary will be compiled to `out/build/x64-debug/24mb-video-compressor.exe`. Ensure the local FFmpeg binaries from `vcpkg_installed` are available.

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
