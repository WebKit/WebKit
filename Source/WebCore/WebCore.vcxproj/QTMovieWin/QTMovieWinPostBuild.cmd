mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\include\QTMovieWin"
xcopy /y /d "%PROJECTDIR%..\platform\graphics\win\QTMovie.h" "%CONFIGURATIONBUILDDIR%\include\QTMovieWin"
if exist "%WEBKITLIBRARIESDIR%\tools\VersionStamper\VersionStamper.exe" "%WEBKITLIBRARIESDIR%\tools\VersionStamper\VersionStamper.exe" --verbose "%TARGETPATH%"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
