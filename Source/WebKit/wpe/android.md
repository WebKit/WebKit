Title: Android Support
Slug: android

WPE WebKit supports running on [Android](https://www.android.com), which is
considerably different from other Linux-based systems.

## Building 

Compiling WPE WebKit for Android requires a suitable toolchain configured
to use a “sysroot” which includes the needed dependencies—including
their development files.

Currently, the preferred method to build WPE WebKit for Android is using
[wpe-android-cerbero](https://github.com/Igalia/wpe-android-cerbero/). This is
a fork of the [Cerbero](https://gitlab.freedesktop.org/gstreamer/cerbero/)
build system used to cross-compile the GStreamer multimedia libraries for a
number of targets—Android included—, with the needed changes to compile WPE
WebKit. Note that this produces the *native* libraries that are part of a
WebKit build, and their *native* dependencies.


## View Widget

A web view widget that integrates with the Android
[View](https://developer.android.com/reference/android/view/View)-based GUI
toolkit and may be used as part of an application written in Java™ or Kotlin
is provided by the separate
[WPE Android](https://github.com/Igalia/wpe-android) project.

Prebuilt packages produced by the WPE Android project are readily available
as
[org.wpewebkit.wpeview](https://central.sonatype.com/artifact/org.wpewebkit.wpeview/wpeview)
at the [Maven Central](https://central.sonatype.com/) repository. Using these
package is the recommended way of using WPE WebKit to develop Android
applications that embed WPE WebKit web views.


## Logging

WPE WebKit integrates with the Android `logd` system service, and uses
the `WPEWebKit` tag. This means that logging is configured using system
properties, and `logcat` may be used to read log entries:

```sh
adb shell setprop log.tag.WPEWebKit VERBOSE
adb logcat -s WPEWebKit
```

The `WEBKIT_DEBUG` [environment variable](environment.html) is replaced by the
`debug.WPEWebKit.log` system property to configure logging channels:

```sh
adb shell setprop debug.WPEWebKit.log 'Process,Media=error'
```

Using the `persist.` prefix may be added to system properties to store
settings across device reboots.
