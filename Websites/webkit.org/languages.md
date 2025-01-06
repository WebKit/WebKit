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

We require a minimum of Python 3.9.
