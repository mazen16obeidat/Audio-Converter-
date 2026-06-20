# Audio-Converter-
Audio converter , it converts MP3, WAV, FLAC  both ways , (FLAC to WAV and vice versa ..etc.) .

I made it because I'm sick of sites.

Description
-----------
A simple graphical tool that converts audio files between WAV, FLAC and MP3
formats in any direction. It supports drag-and-drop, file browsing, format
auto-detection, overwrite protection, and a progress bar.

Supported conversions
---------------------
- WAV  <-> MP3
- WAV  <-> FLAC
- FLAC <-> MP3

Features
--------
- Drag and drop an audio file onto the window to load it
- Browse buttons for manual file selection
- Smart default output format (MP3 -> FLAC, FLAC -> WAV, WAV -> MP3)
- Overwrite confirmation if output file already exists
- Swap input/output paths with one click
- Clear button to reset all fields
- Progress bar during conversion
- Remember last used folders
- Command-line argument: pass a file to pre-fill the input field
  Example: converter.exe myaudio.mp3

System requirements (for the pre-built executable)
--------------------------------------------------
- Windows 10 or later (64-bit)
- No additional software or libraries needed

Running the pre-built package (ZIP)
------------------------------------
1. Download the ZIP file and extract it to any folder.
2. Open the extracted folder. You will see:
   - converter.exe
   - Several .dll files (libfltk.dll, libmp3lame.dll, etc.)
3. Double-click converter.exe to launch the application.
4. You can also drag a file onto converter.exe or run it from Command Prompt:
     "C:\path\to\converter.exe" "C:\path\to\audiofile.wav"

Important: Keep all .dll files in the same folder as converter.exe.
           Do not delete or move them separately.

Building from source (MSYS2 UCRT64)
-----------------------------------
If you prefer to compile the program yourself, follow these steps.

Build dependencies (MSYS2 UCRT64)
----------------------------------
Open MSYS2 UCRT64 terminal and run:

  pacman -S --needed mingw-w64-ucrt-x86_64-gcc
  pacman -S --needed mingw-w64-ucrt-x86_64-fltk
  pacman -S --needed mingw-w64-ucrt-x86_64-lame
  pacman -S --needed mingw-w64-ucrt-x86_64-flac

Single-header libraries
-----------------------
Place these files in the same folder as Audio_converter.cpp:

  dr_wav.h   - from https://github.com/mackron/dr_libs
  dr_flac.h  - from https://github.com/mackron/dr_libs
  minimp3.h  - from https://github.com/lieff/minimp3

Download commands (PowerShell in the project folder):

  Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mackron/dr_libs/master/dr_wav.h" -OutFile "dr_wav.h"
  Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mackron/dr_libs/master/dr_flac.h" -OutFile "dr_flac.h"
  Invoke-WebRequest -Uri "https://raw.githubusercontent.com/lieff/minimp3/master/minimp3.h" -OutFile "minimp3.h"

Compilation (shared build, produces DLL-dependent executable)
-------------------------------------------------------------
This is the default build. It creates converter.exe and requires the DLLs
from MSYS2 to run. The DLLs must be distributed alongside the executable.

From the project folder in UCRT64 terminal:

  g++ -std=c++17 -O2 Audio_converter.cpp -o converter.exe -mwindows -lfltk -lfltk_images -lmp3lame -lFLAC -logg -lpthread -lole32 -luuid -lcomctl32

(If -logg is not found, remove it.)

To make the executable run on any machine, copy the following DLLs
from /ucrt64/bin/ into the same folder as converter.exe:

  libfltk.dll
  libfltk_images.dll
  libmp3lame.dll
  libFLAC.dll
  libogg.dll
  libgcc_s_seh-1.dll
  libstdc++-6.dll
  libwinpthread-1.dll

Then zip the folder and distribute.

Note on static linking
----------------------
A fully self-contained .exe (no DLLs) is not currently possible with the
standard MSYS2 packages because mingw-w64-ucrt-x86_64-fltk-static is not
available. The recommended distribution method is to bundle the required DLLs
as described above. If you need a single-file executable, you would need to
compile FLTK from source with static linking enabled.

Context menu integration (Windows)
-----------------------------------
You can add "Convert with Audio Converter" to the right-click menu for
all file types. Create a .reg file with the content below (adjust the path
to converter.exe) and double-click to install.

  Windows Registry Editor Version 5.00

  [HKEY_CLASSES_ROOT\*\shell\Convert with Audio Converter]
  @="Convert with Audio Converter"

  [HKEY_CLASSES_ROOT\*\shell\Convert with Audio Converter\command]
  @="\"C:\\projects\\Audio-Converter\\converter.exe\" \"%1\""

Uninstall by deleting the registry key or creating a .reg file with a minus
sign (e.g., [-HKEY_CLASSES_ROOT\*\shell\Convert with Audio Converter]).

Notes
-----
- FLAC encoding uses libFLAC, not dr_flac.h (dr_flac is decoder only).
- WAV output is always 16-bit PCM.
- MP3 encoding uses LAME VBR with quality level 2 (~190 kbps).
- The program does not preserve metadata/tags.
- Tested with MSYS2 UCRT64 GCC 15.2.0.

License
-------
This project uses third-party libraries under their own licenses:
  - FLTK: LGPL with static linking exception
  - LAME: LGPL
  - FLAC/libFLAC: BSD license
  - dr_wav, dr_flac: public domain (or MIT-0)
  - minimp3: CC0

The converter source code (Audio_converter.cpp) is provided as-is,
free for personal and commercial use.
