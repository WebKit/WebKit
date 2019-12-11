# Build overrides in GN

This directory is used to allow us to customize variables that differ between
WebRTC being built as standalone and as a part of Chromium.

There's another build_overrides in Chromium that needs to contain the same
set of files with the same set of variables (but with different values).
