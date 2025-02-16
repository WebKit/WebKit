kind: flatpak_image

build-depends:
- flatpak/platform-image.bst
- freedesktop-sdk.bst:integration/platform-integration.bst

config:
  directory: '%{prefix}'
  exclude:
  - debug
  - docs
  - locale
  metadata:
    Runtime:
      name: org.webkit.Platform
      runtime: org.webkit.Platform/%{gcc_arch}/%{sdk-branch}
      sdk: org.webkit.Sdk/%{gcc_arch}/%{sdk-branch}

    Environment:
      GI_TYPELIB_PATH: /app/lib/girepository-1.0
      XDG_DATA_DIRS: /app/share:/usr/share:/usr/share/runtime/share:/run/host/share

    Extension org.webkit.Platform.Locale:
      directory: share/runtime/locale

    # Copied from sdk/elements/platform.bst from freedesktop-sdk because
    # bst doesn't support inheriting extensions.
    # When syncing, skip the Locale extension, and add the freedesktop-sdk
    # version to extensions that don't have a version

    Extension org.freedesktop.Platform.GL:
      # 1.4 is for Nvidia drivers
      versions: "%{sdk-branch};%{sdk-branch-extra};1.4"
      version: "1.4"
      directory: "%{lib}/GL"
      subdirectories: "true"
      no-autodownload: "true"
      autodelete: "false"
      add-ld-path: "lib"
      merge-dirs: "vulkan/icd.d;glvnd/egl_vendor.d;OpenCL/vendors;lib/dri;lib/d3d;vulkan/explicit_layer.d;vulkan/implicit_layer.d"
      download-if: "active-gl-driver"
      enable-if: "active-gl-driver"
      autoprune-unless: active-gl-driver

    Extension org.freedesktop.Platform.Timezones:
      directory: share/zoneinfo
      version: '%{sdk-branch}'

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

    Extension org.freedesktop.Platform.VAAPI.Intel:
      directory: '%{lib}/dri/intel-vaapi-driver'
      autodelete: 'false'
      no-autodownload: 'true'
      add-ld-path: lib
      download-if: have-intel-gpu
      version: '%{sdk-branch}'
