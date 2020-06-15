kind: flatpak_image

build-depends:
- flatpak/sdk-image.bst
- freedesktop-sdk.bst:integration/linker-debuginfo.bst
- freedesktop-sdk.bst:integration/sdk-integration.bst

variables:
  # Obtain the versions of the flatpak extensions
  (@): freedesktop-sdk.bst:include/_private/versions.yml

config:
  directory: '%{prefix}'
  exclude:
  - debug
  - docs
  - locale
  metadata:
    Runtime:
      name: org.webkit.Sdk
      runtime: org.webkit.Platform/%{gcc_arch}/%{sdk-branch}
      sdk: org.webkit.Sdk/%{gcc_arch}/%{sdk-branch}

    Environment:
      GI_TYPELIB_PATH: /app/lib/girepository-1.0
      GST_PLUGIN_SYSTEM_PATH: /app/lib/gstreamer-1.0:/usr/lib/extensions/gstreamer-1.0:%{libdir}/gstreamer-1.0
      XDG_DATA_DIRS: /app/share:/usr/share:/usr/share/runtime/share:/run/host/share

    Extension org.webkit.Sdk.Debug:
      directory: lib/debug

    Extension org.webkit.Sdk.Docs:
      directory: share/runtime/docs
      autodelete: 'true'
      no-autodownload: 'true'

    Extension org.webkit.Sdk.Locale:
      directory: share/runtime/locale

    # Copied from sdk/elements/sdk.bst from freedesktop-sdk because
    # bst doesn't support inheriting extensions
    # When syncing, skip the Debug, Docs and Locale extensions, and
    # add the freedesktop-sdk version to extensions that don't have
    # a version.
    Extension org.freedesktop.Platform.GL:
      # 1.4 is for Nvidia drivers
      versions: "%{freedesktop-sdk-flatpak-branch};1.4"
      version: "1.4"
      directory: "%{lib}/GL"
      subdirectories: "true"
      autodelete: "false"
      add-ld-path: "lib"
      merge-dirs: "vulkan/icd.d;glvnd/egl_vendor.d;OpenCL/vendors;lib/dri;lib/d3d;vulkan/explicit_layer.d"
      download-if: "active-gl-driver"
      enable-if: "active-gl-driver"

    Extension org.freedesktop.Platform.Timezones:
      directory: share/zoneinfo
      version: '%{freedesktop-sdk-flatpak-branch}'

    Extension org.freedesktop.Platform.GStreamer:
      directory: lib/extensions/gstreamer-1.0
      subdirectories: 'true'
      no-autodownload: 'true'
      version: '%{freedesktop-sdk-flatpak-branch}'

    Extension org.freedesktop.Platform.Icontheme:
      directory: share/runtime/share/icons
      subdirectories: 'true'
      no-autodownload: 'true'
      version: '1.0'

    Extension org.gtk.Gtk3theme:
      directory: share/runtime/share/themes
      subdirectories: 'true'
      subdirectory-suffix: gtk-3.0
      no-autodownload: 'true'
      version: '3.22'
      download-if: active-gtk-theme

    Extension org.freedesktop.Platform.openh264:
      directory: '%{lib}/openh264'
      add-ld-path: extra
      version: '%{freedesktop-sdk-flatpak-branch}'

    Extension org.freedesktop.Sdk.Extension:
      subdirectories: 'true'
      directory: lib/sdk
      no-autodownload: 'true'
      add-ld-path: lib
      version: '%{freedesktop-sdk-flatpak-branch}'

    Extension org.freedesktop.Platform.VAAPI.Intel:
      directory: '%{lib}/dri/intel-vaapi-driver'
      autodelete: 'false'
      no-autodownload: 'true'
      download-if: have-intel-gpu
      version: '%{freedesktop-sdk-flatpak-branch}'

    (?):
    - arch in ["x86_64", "aarch64"]:
        Extension org.webkit.Sdk.Compat.arm:
          directory: 'lib/arm-linux-gnueabihf'
          autodelete: 'true'
          no-autodownload: 'true'
          add-ld-path: '.'

        Extension org.webkit.Sdk.Compat.arm.Debug:
          directory: 'lib/arm-linux-gnueabihf'
          autodelete: 'true'
          no-autodownload: 'true'

    - arch == "x86_64":
        Extension org.webkit.Sdk.Compat.i386:
          directory: 'lib/i386-linux-gnu'
          autodelete: 'true'
          no-autodownload: 'true'
          add-ld-path: '.'

        Extension org.webkit.Sdk.Compat.i386.Debug:
          directory: 'lib/debug/usr/lib/i386-linux-gnu'
          autodelete: 'true'
          no-autodownload: 'true'

        Extension org.webkit.Sdk.Compat.aarch64:
          directory: 'lib/aarch64-linux-gnu'
          autodelete: 'true'
          no-autodownload: 'true'
          add-ld-path: '.'

        Extension org.webkit.Sdk.Compat.aarch64.Debug:
          directory: "lib/debug/usr/lib/aarch64-linux-gnu"
          autodelete: 'true'
          no-autodownload: 'true'
