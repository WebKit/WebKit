Title: Overview

# Overview

Dex is a [GNOME](https://www.gnome.org/) library that provides deferred
execution for GObject-based (including GTK) applications through the use
of futures.

The library attempts to follow well established patterns and terminology around
programming with futures.

Dex depends on modern releases of GLib 2.0, but can probably be made to support
older versions depending if people show up to test it.

##  pkg-config name

To build a program that uses Dex, you can use the following command to get
the cflags and libraries necessary to compile and link.

```sh
gcc hello.c `pkg-config --cflags --libs libdex-1` -o hello
```
