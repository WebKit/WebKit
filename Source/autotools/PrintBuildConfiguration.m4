dnl AM_APPEND_TO_DESCRIPTION
dnl Appends the given string to the description variable,
dnl using a separator if the description variable is not empty.
dnl
dnl Usage:
dnl AM_APPEND_TO_DESCRIPTION([DESCRIPTION], [STRING])
AC_DEFUN([AM_APPEND_TO_DESCRIPTION], [
  if test "$$1" != ""; then
    $1="${$1}, "
  fi

  $1="${$1}$2"
]) dnl AM_APPEND_TO_DESCRIPTION


target_description=""
if test "$with_x11_target" = "yes"; then
    AM_APPEND_TO_DESCRIPTION(target_description, "x11")
fi
if test "$with_wayland_target" = "yes"; then
    AM_APPEND_TO_DESCRIPTION(target_description, "wayland")
fi
if test "$with_target" = "win32"; then
    AM_APPEND_TO_DESCRIPTION(target_description, "win32")
fi
if test "$with_target" = "quartz"; then
    AM_APPEND_TO_DESCRIPTION(target_description, "quartz")
fi
if test "$with_target" = "directfb"; then
    AM_APPEND_TO_DESCRIPTION(target_description, "directfb")
fi

AC_OUTPUT

echo "
WebKit was configured with the following options:

Build configuration:
 Enable debugging (slow)                                  : $enable_debug
 Compile with debug symbols (slow)                        : $enable_debug_symbols
 Enable GCC build optimization                            : $enable_optimizations
 Code coverage support                                    : $enable_coverage
 Optimized memory allocator                               : $enable_fast_malloc
 Accelerated rendering backend                            : $acceleration_description

Features:
=======
 WebKit1 support                                          : $enable_webkit1
 WebKit2 support                                          : $enable_webkit2
 Accelerated Compositing                                  : $enable_accelerated_compositing
 Accelerated 2D canvas                                    : $enable_accelerated_canvas
 Battery API support                                      : $enable_battery_status
 Gamepad support                                          : $enable_gamepad
 Geolocation support                                      : $enable_geolocation
 HTML5 video element support                              : $enable_video
 JIT compilation                                          : $enable_jit
 Opcode stats                                             : $enable_opcode_stats
 SVG fonts support                                        : $enable_svg_fonts
 SVG support                                              : $enable_svg
 Spellcheck support                                       : $enable_spellcheck
 Credential storage support                               : $enable_credential_storage
 Web Audio support                                        : $enable_web_audio
 WebGL                                                    : $enable_webgl


GTK+ configuration:
 GTK+ version                                             : $with_gtk
 GDK targets                                              : $target_description
 Introspection support                                    : $enable_introspection
 Generate documentation                                   : $enable_gtk_doc
"
