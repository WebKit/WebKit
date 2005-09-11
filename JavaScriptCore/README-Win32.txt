Notes about building JavaScriptCore on Win32:

The build has been tested with Visual Studio 2003 (7.1) and Visual Studio
2005 Beta 2. It should also work with Microsft's free
http://msdn.microsoft.com/visualc/vctoolkit2003 (which is basically
the same compiler & linker that is a part of VS 2003 but without the IDE)
but it has not been tested.

It doesn't build with VS 6.

To build:
 * make sure that the tools are in the path. VS comes with vcvars32.bat
   batch file somewhere under installation directory which sets all the
   proper env variables. I usually rename it to e.g. vc7.bat and put
   in my %PATH% for easy invocation)
 * you need to download ICU (http://www-306.ibm.com/software/globalization/icu/index.jsp)
   pre-built DLLs and header files. I've tested it with VS 7.1
   (ftp://ftp.software.ibm.com/software/globalization/icu/3.4/icu-3.4-Win32-msvc7.1.zip)
   Set ICUDIR to where you've downloaded the files.
 * you need perl for generating *.lut.h files
 * you need bison for generating parser from grammar.y (I use http://gnuwin32.sourceforge.net/packages/bison.htm)
 * nmake -f Makefile.vc clean
 * nmake -f Makefile.vc
 or
 * nmake -f Makefile.vc DEBUG=1

What do you get: in bin subdirectory you'll find testkjs.exe executable
which is a javascript interpreter. It can be used to run javascript
tests in tests\mozilla (see \WebKitTools\scripts\run-javascriptcore-tests
for an example on how to run them; you can use this script if you
just replace "testkjs" with a full path to testkjs.exe.

TODO

Currently only testkjs.exe is built. It should also build a static
.lib library so that it's easy to build programs incorporating the core.
It should also build as a dynamic DLL. It's just a matter of fiddling
with the makefile.
