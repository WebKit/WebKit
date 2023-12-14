kind: manual

depends:
- freedesktop-sdk.bst:components/gtk3.bst
- freedesktop-sdk.bst:components/perl.bst
      # gtk3 seems to have all the so files that qtbase needs access to
      # perl is listed as a recommended dependency on packages.debian.org
      # plus, perl seems to be needed as a build-dep for almost everything that depends on qtbase

build-depends:
- freedesktop-sdk.bst:components/pkg-config.bst
- freedesktop-sdk.bst:components/which.bst
- freedesktop-sdk.bst:components/libdrm.bst

variables:
  conf-local: |
    -prefix %{prefix} \
    -libdir %{libdir} \
    -accessibility \
    -confirm-license \
    -dbus-linked \
    -fontconfig \
    -glib \
    -gtk \
    -icu \
    -no-cups \
    -no-directfb \
    -no-kms \
    -no-linuxfb \
    -no-pch \
    -no-rpath \
    -no-use-gold-linker \
    -nomake examples \
    -nomake tests \
    -opensource \
    -openssl-linked \
    -optimized-qmake \
    -platform linux-g++ \
    -system-harfbuzz \
    -system-proxies \
    -system-sqlite \
    -shared

config:
  configure-commands:
  - ./configure %{conf-local}

  build-commands:
  - make -j%{max-jobs}

  install-commands:
  - |
    make -j1 INSTALL_ROOT=%{install-root} install

public:
  bst:
    split-rules:
      devel:
        (>):
        - "%{libdir}/*.so"

sources:
- kind: git_repo
  url: kde:qtbase
  track: kde/5.15
  ref: v5.15.0-870-g663d240a18f5acac82fd2bd0fd4a33c4f47b260a
- kind: patch
  path: patches/qtbase-avoid-hardcoding-kernel-version.patch
- kind: patch
  path: patches/qtbase-use-wayland-on-gnome.patch
- kind: patch
  path: patches/qtbase-revert-qstandardpaths-dont-change-permissions-on-xdg-runtime-dir.patch
