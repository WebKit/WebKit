Title: Overview
Slug: overview

# Overview

WPEPlatform is a GObject library that abstracts the platform layer for
WPE WebKit. It is the successor to [libwpe](https://github.com/WebKit/libwpe)
and the various out-of-tree backends written against it (notably
[WPEBackend-fdo](https://github.com/Igalia/WPEBackend-fdo)), and it
lives upstream in the WebKit tree under `Source/WebKit/WPEPlatform/`.

A platform implementation provides a connection to a native windowing
system (Wayland, DRM/KMS, X11, a custom compositor, etc.) and the
machinery WPE WebKit needs to render web content into a surface and
deliver input events back to it. WPEPlatform defines that contract as a
small set of GObject-based abstract classes that implementations
subclass.

WPEPlatform ships with three built-in platform implementations —
**Wayland**, **DRM**, and **headless** — each exposed as an
independently consumable library with its own `pkg-config` module. They
are built by default but each one is individually optional at build
time. Integrators are expected to choose between using a built-in
implementation, subclassing one to extend it, or writing a new
implementation from scratch. External implementations for other
windowing systems already exist (notably for GTK4 and SDL).

<!-- TODO: uncomment once backend-model.md has landed:
See [Backend model](backend-model.html) for how WPEPlatform discovers external modules.
-->

WPEPlatform is used **only from WPE WebKit's UI process**. The web
process never touches the native display directly — it renders into
offscreen buffers shared with the UI process through a platform-specific
buffer-sharing mechanism (such as DMA-BUF or AHardwareBuffer). WebKit's
IPC layer is used to notify the UI process when a buffer is created,
destroyed, or updated; the UI process then hands the buffer to the
platform via [method@View.render_buffer].

<!-- TODO: uncomment once rendering-model.md has landed:
See [Rendering model](rendering-model.html) for the buffer flow in detail.
-->

## Audience

WPEPlatform has three distinct kinds of consumer. The same API serves
all three, but the surface each one uses is different — and recognising
this up front makes the rest of the documentation easier to navigate.

**Browser-application developers.** Most browser applications never
need to call into WPEPlatform directly. Creating a `WebKitWebView`
without specifying a backend will pick a platform automatically and the
application can then work entirely against the existing `WebKitWebView`
API. WPEPlatform becomes visible when the application wants to do things
that the WebKit-level API does not expose — for example, attach custom
keyboard shortcuts to a [class@View], change cursor appearance, or pin
the application to a specific built-in platform implementation.

<!-- TODO: when backend-model.md has landed, restore "(see [Backend model](backend-model.html))" after "pick a platform automatically" above.
-->

**Platform implementers.** Integrators bringing up WPE WebKit on new
hardware or new window systems subclass [class@Display], [class@View],
and [class@Toplevel] (and optionally [class@Keymap],
[class@InputMethodContext], [class@Screen], and so on) to provide the
glue between WPE WebKit and the native platform. The implementation can
live in the embedder's source tree, or be installed as a module that
WPEPlatform discovers and loads at runtime via a GIO extension point.

**WebKit itself.** WPE WebKit's UI process consumes the API to drive
the platform: it asks the display for a view, hands the view the
buffers it produces, listens for input events, and watches the
toplevel's state. As a documentation reader you usually do not need to
know which APIs are used by WebKit internally, but the headers
sometimes contain both a request-style function (e.g.
[method@View.lock_pointer]) and a notification-style function meant for
platform implementations to call back into WebKit (e.g.
[method@View.event]).

<!-- TODO: uncomment once input-handling.md has landed:
Where the distinction matters it is called out in the [Input handling](input-handling.html) concept page.
-->

## Class hierarchy

The core of the API revolves around four abstract classes:

| Class | Role |
|---|---|
| [class@Display] | The connection to a platform — singleton-like, the root of every other object. |
| [class@Toplevel] | A native top-level window (or its conceptual equivalent on platforms that don't have windows). |
| [class@View] | A rendering surface for a single web view, normally hosted inside a toplevel. |
| [class@Buffer] | A piece of pixel data produced by WebKit and handed to the view to be rendered. |

Around them are helper and support classes — [class@Screen],
[class@Settings], [class@Keymap], [struct@Event], [class@Clipboard],
[class@InputMethodContext], [iface@GestureController],
[class@GamepadManager] / [class@Gamepad], [iface@ViewAccessible] (an
interface) — and concrete [class@Buffer] subclasses
([class@BufferDMABuf], [class@BufferSHM], `WPEBufferAndroid`).

<!-- TODO: uncomment once displays-and-views.md has landed:
The [Displays and views](displays-and-views.html) concept page expands
on how these objects relate to each other, who creates whom, and which
lifecycles you can rely on.
-->

## How a browser application uses WPEPlatform

The minimum viable usage looks like this:

1. Create a [class@WebKit.WebView] without specifying a display or a
   backend. WebKit then uses the default display automatically — it is
   obtained by iterating the registered platform modules in priority
   order and connecting to the first one that succeeds.
2. Run the GLib main loop.

WebKit creates the [class@Display], [class@Toplevel], and [class@View]
on the application's behalf in this default flow, so a browser
application does not need to touch the WPEPlatform API at all to get a
window on screen. An application only needs a [class@Display] of its
own when it wants to use a non-default one — for example, to pin to a
specific built-in implementation by instantiating it directly with
`wpe_display_wayland_new()` — in which case it passes the connected
display to the web view on construction.

<!-- TODO: uncomment once tutorial-browser.md has landed:
The [Hello browser tutorial](tutorial-browser.html) walks through both paths.
-->

## How a platform implementation uses WPEPlatform

A platform implementation subclasses [class@Display] and overrides its
virtual methods — at minimum [vfunc@Display.connect] and
[vfunc@Display.create_view] — and
similarly subclasses [class@View] and [class@Toplevel] for the
platform-specific behaviour. It optionally subclasses [class@Keymap],
[class@Screen], [class@InputMethodContext], and others depending on the
features the platform supports.

The implementation can be:

- **Built as a module** and installed under `${LIB_INSTALL_DIR}/wpe-platform-${WPE_API_VERSION}/modules/`, in which case [func@Display.get_default] will pick it up automatically via the `wpe-platform-display` GIO extension point.
- **Linked directly** by the embedder, in which case the embedder instantiates the display class explicitly.

<!-- TODO: uncomment once tutorial-platform.md and backend-model.md have landed:
The [Writing a platform tutorial](tutorial-platform.html) walks through
implementing a minimal backend. The [Backend model](backend-model.html)
concept page describes how WPEPlatform discovers and loads modules.
-->

## Relationship to libwpe

libwpe and its out-of-tree backends remain available but are
deprecated. WPE WebKit can still be built against them while
applications and platform integrators migrate, but new code should
target WPEPlatform.

<!-- TODO: uncomment once migration-mapping.md and migrating-from-libwpe.md have landed (restore the two entries below as a bulleted list):
The migration table at [migration-mapping](migration-mapping.html) lists every libwpe and WPEBackend-fdo public symbol and points at the WPEPlatform equivalent (or marks it as gone).
[Migrating from libwpe](migrating-from-libwpe.html) is a hands-on guide with before/after code for the common patterns.
-->

## What is *not* covered by WPEPlatform

A few things that lived in libwpe / WPEBackend-fdo do not have direct
WPEPlatform equivalents:

- **Process management** (libwpe's `wpe_process_provider_*` API, added in 1.14). Child-process launch is once again handled internally by WPE WebKit.
- **The `renderer-host`/`renderer-backend-egl` plumbing**. The new rendering model is built on buffer sharing through [class@Buffer] subclasses; there is no separate EGL renderer-target abstraction to wire up.
- **WPEBackend-fdo's "exportable" view backend**. The "WebKit hands you rendered buffers via callbacks" pattern is replaced by subclassing [class@View] and implementing [vfunc@View.render_buffer].

<!-- TODO: resolve before publishing — depends on the audio / video-plane
successor decision, and on the migration guide landing:
The extensions/audio.h (wpe_audio_source / wpe_audio_receiver) and
extensions/video-plane-display-dmabuf.h APIs from WPEBackend-fdo have no
visible counterpart in WPEPlatform headers. Confirm whether they have moved
into the WebKit-level API, are still missing, or are intentionally dropped.
-->
