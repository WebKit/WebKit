Title: Writing a WPE platform implementation
Slug: tutorial-platform

WPE WebKit renders web content, but it does not talk to the underlying
platform on its own. That job belongs to a *platform implementation*: a
small set of GObject subclasses that connect WebKit to a display
server, a compositor, a KMS device — whatever your platform provides.
This tutorial walks through what a platform implementation does,
concept by concept, roughly in the order you would build one.

The contract is three classes:

- [class@Display] — the connection to the platform, and the factory
  that creates the other objects.
- [class@Toplevel] — the surface a view is presented in; on a windowed
  platform, a native window.
- [class@View] — renders WebKit's content into a toplevel and receives
  input for it.

Most platform implementations target a *windowed* system; Wayland is
the common case on Linux. A windowless, offscreen implementation — like
the in-tree headless one — is possible too, but it is the exception.
Throughout this page each concept is illustrated with whichever
built-in implementation shows it most clearly, usually **Wayland** and
occasionally **headless**, pointing to each one's full source for the
complete code. The out-of-tree
[GTK implementation](https://github.com/Igalia/wpe-platform-gtk) is a
good example of a real, non-trivial platform built entirely on the
public API.

A platform implementation is made available in one of two ways: as a
**loadable module** that WebKit discovers automatically, or linked
directly into an application that constructs the [class@Display]
itself. The module is optional — this page focuses on writing the
implementation; how WebKit discovers modules at runtime is a separate
topic.
<!-- FIXME: link backend-model.html (discovery) and overview.html model once available/landed -->

The conceptual model behind these classes is introduced in
[Overview](overview.html). The examples here use the public
`wpe-platform-2.0` API, in C.

If you are porting an existing WPEBackend-fdo backend, the "exportable"
callback set (`export_fdo_egl_image` and friends) collapses into the
single [vfunc@View.render_buffer] below, and the rest of the fdo surface
maps onto the classes described here. The [Migration mapping
table](migration-mapping.html) pairs each old symbol with its WPEPlatform
equivalent.

## The display: connecting and creating objects

The [class@Display] is the entry point. It opens the connection to the
platform, creates [class@View] and [class@Toplevel] instances on
request, and optionally exposes extras such as an `EGLDisplay`, a
[class@Keymap], or the set of [class@Screen]s.

You implement it as a GObject subclass. Declaring and registering
GObject types is standard GLib boilerplate — see the
[GObject documentation](https://docs.gtk.org/gobject/) if it is
unfamiliar — so only the WPE-specific parts are shown here.

Two vfuncs are mandatory. [vfunc@Display.connect] opens the connection
to the platform:

```c
static gboolean
my_display_connect (WPEDisplay *display, GError **error)
{
    // Connect to the native platform (a compositor, a device, ...).
    // On failure, set error and return FALSE.
    return TRUE;
}
```

[vfunc@Display.create_view] returns a new [class@View] tied to the
display; implementations create their own subclass:

```c
static WPEView *
my_display_create_view (WPEDisplay *display)
{
    return WPE_VIEW (g_object_new (MY_TYPE_VIEW, "display", display, NULL));
}
```

You wire these — and any optional vfuncs — into the class in the usual
`class_init`.

Beyond the mandatory pair, [struct@DisplayClass] declares a number of
optional slots, each backing a capability your platform may or may not
have:

- [vfunc@Display.create_toplevel] — create a [class@Toplevel]
  subclass. Strongly recommended: a display that hands out views but no
  toplevels is of limited use.
- [vfunc@Display.get_egl_display] — an `EGLDisplay` for
  hardware-accelerated rendering.
- [vfunc@Display.get_keymap], [vfunc@Display.get_clipboard],
  [vfunc@Display.create_input_method_context],
  [vfunc@Display.create_gamepad_manager] — keyboard, clipboard, input
  method, and gamepad support. A display that provides no keymap gets a
  fallback XKB one automatically, and one that provides no gamepad
  manager gets a libmanette-based one.
- [vfunc@Display.get_n_screens] / [vfunc@Display.get_screen] — the
  monitors the platform exposes (see
  [The display's screens](#the-displays-screens)).
- [vfunc@Display.get_preferred_buffer_formats],
  [vfunc@Display.use_explicit_sync], and [vfunc@Display.get_drm_device] —
  buffer format negotiation, explicit-sync support, and the DRM device
  used for rendering. These are a rendering concern, independent of
  whether the platform has monitors: even an offscreen implementation
  has to agree on buffer formats.

Anything left unset falls back to a sensible default.

When a vfunc fails, populate the `GError` with the [error@DisplayError]
domain and an appropriate code, such as
`WPE_DISPLAY_ERROR_NOT_SUPPORTED` or
`WPE_DISPLAY_ERROR_CONNECTION_FAILED`:

```c
g_set_error (error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED,
             "Could not connect to the display server");
```

## The toplevel: the surface

A [class@Toplevel] is the surface a view is presented in — a native
window on a windowed platform, where most of the work is: tracking
size, reporting state (active, fullscreen, maximized), setting a title,
and forwarding every change back to WebKit. A windowless platform still
has a toplevel; it just has no real window behind it.

Your toplevel overrides the vfuncs for the capabilities the platform
supports — [vfunc@Toplevel.resize], [vfunc@Toplevel.set_fullscreen],
[vfunc@Toplevel.set_maximized], [vfunc@Toplevel.set_title] — and leaves
the rest unset. On Wayland these map onto xdg-shell requests; a
windowless implementation has no real window, so most of them become
state-only or no-ops.

Whenever the toplevel's size or state changes, tell WebKit with
[method@Toplevel.resized] and [method@Toplevel.state_changed]:

```c
wpe_toplevel_resized (toplevel, width, height);
wpe_toplevel_state_changed (toplevel,
    WPE_TOPLEVEL_STATE_FULLSCREEN | WPE_TOPLEVEL_STATE_ACTIVE);
```

A toplevel hosts one or more views. Simple platforms allow a single
view that always fills the toplevel, so resizing the toplevel maps
directly to resizing that one view. Platforms with window chrome — a
menu bar, decorations — or several views per window (as the GTK
implementation allows) do not have that one-to-one relationship, and
must position and size their views themselves —
[method@Toplevel.foreach_view] iterates the views a toplevel hosts.

## The view: rendering

The [class@View] is where WebKit's content is rendered and where input
arrives. Its central vfunc is [vfunc@View.render_buffer]: WebKit hands
the view a [class@Buffer], the view presents it onto the toplevel's
surface, and then reports back.

```c
static gboolean
my_view_render_buffer (WPEView *view, WPEBuffer *buffer,
                       const WPERectangle *damage_rects, guint n_damage_rects,
                       GError **error)
{
    if (WPE_IS_BUFFER_DMA_BUF (buffer)) {
        EGLImage image = wpe_buffer_import_to_egl_image (buffer, error);
        // ...present the EGLImage on the toplevel's surface...
    } else if (WPE_IS_BUFFER_SHM (buffer)) {
        // ...blit the pixel data from WPE_BUFFER_SHM (buffer)...
    }
    return TRUE;
}
```

WebKit produces a [class@BufferDMABuf] or a [class@BufferSHM]; dispatch
on the concrete type. [method@Buffer.import_to_egl_image] turns a DMA-BUF
into an `EGLImage` for hardware-accelerated presentation.

The reporting is a two-step lifecycle, and the distinction between the
two steps matters:

- [method@View.buffer_rendered] — the buffer has been presented. The
  frame is now on screen, but the buffer may still be in use (held by
  the compositor, queued for scanout), so it must not be reused yet.
- [method@View.buffer_released] — the buffer is no longer needed, and
  WebKit may reuse or destroy it.

A typical implementation presents the buffer in `render_buffer`, calls
`buffer_rendered` once it is committed, and calls `buffer_released`
later, when the platform signals the buffer is free — a Wayland
`wl_buffer` release, a KMS page-flip completing on the next frame, and
so on.

On an explicit-sync platform the buffer carries fences instead of
blocking. Wait on its rendering fence
([method@Buffer.take_rendering_fence]) before reading the buffer, and
attach a release fence with [method@Buffer.set_release_fence] before
`buffer_released`, so WebKit holds off reusing the buffer until your
presentation completes. The [vfunc@Display.use_explicit_sync] slot
advertises the capability.

### Visibility, focus, and geometry

A view has two related but distinct notions of visibility:

- **visible** ([method@View.get_visible]) — whether the view is *meant*
  to be shown. This can be `TRUE` even when nothing is on screen, for
  example while its toplevel is minimized.
- **mapped** ([method@View.get_mapped]) — whether the view is *actually*
  being presented right now: visible *and* not hidden for another
  reason, such as its toplevel being minimized.

Your implementation drives the mapped state by calling [method@View.map]
and [method@View.unmap] as those conditions change; WebKit uses it to
pause and resume rendering. Report geometry changes with
[method@View.resized]. When one view fills its toplevel, keeping the two
in sync just means resizing the view whenever the toplevel resizes.

Report keyboard focus the same way: call [method@View.focus_in] when the
platform gives the view focus and [method@View.focus_out] when it loses
it. The Wayland implementation drives these from the `wl_seat` keyboard
enter/leave events.

## Input

Input is central to a windowed platform, and it flows the opposite way
from rendering: the implementation receives events from the system and
delivers them to the view. You build a [struct@Event] with one of the
typed constructors — [ctor@Event.keyboard_new],
[ctor@Event.pointer_button_new], [ctor@Event.pointer_move_new],
[ctor@Event.scroll_new], [ctor@Event.touch_new] — and hand it to the
view with [method@View.event]:

```c
g_autoptr(WPEEvent) event =
    wpe_event_keyboard_new (WPE_EVENT_KEYBOARD_KEY_DOWN, view, /* ... */);
wpe_view_event (view, event);
```

Multi-touch is delivered as one event per touch point, each tagged with a
sequence id ([method@Event.touch_get_sequence_id]) so WebKit can track
individual points across their lifetime.

Key events are interpreted through a [class@Keymap]; if your display
provides none, WebKit falls back to an XKB keymap. The Wayland
implementation is the reference here — it drives input from the
`wl_seat` family of interfaces and builds its keymap from the
compositor. A windowless implementation has no input at all, which is
why headless does not implement any of this.

A view can also override [vfunc@View.lock_pointer] and
[vfunc@View.unlock_pointer] to support the Pointer Lock API, and
[vfunc@View.set_cursor_from_name] to set the cursor shape.

## The display's screens

If your platform has monitors, expose them through
[vfunc@Display.get_n_screens] and [vfunc@Display.get_screen], returning
[class@Screen] objects. Wayland maps these onto `wl_output`s and DRM
onto KMS connectors. This is independent of the buffer-format vfuncs
above — a platform can have monitors without special format needs, or
negotiate formats without having any monitors.

## Making the implementation discoverable

WebKit finds implementations through the GIO extension point
`WPE_DISPLAY_EXTENSION_POINT_NAME`: a [class@Display] subclass registers
itself there with a unique name and a priority. The name is what
`WPE_DISPLAY` selects — by convention `wpe-display-<name>` — and the
priority orders candidates when several are installed: the Wayland
implementation uses `0`, and the more specialized DRM and headless ones
use `-100` so they are tried only after Wayland declines.

How you register depends on whether the implementation is compiled in or
loaded as a module.

**Linked directly.** When the type is part of the binary, the
`G_DEFINE_..._WITH_CODE` form registers it against the extension point at
type-registration time:

```c
G_DEFINE_FINAL_TYPE_WITH_CODE (MyDisplay, my_display, WPE_TYPE_DISPLAY,
    g_io_extension_point_implement (WPE_DISPLAY_EXTENSION_POINT_NAME,
        g_define_type_id, "wpe-display-myplatform", 0))
```

An application that links the library can then construct `MyDisplay`
itself, or let [func@Display.get_default] find it.

**As a loadable module.** A module's types are tied to the module's
lifetime, so the display must be a *dynamic* type —
`G_DEFINE_DYNAMIC_TYPE_EXTENDED`, which generates a
`my_display_register_type()` — registered from the module's load entry
point rather than with `WITH_CODE`:

```c
G_MODULE_EXPORT void
g_io_module_load (GIOModule *module)
{
    my_display_register_type (G_TYPE_MODULE (module));
    g_io_extension_point_implement (WPE_DISPLAY_EXTENSION_POINT_NAME,
        MY_TYPE_DISPLAY, "wpe-display-myplatform", 0);
}

G_MODULE_EXPORT void
g_io_module_unload (GIOModule *module)
{
}
```

WebKit scans its module directory eagerly, so a `g_io_module_query` entry
point is not needed. GLib also supports module-name-scoped entry points
(`g_io_<module-name>_load` / `_unload`), which the out-of-tree GTK
implementation uses so several modules can share a binary.

## Building and installing

Build the implementation against the `wpe-platform-2.0` `pkg-config`
module. A loadable module is a shared library installed into the
directory WebKit scans:

```
${LIBDIR}/wpe-platform-2.0/modules/
```

Once installed, force WebKit to use it by setting `WPE_DISPLAY` to the
name you registered:

```sh
WPE_DISPLAY=wpe-display-myplatform MiniBrowser https://webkit.org
```

While iterating on a module that is not installed yet, point WebKit at
the build directory with `WPE_PLATFORMS_PATH`.

## A minimum viable implementation

The smallest implementation that renders anything is a [class@Display]
that overrides [vfunc@Display.connect] and [vfunc@Display.create_view],
plus a [class@View] that overrides [vfunc@View.render_buffer]. That is
enough to run — silently, offscreen — as the headless implementation
demonstrates. Every other vfunc fills in a capability the defaults
cannot guess: add `create_toplevel` and `get_egl_display` early (they
unblock most of WebKit), then input, screens, and buffer-format
negotiation as the platform you are targeting requires.

For complete, working code, read the in-tree implementations under
`Source/WebKit/WPEPlatform/wpe/` — `wayland/` for the full windowed
case, `headless/` for the minimal one — and the out-of-tree
[GTK implementation](https://github.com/Igalia/wpe-platform-gtk) for a
real-world example built on the public API.
