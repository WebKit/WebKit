mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\include\QTMovieWin"
xcopy /y /d "%PROJECTDIR%..\platform\graphics\win\QTMovie.h" "%CONFIGURATIONBUILDDIR%\include\QTMovieWin"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
