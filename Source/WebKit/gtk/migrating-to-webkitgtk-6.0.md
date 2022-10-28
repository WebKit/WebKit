Title: Migrating WebKitGTK Applications to GTK 4
Slug: migrating-to-webkitgtk-6.0

# Migrating WebKitGTK Applications to GTK 4

This document contains guidance to application developers looking to migrate
applications that use WebKitGTK from GTK 3 to GTK 4.

webkitgtk-6.0 is a new API version of WebKitGTK designed for use with GTK 4 and
libsoup 3. This API version obsoletes webkit2gtk-4.0 and webkit2gtk-4.1, the
GTK 3 API versions for libsoup 2 and libsoup 3, respectively. It also obsoletes
webkit2gtk-5.0, which was an earlier unstable API version for GTK 4.

libsoup 2 and libsoup 3 cannot be linked together. If your application currently
uses webkit2gtk-4.0, you must first port to webkit2gtk-4.1 by eliminating use
of libsoup 2. See [Migrating from libsoup 2](https://libsoup.org/libsoup-3.0/migrating-from-libsoup-2.html)
for guidance on this. After first migrating to webkit2gtk-4.1, then it is
time to start looking into webkitgtk-6.0.

## Mandatory Process Swap on Cross-site Navigation

The `WebKitWebContext:process-swap-on-cross-site-navigation-enabled` property
has been removed. Process swapping is now mandatory. Your application should be
prepared for the web view's web process to be replaced when navigating between
different security origins. You can ensure that your application is prepared for
this change before porting to GTK 4 by testing your application with the
`WebKitWebContext:process-swap-on-cross-site-navigation-enabled` property
enabled. This property was previously disabled by default.
