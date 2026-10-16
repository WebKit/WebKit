# WKC Wii U port integration status

This directory builds the modern WebKit tree against the WKC Wii U platform layer. Files carried forward from `WebKit_WiiU4.3` are not automatically buildable on current upstream APIs; do not add every legacy `WKC/` folder to a recursive glob.

## Active integration

- `Source/WebCore/platform/WKC/`, `platform/graphics/WKC/`, `platform/audio/WKC/`, `platform/sql/WKC/`, `platform/text/WKC/` and `platform/mediastream/WKC/` are selected explicitly by `CMakeLists.txt`.
- The modern in-process cURL bridge is `Source/WebCore/platform/WKC/ResourceHandleWKC.cpp`; the older networking backend under `WebCore/platform/network/WKC/` was replaced rather than copied wholesale.
- Wii U low-level overrides live under `Source/WTF/wtf/wiiu/`. The old `wtf/WKC/` include/source path was stale; current CMake now uses `wtf/wiiu/`.
- Text support uses current upstream WTF/ICU implementations with `TextEncodingDetectorWKC.cpp`; the old `TextCodecWKC` and `TextBreakIteratorWKC` files target retired interfaces.
- MHTML parsing/loading comes from modern WebCore's `MHTMLArchive.cpp` and `MHTMLParser.cpp`. The old `MHTMLWebArchiveWKC.cpp` is a legacy page-save implementation behind `ENABLE(WKC_PAGESAVE_MHTML)`, not a drop-in replacement.
- The standardized Gamepad API bridge is in `Source/WebKit/WKC/webkit/WKCWebView.cpp`. The old custom Wii U GamePad DOM class is marked deprecated.

## Legacy sources that need porting, not blind inclusion

- `Source/WebCore/html/shadow/WKC/`: old media-control classes use pre-modern WebCore APIs; current media controls and the WKC render theme remain the modern source of truth.
- `Source/WebCore/page/WKC/`: old drag and event-handler overrides target older class interfaces and should not be linked alongside modern implementations without API-level porting.
- `Source/WebKit/WKC/wiiu/WebCore/` and `Source/WebKit/WKC/wiiu/WKC/private/`: older custom DOM bindings and peer callbacks. Their IDLs are not part of the modern DOM-binding generation flow and must be ported and registered before use.
- The original WebGL files such as `GraphicsContext3DWKC.cpp` target the old `GraphicsContext3D` API. The modern tree has no compatible WKC `GraphicsContextGLWKC.cpp` backend, so WebGL now defaults off; setting `WKC_ENABLE_WEBGL=ON` fails configuration until that backend is added.
- The legacy `URLWKC.cpp` was removed because it duplicated modern `WTF::URL::fileSystemPath()` with a stub returning an empty path; the existing `Source/WTF/wtf/URL.cpp` implementation is the correct shared implementation.

## Known incomplete functionality

`RealtimeMediaSourceCenterWKC.cpp` currently wires capture-device enumeration and track lifecycle, but its own implementation notes that video-frame and audio-sample delivery are not connected yet. Media capture therefore must not be treated as console-verified just because its feature flags compile. Completing it requires real frame/sample callbacks and a Wii U runtime test.

The basic WKC library build, full-WebCore build, and WebRTC build are separate CI paths. Passing one does not imply the full browser engine or live Wii U features are production-ready.
