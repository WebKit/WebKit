# µprof

A minimalistic version of sysprof-cli with just the bare minimum dependencies.
It also doesn't use D-Bus, nor Polkit.

The use case is to be able to get Sysprof marks on embedded devices.

# Dependencies

muprof has exactly runtime 4 dependencies:

 * glib
 * glib-unix
 * gio
 * gio-unix

And one build time dependency:

 * sysprof-capture-4

The following dependencies are optional, and only enabled if they're found
during build time:

 * liburing (can be disabled with `-Dlibdex:liburing=disabled`)
