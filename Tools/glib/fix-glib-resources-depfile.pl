# Workaround for https://gitlab.gnome.org/GNOME/glib/-/merge_requests/3460
# Used by FindGLibCompileResources.cmake for glib-compile-commands < 2.77
BEGIN {
    ($old_xml, $new_target) = splice(@ARGV, 0, 2);
}
s{\Q$old_xml:}{$new_target: $old_xml}