# Avidemux Custom Windows Build

Chinese version: [README.md](README.md)

This branch contains a Windows-focused custom build of Avidemux with workflow enhancements for fast remuxing, profile-based encoding, predictable output naming, and bundled x264/x265 support.

## Screenshots

English UI:

![Avidemux custom main window in English](docs/screenshots/main-en.png)

Simplified Chinese UI:

![Avidemux custom main window in Simplified Chinese](docs/screenshots/main-zh-cn.png)

## Main enhancements

### Output Profile workflow

A new **Output Profile** selector is added above the original video/audio/container controls. Selecting a profile automatically sets:

- video encoder,
- audio encoder,
- output container,
- optional resize mode,
- encoder preset/profile where available.

Built-in profiles include:

- `Copy`
- `DivX HEVC 1080p`
- `DivX HEVC 720p`
- `DivX Plus 4K`
- `DivX Plus HD`
- `MP4 iPad`
- `MP4 iPhone`

The default profile is `Copy`, preserving the original video/audio stream unless the user chooses an encoding profile.

### Custom profile saving

The current output setup can be saved as a custom output profile from the **Save** button next to the profile selector.

Saved custom profiles preserve:

- selected video encoder,
- selected audio encoder,
- selected container,
- resize mode,
- target width and height.

### Resize modes

Profiles do not force resizing unless the selected mode requires it. The size selector supports:

- original size,
- profile size with aspect ratio preserved,
- custom size with aspect ratio preserved,
- custom size stretch.

When custom size with aspect ratio preserved is selected, editing either width or height automatically calculates the other dimension from the source video ratio.

### Automatic fit-to-window on video open

Opening a video now fits the video into the current video display area. The video is fitted to the existing window instead of resizing the window to the video.

### MP4 as default output container

The default output format is changed to MP4 muxer.

### Predictable output naming

The default save name no longer uses `_edit`.

Copy mode:

```text
source-001.mp4
source-002.mp4
source-003.mp4
```

Encoding mode:

```text
source-720p-h264.mp4
source-720p-h264-001.mp4
source-360p-hevc.mp4
```

The resolution suffix is based on the final output height. The codec suffix distinguishes the video encoder only, for example `h264` for x264 and `hevc` for x265.

### Auto save to folder

An **Auto save to folder** option is added at the bottom of the main output panel.

- Checked: save directly to the configured directory using the automatic naming rules.
- Unchecked: open the normal save dialog, prefilled with the automatic name.

### x264 / x265 support

This custom build adds x264 and x265 encoder support for the Windows build environment and includes DivX-style preset JSON profiles.

### Local tuning

x264/x265 presets are tuned for the local target machine used for this build, with explicit thread-related settings for the Ryzen 7 4800U class 8C/16T CPU.

### English / Simplified Chinese / Traditional Chinese UI strings

The custom UI strings added by this branch support:

- English,
- Simplified Chinese,
- Traditional Chinese.

The language follows the Avidemux language preference first. If the preference is `auto`, it falls back to the system locale.

## Windows Build

This branch targets Windows x86-64 only. Building a 32-bit x86 version is not required.

### Build environment

Verified environment:

- Windows x86-64;
- Visual Studio 2022 Build Tools / MSVC;
- CMake;
- Ninja;
- Qt 6.8.3 MSVC 2022 x64;
- MSYS2 for x264 / x265 related dependencies;
- GitHub CLI / Git for source management and pushing.

Example Qt installation path:

```text
D:\Qt\6.8.3\msvc2022_64
```

### Clone the source

```powershell
git clone https://github.com/<your-user>/avidemux2.git
cd avidemux2
git checkout custom-windows-enhancements
```

If your network requires a proxy, configure the local HTTP/HTTPS proxy:

```powershell
git config --global http.proxy http://127.0.0.1:7890
git config --global https.proxy http://127.0.0.1:7890
```

### Build the main application

Build from an “x64 Native Tools Command Prompt for VS 2022”, or from PowerShell / cmd after loading VS DevCmd.

Example command:

```cmd
set QTDIR=D:\Qt\6.8.3\msvc2022_64
set PATH=C:\Program Files\CMake\bin;D:\Qt\6.8.3\msvc2022_64\bin;C:\msys64\usr\bin;%PATH%
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
cmake --build build\qt-msvc2 --config Release --target install --parallel 8
```

If x264 / x265 presets or plugin-related files changed, build and install the plugin tree too:

```cmd
cmake --build build\plugins-msvc-x26x3 --config Release --target install --parallel 8
```

### Create the dist directory

```cmd
xcopy /E /I /Y install\* dist\
D:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe --release --no-compiler-runtime --no-opengl-sw dist\avidemux.exe
```

Remove development files so `.lib` files and `include/` are not shipped:

```cmd
if exist dist\include rmdir /s /q dist\include
del /s /q dist\*.lib
```

### Create the zip package

```powershell
Compress-Archive -Path .\dist\* -DestinationPath .\avidemux-custom-dist.zip -CompressionLevel Optimal
```

### Basic verification

```powershell
$env:Path = "$(Resolve-Path .\dist);" + $env:Path
.\dist\avidemux.exe --nogui --video-codec x264 --quit
.\dist\avidemux.exe --nogui --video-codec x265 --quit
```

Both commands should exit with code `0`, which means the x264 and x265 plugins can be loaded.

## Build status

Verified locally on Windows x86-64 with MSVC / Qt 6.8.3:

- GUI build succeeds.
- x264 codec loading succeeds.
- x265 codec loading succeeds.
- Generated distribution package excludes development files such as `.lib` and `include/`.

## Notes

This branch is intended as a personal custom Windows build. Some behavior, such as output naming and local CPU tuning, is intentionally opinionated and may not be suitable as-is for upstream Avidemux.

