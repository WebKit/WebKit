# WebKit programming languages

## C++

Most WebKit code is written in C++. But a reasonable question is, “Which version C++?”

It’s C++17.
We eventually plan to move forward to C++20 and beyond.
Before we do that we make sure that compilers and libraries
on the platforms that WebKit contributors are working on are ready.

The relevant compilers are versions of clang, gcc, and Visual Studio.
The WebKit project has [a document explaining which version of gcc is required](https://trac.webkit.org/wiki/WebKitGTK/GCCRequirement).

## Scripts

The preferred language for WebKit project scripts is Python.
We have legacy scripts in Perl and Ruby, but we ask that new scripts be done in Python.

### Python

Some WebKit scripts are in Python 3, others in Python 2. What’s the policy on that?

#### Scripts used for building and testing WebKit

This includes scripts like `run-webkit-tests` and `build-webkit`.

All of these scripts must be written so that they are compatible with both Python 2 and Python 3.
Some ports build on environments that have only Python 2, including the internal Apple build
environment for older Apple OS versions such as macOS Movaje.
Other ports build on environments that have only Python 3.

At some point in the future the Python 2 requirement will be relaxed.

#### Scripts used for developing WebKit

This includes scripts like `configure-xcode-for-embedded-development` and `lint-test-expectations`.

These scripts must be compatible with Python 3,
and do not need to be compatible with Python 2.
While some WebKit development is done on systems
that don’t have Python 3 installed, the vast majority is not
and so we’re not requiring Python 2 compatibility for new scripts
that aren’t part of building and testing.

#### Which version of Python 3?

At the moment, the practical minimum version of Python 3 is Python 3.7.3.
Most platforms have newer versions, and that is the version in included with macOS Catalina.
