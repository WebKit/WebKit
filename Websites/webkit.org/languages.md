# WebKit programming languages

## C++

Most WebKit code is written in C++. But a reasonable question is, “Which version C++?”

It’s C++23.

The relevant compilers are versions of [Clang](https://clang.llvm.org) and [GCC](https://gcc.gnu.org).
The WebKit project has [a document explaining which version of GCC is required](https://docs.webkit.org/Ports/WebKitGTK%20and%20WPE%20WebKit/GCCRequirement.html).

## Scripts

The preferred language for WebKit project scripts is Python.
We have legacy scripts in Perl and Ruby, but we ask that new scripts be done in Python.

### Python

We require a minimum of Python 3.9.
