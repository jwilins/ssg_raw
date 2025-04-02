@echo off

if "%VCINSTALLDIR%" == "" (
	echo Error: The build must be run from within Visual Studio's `x64_x86 Cross Tools Command Prompt`.
	exit 1
)

sh ./submodules_check.sh ^
	libs/BLAKE3 ^
	libs/dr_libs ^
	libs/libogg ^
	libs/libvorbis ^
	libs/miniaudio ^
	libs/SDL ^
	libs/tupblocks
if %errorlevel% neq 0 exit /b %errorlevel%

tup %*
exit /b
