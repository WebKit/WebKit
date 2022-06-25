# WebRTC Obj-C SDK

This directory contains the Obj-C SDK for WebRTC. This includes wrappers for the
C++ PeerConnection API and some platform specific components for iOS and macOS.

## Organization

- api/

  Wrappers around classes and functions in the C++ API for creating and
  configuring peer connections, etc.

- base/

  This directory contains some base protocols and classes that are used by both
  the platform specific components and the SDK wrappers.

- components/

  These are the platform specific components. Contains components for handling
  audio, capturing and rendering video, encoding and decoding using the
  platform's hardware codec implementation and for representing video frames
  in the platform's native format.

- helpers/

  These files are not WebRTC specific, but are general helper classes and
  utilities for the Cocoa platforms.

- native/

  APIs for wrapping the platform specific components and using them with the
  C++ API.

- unittests/

  This directory contains the tests.
