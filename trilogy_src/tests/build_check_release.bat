@echo off
rem Needs ..\build_trilogy.bat run first (its objects).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 -no_logo
cd /d "%~dp0..\.."
set O=trilogy_src\obj
cl /nologo /O2 /MT /D_CRT_SECURE_NO_WARNINGS /DTRILOGY /DUNICODE /D_UNICODE /Ic_src\platform\windows /Fo%O%\ ^
   trilogy_src\tests\check_release.c %O%\plat_win.obj ^
   %O%\fw\*.obj %O%\chips\*.obj %O%\gen\*.obj %O%\gen_snd\*.obj %O%\sw\*.obj %O%\esb\*.obj ^
   /Fe:trilogy_src\tests\check_release.exe ^
   /link /SUBSYSTEM:CONSOLE user32.lib gdi32.lib winmm.lib ole32.lib dwmapi.lib opengl32.lib shell32.lib winhttp.lib || exit /b 1
trilogy_src\tests\check_release.exe
