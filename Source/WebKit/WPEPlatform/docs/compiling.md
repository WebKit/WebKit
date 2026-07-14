Title: Compiling against WPEPlatform
Slug: compiling

# Compiling against WPEPlatform

This page lists the `pkg-config` modules and headers an application or
browser needs to build against WPEPlatform, plus minimal
[CMake](https://cmake.org) and [Meson](https://mesonbuild.com/)
snippets. For a higher-level introduction to the API see
[overview](overview.html).

## pkg-config modules

All of WPEPlatform's code is compiled into a single shared library,
`libWPEWebKit-2.0`; there is no separate per-backend library. What
varies is the set of installed `pkg-config` modules: the core
`wpe-platform-2.0` module is always present and carries the compiler
and linker flags (it links `libWPEWebKit-2.0`), while each per-backend
module is installed only when its built-in implementation was enabled
at build time (see [Build-time availability](#build-time-availability)
below) and simply depends on the core module.

| Module | Header | What it provides |
|---|---|---|
| `wpe-platform-2.0` | `<wpe/wpe-platform.h>` | The core API: [class@Display], [class@View], [class@Toplevel], [class@Buffer], events, settings, keymap, clipboard, gamepad, input-method, screens. |
| `wpe-platform-wayland-2.0` | `<wpe/wayland/wpe-wayland.h>` | The built-in Wayland implementation (`wpe_display_wayland_new()`, `WPEScreenWayland`, `WPEToplevelWayland`, `WPEViewWayland`, `WPEClipboardWayland`). |
| `wpe-platform-drm-2.0` | `<wpe/drm/wpe-drm.h>` | The built-in DRM/KMS implementation (`wpe_display_drm_new()`, related screen/toplevel/view classes). |
| `wpe-platform-headless-2.0` | `<wpe/headless/wpe-headless.h>` | The built-in headless implementation (`wpe_display_headless_new()`, headless toplevel/view). |
| `wpe-webkit-2.0` | `<wpe/webkit.h>` | The higher-level WebKit API. Almost every application needs this in addition to `wpe-platform-2.0`. |

## Build-time availability

WPEPlatform is gated behind WPE WebKit's `ENABLE_WPE_PLATFORM` CMake
option. As of writing this option defaults to
`ENABLE_DEVELOPER_MODE`, which means **release builds and distribution
packages may ship without it**. Before assuming any of the modules
above exist on a target, confirm that WPE WebKit was built with
`-DENABLE_WPE_PLATFORM=ON`, or probe for the module from your build
system:

```sh
pkg-config --exists wpe-platform-2.0 && echo present
```

The per-backend implementations are independently optional via
`ENABLE_WPE_PLATFORM_WAYLAND`, `ENABLE_WPE_PLATFORM_DRM`, and
`ENABLE_WPE_PLATFORM_HEADLESS` (all default `ON` when WPEPlatform
itself is enabled). The DRM built-in additionally requires GBM
(`USE_GBM`), so `wpe-platform-drm-2.0` is absent when GBM is
unavailable even with the option left on. If your application pins to
a specific built-in implementation, probe for that module too:

```sh
pkg-config --exists wpe-platform-wayland-2.0 || \
    echo "Wayland built-in not available; fall back to the default display"
```

In most cases applications should prefer [func@Display.get_default]
and let WPEPlatform pick whichever implementation is installed, rather
than hard-linking against a specific backend.

## CMake

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(WPEPlatform REQUIRED IMPORTED_TARGET wpe-platform-2.0)
pkg_check_modules(WPEWebKit   REQUIRED IMPORTED_TARGET wpe-webkit-2.0)

# Optional: only link the Wayland built-in if you need its symbols.
pkg_check_modules(WPEPlatformWayland IMPORTED_TARGET wpe-platform-wayland-2.0)

add_executable(my-browser main.c)
target_link_libraries(my-browser PRIVATE
    PkgConfig::WPEPlatform
    PkgConfig::WPEWebKit
)
```

## Meson

```meson
wpe_platform_dep = dependency('wpe-platform-2.0')
wpe_webkit_dep   = dependency('wpe-webkit-2.0')

# Optional Wayland built-in.
wpe_platform_wayland_dep = dependency('wpe-platform-wayland-2.0',
                                      required: false)

executable('my-browser', 'main.c',
    dependencies: [wpe_platform_dep, wpe_webkit_dep],
)
```
