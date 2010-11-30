mkdir 2>NUL "%WEBKITOUTPUTDIR%\include\QTMovieWin"
xcopy /y /d "%PROJECTDIR%..\platform\graphics\win\QTMovieWin.h" "%WEBKITOUTPUTDIR%\include\QTMovieWin"

if exist "%WEBKITOUTPUTDIR%\buildfailed" del "%WEBKITOUTPUTDIR%\buildfailed"
