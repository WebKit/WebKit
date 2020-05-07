kind: manual

depends:
- sdk/gtk+-3.bst
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
  - make

  install-commands:
  - |
    make -j1 INSTALL_ROOT=%{install-root} install

    #  - |
    #    rm -f "%{install-root}%{libdir}"/*.a
    #    rm -f "%{install-root}%{libdir}/lib"/*.la

public:
  bst:
    split-rules:
      devel:
        (>):
        - "%{libdir}/*.so"

sources:
- kind: tar
  url: http://download.qt.io/archive/qt/5.14/5.14.1/submodules/qtbase-everywhere-src-5.14.1.tar.xz
  ref: d9d423a6e7bcf1055c0372fc029f14a6fe67dd62c67b83095cde68b60b762cf7
- kind: patch
  path: patches/qtbase-avoid-hardcoding-kernel-version.patch
- kind: patch
  path: patches/qtbase-use-wayland-on-gnome.patch
- kind: patch
  path: patches/qtbase-revert-correct-handling-for-xdg-runtime-dir.patch
