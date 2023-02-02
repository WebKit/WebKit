Title: Migrating WebKitGTK Applications to GTK 4
Slug: migrating-to-webkitgtk-6.0

# Migrating WebKitGTK Applications to GTK 4

This document contains guidance to application developers looking to migrate
applications that use WebKitGTK from GTK 3 to GTK 4.

webkitgtk-6.0 is a new API version of WebKitGTK designed for use with GTK 4 and
libsoup 3. This API version obsoletes webkit2gtk-4.0 and webkit2gtk-4.1, the
GTK 3 API versions for libsoup 2 and libsoup 3, respectively. It also obsoletes
webkit2gtk-5.0, which was an earlier unstable API version for GTK 4.

## Upgrade to libsoup 3

libsoup 2 and libsoup 3 cannot be linked together. If your application currently
uses webkit2gtk-4.0, you must first port to webkit2gtk-4.1 by eliminating use
of libsoup 2. See [Migrating from libsoup 2](https://libsoup.org/libsoup-3.0/migrating-from-libsoup-2.html)
for guidance on this. After first migrating to webkit2gtk-4.1, then it is
time to start looking into webkitgtk-6.0.

## Stop Using Deprecated APIs

All APIs that were previously deprecated in webkit2gtk-4.0 and webkit2gtk-4.1
have been removed. This includes the original JavaScriptCore API (e.g.
`JSContextRef` and `JSObjectRef`), which has been replaced by the GObject-style
JavaScriptCore API (e.g. [type@JSC.Context] and [type@JSC.Object]) that is
available since 2.22. It also includes the entire GObject DOM API (e.g.
`WebKitDOMDocument`), which has been removed without replacement. Use JavaScript
to interact with and manipulate the DOM instead, perhaps via
[method@WebKit.WebView.run_javascript] or [method@JSC.ValueObject.invoke_method].

## Upgrade to GTK 4

After successfully building your webkit2gtk-4.1 application without deprecation
warnings, then it is time to attempt to upgrade to GTK 4 and webkitgtk-6.0.
This is easier said than done, but [the GTK 4 migration guide](https://docs.gtk.org/gtk4/migrating-3to4.html)
will help. Good luck.

## Most Types Are Final

Only two types are now derivable:

- [type@WebView] has been often subclassed to customize its behavior for an
  specific application. This possibility has been kept, as it has proved
  useful in the past.
- [type@InputMethodContext] is specifically designed in a way that subclassing
  is required to make use of it.

The rest of the types are no longer derivable; they are defined with the
`G_TYPE_FLAG_FINAL` flag set. Use composition instead of derivation.

## Mandatory Web Process Sandbox

The `webkit_web_context_set_sandbox_enabled()` and `webkit_web_context_get_sandbox_enabled()`
functions have been removed. The web process sandbox is now always enabled. If
your application's web process needs to access extra directories, use
[method@WebKit.WebContext.add_path_to_sandbox] to mount them in the sandbox.

## Mandatory Process Swap on Cross-site Navigation

The `WebKitWebContext:process-swap-on-cross-site-navigation-enabled` property
has been removed. Process swapping is now mandatory. Your application should be
prepared for the web view's web process to be replaced when navigating between
different security origins. You can ensure that your application is prepared for
this change before porting to GTK 4 by testing your application with the
`WebKitWebContext:process-swap-on-cross-site-navigation-enabled` property
enabled. This property was previously disabled by default.

## Event Parameter Removed from Context Menu and Option Menu Signals

[signal@WebKit.WebView::context-menu] and [signal@WebKit.WebView::show-option-menu]
no longer have a [type@Gdk.Event] parameter. Adjust your signal handlers
accordingly.

## Changes to WebKitWebView Construction

`webkit_web_view_new_with_context()`, `webkit_web_view_new_with_settings()`, and
`webkit_web_view_new_with_user_content_manager()` have all been removed. You
may directly use `g_object_new()` instead. [ctor@WebKit.WebView.new] and
[ctor@WebKit.WebView.new_with_related_view] both remain.

## Network Session API

WebKit now uses a single global network process for all web contexts, and different
network sessions can be created and used in the same network process. All the networking
APIs have been moved from [type@WebKit.WebContext] and [type@WebKit.WebsiteDataManager] to the new class
[type@WebKit.NetworkSession]. There's a default global persistent session that you can get with
[func@WebKit.NetworkSession.get_default]. You can also create new sessions with
[ctor@WebKit.NetworkSession.new] for persistent sessions and [ctor@WebKit.NetworkSession.new_ephemeral]
for ephemeral sessions. It's no longer possible to create a [type@WebKit.WebsiteDataManager], it's now
created by the [type@WebKit.NetworkSession] automatically at construction time. The [type@WebKit.NetworkSession]
to be used must be passed to the [type@WebKit.WebView] as a construct parameter. You can pass the
same [type@WebKit.NetworkSession] object to several web views to use the same session. The only exception
is automation mode, which uses its own ephemeral session that is configured by the automation
session capabilities.

## Hardware Acceleration Policy

`WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND` has been removed from
[enum@WebKit.HardwareAccelerationPolicy]. You may still use
[method@WebKit.Settings.set_hardware_acceleration_policy] to enable or disable
hardware acceleration.

## Scrollbar Appearance

Because GTK 4 does not contain a foreign drawing API, it is no longer possible
to draw scrollbars that match arbitrary GTK themes. Accordingly, the
`webkit_web_context_get_use_system_appearance_for_scrollbars` and
`webkit_web_context_set_use_system_appearance_for_scrollbars` APIs have been
removed. WebKit will draw scrollbars that match the Adwaita GTK theme.
