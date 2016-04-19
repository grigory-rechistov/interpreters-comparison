@echo off

set steps=100000000
set mode=Debug
set targets=switched.exe predecoded.exe subroutined.exe tailrecursive.exe
set exepath=%~dp0%x64
set exepath=%exepath%\%mode%\

@echo %exepath%

(for %%a in (%targets%) do (
   @echo Running %%a
   start "%%a" timecmd.bat "%exepath%%%a" %steps%
   @echo\
))
@echo Done
pause