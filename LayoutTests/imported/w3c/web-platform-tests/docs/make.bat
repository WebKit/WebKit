@ECHO OFF

pushd %~dp0

REM Command file for Sphinx documentation

if "%SPHINXBUILD%" == "" (
	set SPHINXBUILD=sphinx-build
)
set SOURCEDIR=.
set BUILDDIR=_build

if "%1" == "" goto help

%SPHINXBUILD% >NUL 2>NUL
if errorlevel 9009 (
	echo.
	echo.The 'sphinx-build' command was not found. Make sure you have Sphinx
	echo.installed, then set the SPHINXBUILD environment variable to point
	echo.to the full path of the 'sphinx-build' executable. Alternatively you
	echo.may add the Sphinx directory to PATH.
	echo.
	echo.If you don't have Sphinx installed, grab it from
	echo.http://sphinx-doc.org/
	exit /b 1
)

if not exist tools\ ( mkdir tools )

if not exist tools\wptserve\ ( mklink /d tools\wptserve ..\..\tools\wptserve )
if not exist tools\certs\ ( mklink /d tools\certs ..\..\tools\certs )
if not exist tools\wptrunner\ ( mklink /d tools\wptrunner ..\..\tools\wptrunner )

if not exist tools\third_party\ ( mkdir tools\third_party )
if not exist tools\third_party\pywebsocket3\ ( mklink /d tools\third_party\pywebsocket3 ..\..\..\tools\third_party\pywebsocket3 )

%SPHINXBUILD% -M %1 %SOURCEDIR% %BUILDDIR% %SPHINXOPTS%
goto end

:help
%SPHINXBUILD% -M help %SOURCEDIR% %BUILDDIR% %SPHINXOPTS%

:end
popd
