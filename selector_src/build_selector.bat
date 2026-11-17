@echo off
rem Builds selection.exe: the game selection page, on the C port's beam
rem renderer and Windows framework files (taken from ..\c_src, not copied).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 -no_logo
cd /d "%~dp0"
if not exist obj mkdir obj
set FW=..\c_src\platform\windows
cl /nologo /O2 /W3 /std:c11 /MD /D_CRT_SECURE_NO_WARNINGS /DUNICODE /D_UNICODE /I%FW% /Foobj\ ^
   selector.c %FW%\sys_gl.c %FW%\glew.c %FW%\log.c %FW%\vector_draw.c %FW%\mat4.c ^
   %FW%\rawinput.c %FW%\ini.c %FW%\joystick.c ^
   /Fe:selection.exe ^
   /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib winmm.lib ole32.lib opengl32.lib || exit /b 1
echo SELECTOR BUILD OK
