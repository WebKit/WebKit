# Prerequisites

- Xcode
- ```sccache(1)``` installed in /usr/local/bin

# Building with sccache

- ```make ARGS="WK_USE_SCCACHE=YES"```
- ```build-webkit WK_USE_SCCACHE=YES```
- Build in the Xcode UI by adding ```WK_USE_SCCACHE = YES;``` to Tools/sccache/sccache.xcconfig (FIXME: this dirties the working directory).

# Configuring sccache

The maximum cache size is 10GB by default, but a WebKit Debug build can require 20GB or more:

export SCCACHE_CACHE_SIZE="50G

# Viewing cache statistics

- ```sccache -s```
