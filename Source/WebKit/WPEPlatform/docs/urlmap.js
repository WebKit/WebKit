const baseURLApiLevelSuffix = (function (uri) {
  const slashPos = uri.lastIndexOf("/");
  const dashPos = uri.lastIndexOf("-", slashPos);
  return uri.substring(dashPos, slashPos + 1);
})(window.location.href);

baseURLs = [
  ["GLib", "https://docs.gtk.org/glib/"],
  ["GObject", "https://docs.gtk.org/gobject/"],
  ["Gio", "https://docs.gtk.org/gio/"],
  ["WPEPlatform", "../wpe-platform" + baseURLApiLevelSuffix]
]
