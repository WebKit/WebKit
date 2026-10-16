# WKC Wii U port integration status

## Porting rule

Modern upstream WebKit owns shared algorithms and standard behavior. WKC code is the Wii U platform adapter: it connects those modern interfaces to Wii U-specific services, hardware, filesystem conventions, input, rendering, audio and media APIs.

A legacy WKC file must **not** be removed just because its API is old. First identify the current upstream abstraction, then port the Wii U-specific behavior to that interface and update build registration. Do not compile old code unchanged when it depends on removed APIs, but do not discard its platform behavior either. Remove a file only when inspection shows it is genuinely obsolete, redundant with a modern implementation, or has no useful implementation—and record the evidence.

For each ported area:
1. Find the modern interface and its existing upstream implementation.
2. Compare the legacy WKC implementation to identify Wii U-specific behavior that still needs to be preserved.
3. Move/adapt only the platform-specific pieces to the modern interface; reuse modern common logic instead of forking it.
4. Update source lists, feature flags and generated bindings as required.
5. Validate with the relevant build/test and, for hardware-facing code, an on-console test when possible.

## Active integration

- `Source/WebCore/platform/WKC/`, `platform/graphics/WKC/`, `platform/audio/WKC/`, `platform/sql/WKC/`, `platform/text/WKC/` and `platform/mediastream/WKC/` contain the current WKC platform adapters selected by `CMakeLists.txt`.
- The current cURL bridge is `Source/WebCore/platform/WKC/ResourceHandleWKC.cpp`. Review the older network sources for any Wii U-specific behavior not yet represented before treating the port as complete.
- Wii U low-level WTF adapters live under `Source/WTF/wtf/wiiu/`; CMake must select this current path.
- Text handling currently reuses modern WTF/ICU implementations plus `TextEncodingDetectorWKC.cpp`. Older codec/break-iterator files use retired interfaces and should be evaluated for missing Wii U-specific behavior before being considered fully replaced.
- Modern WebCore owns MHTML parsing/loading. The legacy `MHTMLWebArchiveWKC.cpp` contains page-save behavior and is not automatically a substitute for modern MHTML parsing; determine whether that Wii U page-save behavior is still required.
- The standardized Gamepad API should use modern WebCore APIs, with `WKCWebView.cpp` serving as the Wii U event bridge. Preserve any necessary Wii U controller mapping while avoiding a second, obsolete DOM implementation.

## Items that still need modern-interface work

- The legacy `html/shadow/WKC/` and `page/WKC/` directories use older WebCore interfaces. Do not bulk-add them to the build, but inspect each file for platform behavior that current upstream code does not provide and port that behavior where needed.
- The older `Source/WebKit/WKC/wiiu/WebCore/` and `.../WKC/private/` code contains platform callbacks and custom bindings. These need to be matched to modern DOM/binding APIs; the fact that they are not currently registered does not by itself prove their functionality is unnecessary.
- The Wii U graphics sources such as `GraphicsContext3DWKC.cpp` target the retired `GraphicsContext3D` API. They need a port to the current `GraphicsContextGL` abstraction, preserving the Wii U rendering backend. There is currently no `GraphicsContextGLWKC.cpp`, so `WKC_ENABLE_WEBGL` defaults off and explicitly enabling it fails until that adapter exists. This is a temporary missing implementation, not a decision that WebGL is obsolete.
- `RealtimeMediaSourceCenterWKC.cpp` currently wires capture-device enumeration and track lifecycle, but video-frame/audio-sample delivery is not connected. This requires modern callback wiring and runtime validation.

## The URLWKC.cpp deletion

The removed `Source/WebCore/platform/WKC/URLWKC.cpp` did not contain a Wii U filesystem-path implementation: its `URL::fileSystemPath()` only called `notImplemented()` and returned an empty string. Modern `WTF::URL` already implements the shared URL-to-path behavior, so retaining that stub would override useful upstream behavior rather than adapt it. This is a narrowly evidenced obsolete stub—not a general reason to remove legacy WKC files.

The shared implementation still needs validation against Wii U `file:` URL and mount-path conventions. If the console needs special handling, add it as a modern-compatible Wii U adaptation instead of restoring the empty stub.

A successful compile only proves the compiled path builds. It does not establish that the full browser engine or hardware-facing features work correctly on console.
