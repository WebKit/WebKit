Title: Migrating from libwpe
Slug: migrating-from-libwpe

This guide is for embedders moving an application from
[libwpe](https://github.com/WebKit/libwpe) and
[WPEBackend-fdo](https://github.com/Igalia/WPEBackend-fdo) to WPEPlatform.
The higher-level `WebKitWebView` API is unchanged; almost everything you
did *below* it disappears.

Under libwpe an application created a `wpe_view_backend` — usually through
WPEBackend-fdo's "exportable" backend — and drove rendering, buffer
release, and input dispatch itself through its callbacks. WPEPlatform
moves all of that into WebKit and the platform implementation, so
migrating an application is mostly a matter of *deleting* code: the
view-backend, the exportable client, buffer management, and input
plumbing all go away. In the common case you construct the same
`WebKitWebView` as before and never touch a WPEPlatform type.

WPEPlatform surfaces only when you want more than the defaults — to pin
the application to a particular platform, add keyboard shortcuts, or drive
the window. Those cases follow, each optional; an application that needs
none of them is migrated once its web view is constructed (section 1).

For a symbol-by-symbol lookup, see [Migration mapping
table](migration-mapping.html). If you maintained a *custom*
WPEBackend-fdo backend rather than an application, that code is a platform
implementation — see [Writing a WPE platform
implementation](tutorial-platform.html). The snippets are stripped of
boilerplate and assume familiarity with GLib/GObject.

## 1. Creating the web view

The one change every application makes is how the web view is
constructed. Under WPEBackend-fdo you built a `wpe_view_backend`, wrapped
it in a `WebKitWebViewBackend`, and passed that as the web view's
`backend` property. WPEPlatform has no backend to build: construct the
`WebKitWebView` without one and WebKit selects a platform for it.

**Before:**

```c
struct wpe_view_backend *wpe_backend = /* ...fdo exportable backend... */;
WebKitWebViewBackend *backend =
    webkit_web_view_backend_new (wpe_backend, NULL, NULL);
WebKitWebView *web_view =
    g_object_new (WEBKIT_TYPE_WEB_VIEW, "backend", backend, NULL);
```

**After:**

```c
WebKitWebView *web_view = g_object_new (WEBKIT_TYPE_WEB_VIEW, NULL);
```

There is no `webkit_web_view_new()` in this API; a web view is always
created with `g_object_new()`, so for most applications the migration is
simply dropping the `backend` property. WebKit then resolves a platform
by iterating the registered implementations — the built-in Wayland, DRM,
and headless ones, plus any installed module — and connecting to the
first that succeeds. Your existing settings, network-session, navigation,
and signal-handler code carries over unchanged. For an application that
needs no platform-specific control, this is the whole migration.

## 2. Pinning to a specific platform

When an application must run on a particular platform — a Wayland-only
kiosk, say — construct a [class@Display] for it and pass it to the web
view through its `display` construct property (since 2.44), instead of
letting WebKit choose.

```c
g_autoptr(GError) error = NULL;
g_autoptr(WPEDisplayWayland) display = wpe_display_wayland_new ();
if (!wpe_display_wayland_connect (display, NULL, &error))
    g_error ("Failed to connect to Wayland: %s", error->message);

WebKitWebView *web_view =
    g_object_new (WEBKIT_TYPE_WEB_VIEW, "display", display, NULL);
```

This links the platform library — here `wpe-platform-wayland-2.0` — and
instantiates it directly, with no module discovery involved. To stay
portable but still choose at runtime, use [func@Display.get_default],
which returns the first platform that connects, or set `WPE_DISPLAY=<name>`
in the environment to force one. This is what replaces libwpe's
`wpe_loader_init()`, which selected a backend by shared-library name.

## 3. Adding keyboard shortcuts

libwpe delivered input through a view-backend input client whose
`dispatch_*_event` methods an application overrode — typically to
implement browser keyboard shortcuts. WPEPlatform delivers the same input
as the [signal@View::event] signal on the [class@View] WebKit created for
the web view. Reach the view with `webkit_web_view_get_wpe_view()`,
connect to the signal, inspect the [struct@Event], and return `TRUE` to
consume the event before the page sees it.

**Before (a libwpe input client):**

```c
bool dispatch_keyboard_event (struct wpe_input_keyboard_event *event)
{
    if (event->pressed
        && (event->modifiers & wpe_input_keyboard_modifier_control)
        && event->key_code == WPE_KEY_q) {
        quit ();
        return true;   // handled
    }
    return false;
}
```

**After:**

```c
static gboolean
on_view_event (WPEView *view, WPEEvent *event, gpointer user_data)
{
    if (wpe_event_get_event_type (event) != WPE_EVENT_KEYBOARD_KEY_DOWN)
        return FALSE;

    WPEModifiers modifiers = wpe_event_get_modifiers (event);
    guint        keyval    = wpe_event_keyboard_get_keyval (event);

    if ((modifiers & WPE_MODIFIER_KEYBOARD_CONTROL) && keyval == WPE_KEY_q) {
        quit ();
        return TRUE;   // consumed, not forwarded to the page
    }
    return FALSE;
}

WPEView *view = webkit_web_view_get_wpe_view (web_view);
g_signal_connect (view, "event", G_CALLBACK (on_view_event), NULL);
```

Event details come from typed accessors — [method@Event.get_event_type],
[method@Event.get_modifiers], [method@Event.keyboard_get_keyval], and the
pointer, scroll, and touch equivalents — rather than fields of a C struct.
The `WPE_KEY_*` keysym constants keep their names, now in
`<wpe/WPEKeysyms.h>`.

## 4. Controlling the window

The view is presented in a [class@Toplevel] — the window. Reach it with
[method@View.get_toplevel] and drive the window from there: set the title,
toggle fullscreen or maximize, request a resize. Observe changes through
the [signal@View::toplevel-state-changed] signal and
[method@Toplevel.get_state].

**Before (a libwpe fullscreen handler):**

```c
wpe_view_backend_set_fullscreen_handler (backend, on_fullscreen, app);
wpe_view_backend_platform_set_fullscreen (backend, true);
```

**After:**

```c
WPEToplevel *toplevel = wpe_view_get_toplevel (view);
wpe_toplevel_set_title (toplevel, "Hello WPE");

if (wpe_toplevel_get_state (toplevel) & WPE_TOPLEVEL_STATE_FULLSCREEN)
    wpe_toplevel_unfullscreen (toplevel);
else
    wpe_toplevel_fullscreen (toplevel);
```

[method@Toplevel.maximize], [method@Toplevel.minimize], and
[method@Toplevel.resize] round out the window controls. Only a windowed
platform such as Wayland acts on them; on DRM and headless they are
no-ops.

## 5. What WebKit now handles for you

The platform code an fdo-based application carried has no WPEPlatform
equivalent, because it is no longer the application's responsibility:

- **Rendering.** The exportable EGL/SHM export callbacks, buffer release,
  and frame-complete notifications are gone; WebKit renders into the view
  directly.
- **Input dispatch.** The application no longer creates or dispatches
  events, only observes them (section 3).
- **View state.** Visibility, focus, and scale factor are driven by WebKit
  and the platform, not set by the application.
- **Process and renderer setup.** `wpe_renderer_host_*`,
  `wpe_renderer_backend_egl_*`, and `wpe_process_provider_*` are now
  internal to WebKit.

The [Migration mapping table](migration-mapping.html) records where each
of these symbols went. The machinery a platform implementation *does*
still need is covered in [Writing a WPE platform
implementation](tutorial-platform.html).
