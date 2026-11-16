
# SFrame

This repository contains an implementation of [the draft SFrame
standard](https://datatracker.ietf.org/doc/html/draft-omara-sframe) for
end-to-end media encryption.  Since the spec is still in progress, the
implementation here might not match exactly.  These differences will resolve as
the specification and this implementation evolve together.

## Building and Running Tests

A convenience Makefile is included to avoid the need to remember a bunch of
CMake parameters.

```
> make        # Configures and builds the library 
> make dev    # Configure a "developer" build with tests and checks
> make test   # Builds and runs tests
> make format # Runs clang-format over the source
```

## Prerequisites

You need openssl 1.1 or greater installed, C++ compiler, make, and cmake.  To
run tests, you will need the doctest framework and Niels Lohmann's json library.
To automatically format the code, you will need clang-format.

Here is an example command to install these packages on Linux Ubuntu:
```
sudo apt update && sudo apt-get -y install clang clang-format clang-tidy cmake doctest-dev libssl-dev nlohmann-json3-dev
```
