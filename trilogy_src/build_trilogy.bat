@echo off
rem Builds StarWarsTrilogy.exe: Star Wars (the C port, ..\c_src), The Empire
rem Strikes Back (its ROM on 6809 cores, ..\esb_src) and the selection page
rem (..\selector_src) on one platform layer (..\c_src\platform\windows),
rem with the C runtime linked in (/MT): one file, no DLL beside it.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 -no_logo
cd /d "%~dp0.."

set O=trilogy_src\obj
set FW=c_src\platform\windows
set CF=/nologo /O2 /MT /D_CRT_SECURE_NO_WARNINGS /DTRILOGY
for %%d in (%O% %O%\fw %O%\chips %O%\gen %O%\gen_snd %O%\sw %O%\esb %O%\sel) do if not exist %%d mkdir %%d
del /q %O%\gen\*.obj %O%\gen_snd\*.obj 2>nul

rem ---- framework and chips (one copy: ESB's chips are the same files) ----
cl %CF% /W3 /DUNICODE /D_UNICODE /c %FW%\sys_gl.c %FW%\glew.c %FW%\log.c %FW%\vector_draw.c ^
   %FW%\mat4.c %FW%\rawinput.c %FW%\mixer.c %FW%\fileio.c %FW%\miniz.c %FW%\ini.c %FW%\joystick.c ^
   /Fo%O%\fw\ || exit /b 1
cl %CF% /W3 /std:c11 /Ic_src /c c_src\c012294.c c_src\avg.c c_src\mathbox.c c_src\x2212.c c_src\riot6532.c /Fo%O%\chips\ || exit /b 1
cl %CF% /W3 /std:c++17 /EHsc /Ic_src /c c_src\tms5220.cpp /Fo%O%\chips\ || exit /b 1

rem ---- Star Wars: the translated programs and the C port's machine ----
cl %CF% /W3 /std:c11 /Ic_src /c c_src\gen\*.c c_src\hand\*.c c_src\sw_rt.c /Fo%O%\gen\ || exit /b 1
cl %CF% /W3 /std:c11 /Ic_src /c c_src\gen_snd\*.c /Fo%O%\gen_snd\ || exit /b 1
cl %CF% /W3 /std:c11 /DSW_SND_PROGRAM /Ic_src /c c_src\sw_rt.c /Fo%O%\gen_snd\ || exit /b 1
cl %CF% /W4 /std:c++17 /EHsc /Ic_src /c c_src\hw.cpp c_src\app.cpp c_src\port.cpp /Fo%O%\sw\ || exit /b 1

rem ---- The Empire Strikes Back: its board in namespace esb ----
cl %CF% /W3 /std:c++17 /EHsc /Iesb_src /Iesb_src\cpu /Ic_src /c esb_src\cpu\cpu_m6809.cpp ^
   esb_src\hw.cpp esb_src\board.cpp esb_src\emu_main.cpp /Fo%O%\esb\ || exit /b 1

rem ---- the selection page and the platform layer ----
cl %CF% /W3 /std:c11 /DUNICODE /D_UNICODE /I%FW% /c selector_src\selector.c /Fo%O%\sel\ || exit /b 1
echo === StarWarsTrilogy.exe
cl %CF% /W4 /std:c11 /DUNICODE /D_UNICODE /Ic_src /Fo%O%\ %FW%\plat_win.c ^
   %O%\fw\*.obj %O%\chips\*.obj %O%\gen\*.obj %O%\gen_snd\*.obj %O%\sw\*.obj %O%\esb\*.obj %O%\sel\*.obj ^
   /Fe:trilogy_src\StarWarsTrilogy.exe ^
   /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib winmm.lib ole32.lib dwmapi.lib opengl32.lib shell32.lib winhttp.lib || exit /b 1
echo TRILOGY BUILD OK
