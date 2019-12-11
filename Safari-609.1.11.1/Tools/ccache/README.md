# Prerequisites

- Xcode
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
