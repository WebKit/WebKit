This documentation explains how to compile, install & run Capstone on MacOSX,
Linux, *BSD & Solaris. We also show steps to cross-compile for Microsoft Windows.

To natively compile for Windows using Microsoft Visual Studio, see COMPILE_MSVC.TXT.

To compile using CMake, see COMPILE_CMAKE.TXT.

To compile using XCode on MacOSX, see xcode/README.md.

To compile for Windows CE (a.k.a, Windows Embedded Compact), see windowsce/COMPILE.md.

                        *-*-*-*-*-*

Capstone requires no prerequisite packages, so it is easy to compile & install.



(0) Tailor Capstone to your need.

  Out of 12 archtitectures supported by Capstone (Arm, Arm64, M68K, Mips, PPC,
  Sparc, SystemZ, XCore, X86, M680X, TMS320C64x & EVM), if you just need several
  selected archs, choose the ones you want to compile in by editing "config.mk"
  before going to next steps.

  By default, all 12 architectures are compiled.

  The other way of customize Capstone without having to edit config.mk is to
  pass the desired options on the commandline to ./make.sh. Currently,
  Capstone supports 5 options, as followings.

  - CAPSTONE_ARCHS: specify list of architectures to compiled in.
  - CAPSTONE_USE_SYS_DYN_MEM: change this if you have your own dynamic memory management.
  - CAPSTONE_DIET: use this to make the output binaries more compact.
  - CAPSTONE_X86_REDUCE: another option to make X86 binary smaller.
  - CAPSTONE_X86_ATT_DISABLE: disables AT&T syntax on x86.
  - CAPSTONE_STATIC: build static library.
  - CAPSTONE_SHARED: build dynamic (shared) library.

  By default, Capstone uses system dynamic memory management, both DIET and X86_REDUCE
  modes are disable, and builds all the static & shared libraries.

  To avoid editing config.mk for these customization, we can pass their values to
  make.sh, as followings.

  $ CAPSTONE_ARCHS="arm aarch64 x86" CAPSTONE_USE_SYS_DYN_MEM=no CAPSTONE_DIET=yes CAPSTONE_X86_REDUCE=yes ./make.sh

  NOTE: on commandline, put these values in front of ./make.sh, not after it.

  For each option, refer to docs/README for more details.



(1) Compile from source

  On *nix (such as MacOSX, Linux, *BSD, Solaris):

  - To compile for current platform, run:

		$ ./make.sh

  - On 64-bit OS, run the command below to cross-compile Capstone for 32-bit binary:

		$ ./make.sh nix32



(2) Install Capstone on *nix

  To install Capstone, run:

	$ sudo ./make.sh install

	For FreeBSD/OpenBSD, where sudo is unavailable, run:

		$ su; ./make.sh install

  Users are then required to enter root password to copy Capstone into machine
  system directories.

  Afterwards, run ./tests/test* to see the tests disassembling sample code.


  NOTE: The core framework installed by "./make.sh install" consist of
  following files:

	/usr/include/capstone/capstone.h
	/usr/include/capstone/x86.h
	/usr/include/capstone/arm.h
	/usr/include/capstone/arm64.h
	/usr/include/capstone/evm.h
	/usr/include/capstone/m68k.h
	/usr/include/capstone/m680x.h
	/usr/include/capstone/mips.h
	/usr/include/capstone/ppc.h
	/usr/include/capstone/sparc.h
	/usr/include/capstone/systemz.h
	/usr/include/capstone/tms320c64x.h
	/usr/include/capstone/xcore.h
	/usr/include/capstone/platform.h
	/usr/lib/libcapstone.so (for Linux/*nix), or /usr/lib/libcapstone.dylib (OSX)
	/usr/lib/libcapstone.a



(3) Cross-compile for Windows from *nix

  To cross-compile for Windows, Linux & gcc-mingw-w64-i686 (and also gcc-mingw-w64-x86-64
  for 64-bit binaries) are required.

	- To cross-compile Windows 32-bit binary, simply run:

		$ ./make.sh cross-win32

	- To cross-compile Windows 64-bit binary, run:

		$ ./make.sh cross-win64

  Resulted files libcapstone.dll, libcapstone.dll.a & tests/test*.exe can then
  be used on Windows machine.



(4) Cross-compile for iOS from Mac OSX.

  To cross-compile for iOS (iPhone/iPad/iPod), Mac OSX with XCode installed is required. 

	- To cross-compile for ArmV7 (iPod 4, iPad 1/2/3, iPhone4, iPhone4S), run:
		$ ./make.sh ios_armv7

	- To cross-compile for ArmV7s (iPad 4, iPhone 5C, iPad mini), run:
		$ ./make.sh ios_armv7s

	- To cross-compile for Arm64 (iPhone 5S, iPad mini Retina, iPad Air), run:
		$ ./make.sh ios_arm64

	- To cross-compile for all iDevices (armv7 + armv7s + arm64), run:
		$ ./make.sh ios

  Resulted files libcapstone.dylib, libcapstone.a & tests/test* can then
  be used on iOS devices.



(5) Cross-compile for Android

  To cross-compile for Android (smartphone/tablet), Android NDK is required.
  NOTE: Only ARM and ARM64 are currently supported.

	$ NDK=/android/android-ndk-r10e ./make.sh cross-android arm
  or
	$ NDK=/android/android-ndk-r10e ./make.sh cross-android arm64

  Resulted files libcapstone.so, libcapstone.a & tests/test* can then
  be used on Android devices.



(6) Compile on Windows with Cygwin

  To compile under Cygwin gcc-mingw-w64-i686 or x86_64-w64-mingw32 run:

        - To compile Windows 32-bit binary under Cygwin, run:

                $ ./make.sh cygwin-mingw32

        - To compile Windows 64-bit binary under Cygwin, run:

                $ ./make.sh cygwin-mingw64

  Resulted files libcapstone.dll, libcapstone.dll.a & tests/test*.exe can then
  be used on Windows machine.



(7) By default, "cc" (default C compiler on the system) is used as compiler.

	- To use "clang" compiler instead, run the command below:

		$ ./make.sh clang

	- To use "gcc" compiler instead, run:

		$ ./make.sh gcc



(8) To uninstall Capstone, run the command below:

		$ sudo ./make.sh uninstall



(9) Language bindings

  So far, Python, Ocaml & Java are supported by bindings in the main code.
  Look for the bindings under directory bindings/, and refer to README file
  of corresponding languages.

  Community also provide bindings for C#, Go, Ruby, NodeJS, C++ & Vala. Links to
  these can be found at address http://capstone-engine.org/download.html
