mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\include\QTMovieWin"
xcopy /y /d "%PROJECTDIR%..\..\platform\graphics\win\QTMovie.h" "%CONFIGURATIONBUILDDIR%\include\QTMovieWin"
if exist "%WEBKIT_LIBRARIES%\tools\VersionStamper\VersionStamper.exe" "%WEBKIT_LIBRARIES%\tools\VersionStamper\VersionStamper.exe" --verbose "%TARGETPATH%"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
