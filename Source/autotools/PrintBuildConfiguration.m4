AC_OUTPUT

echo "
WebKit was configured with the following options:

Build configuration:
 Enable debugging (slow)                                  : $enable_debug
 Compile with debug symbols (slow)                        : $enable_debug_symbols
 Enable debug features (slow)                             : $enable_debug_features
 Enable GCC build optimization                            : $enable_optimizations
 Code coverage support                                    : $enable_coverage
 Unicode backend                                          : $with_unicode_backend
 Optimized memory allocator                               : $enable_fast_malloc
 Accelerated rendering backend                            : $acceleration_backend_description

Features:
=======
 WebKit1 support                                          : $enable_webkit1
 WebKit2 support                                          : $enable_webkit2
 Accelerated Compositing                                  : $enable_accelerated_compositing
 Gamepad support                                          : $enable_gamepad
 Geolocation support                                      : $enable_geolocation
 HTML5 video element support                              : $enable_video
 JIT compilation                                          : $enable_jit
 Media stream support                                     : $enable_media_stream
 Opcode stats                                             : $enable_opcode_stats
 SVG fonts support                                        : $enable_svg_fonts
 SVG support                                              : $enable_svg
 Spellcheck support                                       : $enable_spellcheck
 Web Audio support                                        : $enable_web_audio
 WebGL                                                    : $enable_webgl
 XSLT support                                             : $enable_xslt


GTK+ configuration:
 GTK+ version                                             : $with_gtk
 GDK target                                               : $with_target
 GStreamer version                                        : $with_gstreamer
 Introspection support                                    : $enable_introspection
 Generate documentation                                   : $enable_gtk_doc
"
if test "$with_unicode_backend" = "glib"; then
    echo "     >> WARNING: the glib-based unicode backend is slow and incomplete <<"
    echo
    echo
fi
