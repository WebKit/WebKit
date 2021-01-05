# Haiku WebKit #

This repository contains the Haiku WebKit port source code.
For more information, please visit the [project's wiki and issue tracker](http://dev.haiku-os.org/)

## Quick build instructions ##

### Cloning the repository ###

This repository is *huge* (about 5 gigabytes). If you are only interested in building
the latest version of WebKit, remember to use the --depth option to git clone.
This can be used to download only a limited part of the history and will reduce the
checkout to about 600MB. Note that WebKit uses SVN and they have the changelog inside the tree in "Changelog"
files, which you can still use as a primitive way to browse the history. You
can also use github for that, or download parts of the history later.

### Requirements ###

- A recent version of Haiku (beta2 is too old, it lacks at least madvise() and support for compositing draw operations)
- The GCC8 development tools
- The dependencies as listed below
- At least about 2G of RAM
- Preferably a fast computer!

Dependencies can be installed (for a gcc2hybrid version) via:

    $ pkgman install cmake_x86 gcc_x86 gperf haiku_x86_devel \
	libjpeg_turbo_x86_devel sqlite_x86_devel libpng16_x86_devel \
	libxml2_x86_devel libxslt_x86_devel icu66_x86_devel perl python \
	ruby_x86 libexecinfo_x86_devel libwebp_x86_devel ninja_x86 \
	pkgconfig_x86 pywebsocket gnutls_x86 gnutls_x86_devel

Additionally if you want to run the tests:

    $ pkgman install php_x86 lighttpd_x86

##### NOTE :
If you get an _Ruby missng error_ even after you have installed ruby, similar to <br>
`Could NOT find Ruby  (missing: RUBY_INCLUDE_DIR RUBY_LIBRARY RUBY_CONFIG_INCLUDE_DIR)  (found suitable version "2.2.0", minimum required is "1.9")`, you can skip that.

Packages for other flavors of Haiku may or may not be available. Use [haikuporter](http://haikuports.org) to build them if needed.

### Building WebKit ###

#### Configuring your build for the first time ####
Commands to run from the webkit checkout directory:

On a gcc2hybrid (32bit) Haiku:
	$ PKG_CONFIG_LIBDIR=/boot/system/develop/lib/x86/pkgconfig \
        CC=gcc-x86 CXX=g++-x86 Tools/Scripts/build-webkit \
		--cmakeargs="-DCMAKE_AR=/bin/ar-x86 -DCMAKE_RANLIB=/bin/ranlib-x86" --haiku

On other versions:
    $ Tools/Scripts/build-webkit --haiku

#### Regular build, once configured ####
	$ cd WebKitBuild/Release
	$ ninja

This will build a release version of WebKit libraries on a quad core cpu.

On a successful build, executables and libraries are generated in the WebKitBuild/Release directory.


### Advanced Build, other targets ###

The following make targets are available:

- libwtf.so - WebKit Template Framework (a complement of the STL used in WebKit)
- libjavascriptcore.so - The JavaScript interpreter
- jsc - The JavaScript executable shell
- libwebcore.so - The WebCore library (cross-platform WebKit code)
- libwebkitlegacy.so - The Haiku specific parts of WebKit
- HaikuLauncher - A simple browsing test app
- DumpRenderTree - The tree parsing test tool

Example given, this will build the JavaScriptCore library:

    $ ninja libjavascriptcore.so

In some rare cases the build system can be confused, to be sure that everything gets rebuilt from scratch,
you can remove the WebKitBuild/ directory and start over.

There are several cmake variables available to configure the build in various ways.
These can be given to build-webkit using the --cmakeargs option, or changed later on
using "cmake -Dvar=value WebKitBuild/Release".

### Speeding up the build with distcc ###

You can set the compiler while calling the configure script:
    $ CC="distcc gcc-x86" CXX="distcc g++-x86" build-webkit ...

It is a good idea to set the NUMBER\_OF\_PROCESSORS environment variable as well
(this will be given to cmake through the -j option). If you don't set it, only
the local CPUs will be counted, leading to a sub-optimal distcc distribution.

distcc will look for a compiler named gcc-x86 and g++-x86. You'll need to adjust
the path on the slaves to get that pointing to the gcc8 version (the gcc8 compiler
is already visible under this name on the local machine and haiku slaves).
CMake usually tries to resolve the compiler to an absolute path on the first
time it is called, but this doesn't work when the compiler is called through
distcc.

## Testing ##

### Testing the test framework ###
    $ ruby Tools/Scripts/test-webkitruby
    $ perl Tools/Scripts/test-webkitperl
    $ python Tools/Scripts/test-webkitpy

The ruby tests pass (all 2 of them!)
The perl test almost pass: Failed 1/27 test programs. 1/482 subtests failed.
The python tests hit some errors related to file locking, tracked in #13795, as
well as some other issues.

### JSC ###
    $ perl Tools/Scripts/run-javascriptcore-tests

Add the --no-build argument if you already compiled JSC. It is built by default
for the Haiku port, so it is a good idea to always add this.

Current results:
- 9258 tests are run (some are excluded because of missing features in our Ruby port)
- 10 failures related to parsing dates and trigonometry:

    mozilla-tests.yaml/ecma_3/Date/15.9.5.6.js.mozilla
    mozilla-tests.yaml/ecma_3/Date/15.9.5.6.js.mozilla-baseline
    mozilla-tests.yaml/ecma_3/Date/15.9.5.6.js.mozilla-dfg-eager-no-cjit-validate-phases
    mozilla-tests.yaml/ecma_3/Date/15.9.5.6.js.mozilla-llint
    stress/ftl-arithcos.js.always-trigger-copy-phase
    stress/ftl-arithcos.js.default
    stress/ftl-arithcos.js.dfg-eager
    stress/ftl-arithcos.js.dfg-eager-no-cjit-validate
    stress/ftl-arithcos.js.no-cjit-validate-phases
    stress/ftl-arithcos.js.no-llint


### WebKit ###
You will have to install the Ahem font for layout tests to work properly. This
is a font with known-size glyphs that render the same on all platforms. Most of
the characters look like black squares, this is expected and not a bug!
http://www.w3.org/Style/CSS/Test/Fonts/Ahem/

$ cp LayoutTests/resources/Ahem.ttf /system/non-packaged/data/fonts/

It is also a good idea to enable automated debug reports for DumpRenderTree.
Create the file ~/config/settings/system/debug\_server/settings and add:

    executable_actions {
        DumpRenderTree log
    }

The crash reports will be moved from the Desktop to the test result directory
and renamed to the name of the test that triggered the crash. If you don't do
this, you have to manually click the "save report" button, and while the
testsuite waits on that, it may mark one or several tests as "timed out".

WebKit also needs an HTTP server for some of the tests, with CGI support for
PHP, Perl, and a few others. You can use the --no-http option to
run-webkit-tests to skip this part. Otherwise you need to install lighttpd
and PHP (both available in HaikuPorts package depot).

Finally, the tests are a mix of html and xhtml files. The file:// loader in
Haiku relies on MIME sniffing to tell them apart. This is not completely
reliable, so for some tests the type needs to be forced (unfortunately this
can't be stored in the git repo):

    $ sh Tools/haiku/mimefix.sh

You can then run the testsuite:

    $ python Tools/Scripts/run-webkit-tests --platform=haiku --dump-render-tree --no-build \
        --no-retry-failures --clobber-old-results --no-new-test-results

The options will prevent the script to try updating DumpRenderTree (it doesn't
know how to do that on Haiku, yet). It doesn't retry failed tests, will remove
previous results before starting, and will not generate missing "expected" files
in the LayoutTests directory.

A lot of tests are currently failing. The problems are either in the WebKit
code itself, or in the various parts of the test harness, none of which are
actually complete: DumpRenderTree, webkitpy, etc. Some of them are triggering
asserts in WebKit code.

You can run the tests manually using either DumpRenderTree or HaikuLauncher
(both accept an URL from the command line). For tests that require the page to
be served over http (and not directly read from a file), you need an HTTP server.
Install the lighttpd package and run:

    Tools/Scripts/new-run-webkit-httpd --server start

This will start lighttpd with the appropriate setting file, allowing you to run
the tests. The server listens on port 8000 by default.

### WebKit2 ###

Same as above, but:

    $ python Tools/Scripts/run-webkit-tests --platform=haiku-wk2 --no-build \
        --no-retry-failures --clobber-old-results --no-new-test-results

Note that this is currently not working.

### Others ###

There are more tests, but the build-\* scripts must be working before we can run them.

## Status of WebKit2 port ##

The Haiku port currently uses the WebKitLegacy API. Eventually we should move to
WebKit2 (simply called WebKit in the sources). WebKit2 splits the web engine into
multiple processes: an user interface, a web process, a network process, etc. This
allows for better sandboxing, and better stability (the user interface will not
crash or freeze when it hits a problematic website).

The work on WebKit2 is found in the GSoC2019 tag. It has not been updated since
and the internals of WebKit have changed a bit. An attempt to rebase it is found
in the webkit2 branch, but it's completely broken. The best thing to do is
probably to enable webkit2 in the current rebased branch, see what breaks, and
cherry-pick the relevant changes from the GSoC2019 tag (and the commit history leading to it).

## Notes ##

cmake is smart enough to detect when a variable has changed and will rebuild everything.
You can keep several generated folders with different settings if you need to switch
between them very often (eg. debug and release builds). Just invoke the build-webkit
script with different settings and different output dirs. You can then run make 
(or ninja) from each of these folders.

You can copy a WebPositive binary from your Haiku installation into the
WebKitBuild/Release folder. Launching it will then use the freshly built
libraries instead of the system ones. It is a good idea to test this because
HaikuLauncher doesn't use tabs, which sometimes expose different bugs.

This document was last updated August 13, 2020.

Authors: Maxime Simon, Alexandre Deckner, Adrien Destugues
