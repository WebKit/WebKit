**NOTE:** As of Xcode 26, content-based caching is built in to LLVM. WebKit
enables it via the `COMPILATION_CACHE_ENABLE_CACHING` build setting and emits
cache hit metrics in build logs. Extra diagnostics are available with
`COMPILATION_CACHE_ENABLE_DIAGNOSTIC_REMARKS=YES`.

# Prerequisites

- Xcode < 26 (or opt out of the default compilation caching behavior).
- ```ccache(1)``` installed in /usr/local/bin

# Configuring ccache

The maximum cache size is 5GB by default, but a WebKit Debug build can require 20GB or more:

- ```ccache --max-size=20G```

# Building with ccache

- ```make ARGS="WK_USE_CCACHE=YES"```
- ```build-webkit WK_USE_CCACHE=YES```
- Build in the Xcode UI by adding ```WK_USE_CCACHE = YES;``` to Tools/ccache/ccache.xcconfig (FIXME: this dirties the working directory).

# Viewing cache statistics

- ```ccache -s```
