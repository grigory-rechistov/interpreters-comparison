@echo off



set steps=100
set mode=Debug
set targets=switched.exe predecoded.exe subroutined.exe tailrecursive.exe
set exepath=%~dp0%x64
set exepath=%exepath%\%mode%\

@echo %exepath%

(for %%a in (%targets%) do (
   @echo Testing %%a
   call "%exepath%%%a" "%steps%"
   @echo\
))
@echo "Sanity OK"
pause