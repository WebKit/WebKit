kind: flatpak_image

build-depends:
- flatpak/platform-image.bst
- freedesktop-sdk.bst:integration/platform-integration.bst

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

    Extension org.freedesktop.Platform.Timezones:
      directory: share/zoneinfo
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

    Extension org.freedesktop.Platform.VAAPI.Intel:
      directory: '%{lib}/dri/intel-vaapi-driver'
      autodelete: 'false'
      no-autodownload: 'true'
      add-ld-path: lib
      download-if: have-intel-gpu
      version: '%{freedesktop-sdk-flatpak-branch}'

    Extension org.freedesktop.Platform.openh264:
      directory: '%{lib}/openh264'
      add-ld-path: extra
      autodelete: 'false'
      version: '%{freedesktop-sdk-flatpak-branch}'
