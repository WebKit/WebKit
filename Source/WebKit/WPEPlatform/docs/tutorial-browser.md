Title: How to Use WPE Platform to Create a WPE WebKit Browser
Slug: tutorial-browser

# How to Use WPE Platform to Create a WPE WebKit Browser

This tutorial walks through writing the smallest useful WPE WebKit
browser: a single window that loads a URL and runs a GLib main loop.
It is aimed at application developers who are starting a new project
on top of WPEPlatform, or who are porting an existing libwpe-based
application across.

The page assumes [Compiling against WPEPlatform](compiling.html) has
been read — the `pkg-config` modules, headers, and a working
CMake or Meson skeleton are taken for granted below. Code is in C and
targets `-std=c11`.

Two variants are covered. The first uses [func@Display.get_default]
and stays portable across whichever built-in platform is installed
(Wayland, DRM, headless). The second pins the application to a
specific built-in — for example, when the binary is only intended to
run on Wayland.

## A minimal browser, multi-platform

```c
// main.c
#include <glib.h>
#include <wpe/wpe-platform.h>
#include <wpe/webkit.h>

static void
on_view_closed (WPEView  *view,
                gpointer  user_data)
{
    GMainLoop *loop = user_data;
    g_main_loop_quit (loop);
}

int
main (int argc, char *argv[])
{
    const char *uri = (argc > 1) ? argv[1] : "https://wpewebkit.org";

    WPEDisplay *display = wpe_display_get_default ();
    if (!display) {
        g_printerr ("Could not connect to a WPE display.\n");
        return 1;
    }

    g_autoptr(WebKitWebView) web_view =
        WEBKIT_WEB_VIEW (g_object_new (WEBKIT_TYPE_WEB_VIEW,
                                       "display", display,
                                       NULL));

    WPEView *wpe_view = webkit_web_view_get_wpe_view (web_view);
    WPEToplevel *toplevel = wpe_view_get_toplevel (wpe_view);
    if (toplevel)
        wpe_toplevel_set_title (toplevel, "Hello WPE");

    g_autoptr(GMainLoop) loop = g_main_loop_new (NULL, FALSE);
    g_signal_connect (wpe_view, "closed",
                      G_CALLBACK (on_view_closed), loop);

    webkit_web_view_load_uri (web_view, uri);

    g_main_loop_run (loop);

    return 0;
}
```

The flow has four steps:

1. **Get a display.** [func@Display.get_default] iterates the
   registered platform modules in priority order and returns the first
   one that connects successfully. The application does not need to
   know whether the connection landed on Wayland, on DRM, or on the
   headless backend.
2. **Create the web view.** WPE WebKit's `WebKitWebView` exposes a
   construct-only `display` property (since 2.44) that accepts the
   [class@Display]. The display **must already be connected** when it
   is passed to the view — [func@Display.get_default] returns a
   connected display that the application borrows (so it stays a plain
   pointer, not a `g_autoptr`); constructors like
   `wpe_display_wayland_new()` return an unconnected one and require an
   explicit [method@Display.connect] first. Passing the display via
   `g_object_new()` is the only path — there is no
   `webkit_web_view_new()` taking a [class@Display] in the
   `wpe-webkit-2.0` API. The view internally calls
   [vfunc@Display.create_view] (and a toplevel, by default) on the
   application's behalf.
3. **Decorate the toplevel.** A view created against a default-display
   path is normally hosted inside a [class@Toplevel] automatically.
   `webkit_web_view_get_wpe_view()` gives access to the
   [class@View], and from there [method@View.get_toplevel] lets the
   application set a window title, resize the toplevel, request
   fullscreen, etc. <!-- FIXME: link webkit_web_view_get_wpe_view once WebKit cross-namespace linking is wired -->
4. **Run the main loop.** WPEPlatform delivers events on the default
   GLib main context. Connecting to [signal@View::closed] is the
   simplest way to exit when the user closes the window.

Of the built-in platforms, only the Wayland backend emits this signal
(when the compositor sends an `xdg-toplevel` close request, in
`WPEToplevelWayland.cpp`). The DRM and headless backends have no
user-driven close path, so on those backends the application must
exit on its own criteria (timeout, navigation event, signal handler,
etc.).

The example needs only `wpe-platform-2.0` and `wpe-webkit-2.0` (see
[Compiling against WPEPlatform](compiling.html)), compiled as C11.
Build and run:

```sh
cmake -B build -S .
cmake --build build
./build/hello-wpe https://webkit.org
```

If [func@Display.get_default] cannot find any registered platform
module, set `WPE_DISPLAY` to nudge it
(`WPE_DISPLAY=wpe-display-wayland`, `WPE_DISPLAY=wpe-display-drm`, or
`WPE_DISPLAY=wpe-display-headless`) and re-check that WPE WebKit was
built with the relevant `ENABLE_WPE_PLATFORM_*` flag — see the
Environment variables and Backend model documentation.
<!-- FIXME: link environment-variables.html and backend-model.html once those pages land -->

## Pinning to a specific platform

When the application is guaranteed to run on a known windowing
system, instantiating the built-in directly is more explicit and
removes the module-discovery hop. The only change from the listing
above is the display construction:

```c
#include <wpe/wayland/wpe-wayland.h>

/* ... */

g_autoptr(WPEDisplay) display = WPE_DISPLAY (wpe_display_wayland_new ());
g_autoptr(GError) error = NULL;
if (!wpe_display_connect (display, &error)) {
    g_printerr ("Could not connect to Wayland: %s\n", error->message);
    return 1;
}
```

Add the corresponding `pkg-config` module to the build
(`wpe-platform-wayland-2.0`) and the per-backend header
(`<wpe/wayland/wpe-wayland.h>`). The rest of `main()` is unchanged,
apart from display ownership (see below).

Unlike the borrowed display from [func@Display.get_default],
`wpe_display_wayland_new()` returns a display the caller **owns**
(`(transfer full)`); declaring it with `g_autoptr(WPEDisplay)`, as
above, releases it automatically. It is also returned non-connected,
so the explicit [method@Display.connect] call is required. To connect
to a specific Wayland socket name rather than the default, use
`wpe_display_wayland_connect()` (which takes a `name` argument)
instead of the generic [method@Display.connect].

The DRM and headless built-ins follow the same shape:

- **DRM.** Include `<wpe/drm/wpe-drm.h>`, link against
  `wpe-platform-drm-2.0`, and call `wpe_display_drm_new()`. Useful for
  set-top boxes, kiosks, and any application that owns the display
  outright.
- **Headless.** Include `<wpe/headless/wpe-headless.h>`, link against
  `wpe-platform-headless-2.0`, and call `wpe_display_headless_new()`
  (or `wpe_display_headless_new_for_device()` when targeting a
  specific render node). Useful for tests, capture, and any pipeline
  that wants pixels but no window.

In practice most applications should prefer
[func@Display.get_default] — pinning is appropriate when the
application's deployment story is single-platform and the explicit
dependency is worth the loss of portability.

## Next steps

The browser above renders pages but does little else. From here:

<!-- FIXME: link input-handling.html, displays-and-views.html and backend-model.html once those pages land -->

- Input handling — how key, pointer, touch, and gesture events flow
  from the platform into the view, and how to intercept them.
- Displays and views — the relationships between [class@Display],
  [class@Toplevel], [class@View], and [class@Screen], including who
  owns whom and when each is created.
- Backend model — how WPEPlatform discovers module-installed
  implementations, how priority is resolved, and how to subclass a
  built-in or write a new one from scratch.

For platform implementers — as opposed to application developers —
see the Writing a platform tutorial instead.
<!-- FIXME: link tutorial-platform.html once the platform tutorial has content -->
