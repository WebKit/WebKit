kind: meson

build-depends:
- freedesktop-sdk.bst:components/bison.bst
- freedesktop-sdk.bst:components/flex.bst
- freedesktop-sdk.bst:components/python3.bst
- freedesktop-sdk.bst:components/python3-mako.bst
- freedesktop-sdk.bst:components/vulkan.bst
- freedesktop-sdk.bst:components/wayland-protocols.bst
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst


depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/glslang.bst
- freedesktop-sdk.bst:components/llvm.bst
- freedesktop-sdk.bst:components/libva.bst
- freedesktop-sdk.bst:components/libclc.bst
- freedesktop-sdk.bst:components/opencl.bst
- freedesktop-sdk.bst:components/xorg-lib-xdamage.bst
- freedesktop-sdk.bst:components/xorg-lib-xfixes.bst
- freedesktop-sdk.bst:components/xorg-lib-xrandr.bst
- freedesktop-sdk.bst:components/xorg-lib-xshmfence.bst
- freedesktop-sdk.bst:components/xorg-lib-xxf86vm.bst
- freedesktop-sdk.bst:components/wayland.bst
- freedesktop-sdk.bst:components/libglvnd.bst
- freedesktop-sdk.bst:components/libvdpau.bst
- freedesktop-sdk.bst:components/libunwind.bst
- sdk/libdrm.bst

variables:
  (?):
  - arch == "i686" or arch == "x86_64":
      gallium_drivers: iris,nouveau,r300,r600,radeonsi,svga,swrast,virgl,zink
      dri_drivers: i915,i965,nouveau,r100,r200
      vulkan_drivers: amd,intel
      enable_libunwind: 'true'
  - arch == "arm" or arch == "aarch64":
      gallium_drivers: etnaviv,freedreno,kmsro,lima,nouveau,panfrost,swrast,tegra,virgl,v3d,vc4,zink
      dri_drivers: ''
      vulkan_drivers: freedreno
      enable_libunwind: 'false'
  - arch == "ppc64le":
      gallium_drivers: nouveau,r600,r300,radeonsi,swrast,virgl
      dri_drivers: r100,r200,nouveau
      vulkan_drivers: amd
      enable_libunwind: 'false'

  meson-local: >-
    -Db_ndebug=false
    -Ddri3=true
    -Ddri-drivers=%{dri_drivers}
    -Degl=true
    -Dgallium-drivers=%{gallium_drivers}
    -Dgallium-nine=true
    -Dgallium-omx=disabled
    -Dgallium-opencl=icd
    -Dgallium-va=true
    -Dgallium-vdpau=true
    -Dgallium-xa=false
    -Dgallium-xvmc=false
    -Dgbm=true
    -Dgles1=false
    -Dgles2=true
    -Dglvnd=true
    -Dglx=dri
    -Dosmesa=gallium
    -Dlibunwind=%{enable_libunwind}
    -Dllvm=true
    -Dlmsensors=false
    -Dplatforms=x11,drm,surfaceless,wayland
    -Dselinux=false
    -Dshared-glapi=true
    -Dvalgrind=false
    -Dvulkan-overlay-layer=true
    -Dvulkan-drivers=%{vulkan_drivers}
    -Dvulkan-icd-dir="%{libdir}/vulkan/icd.d"
    -Dxlib-lease=true
    -Dprefer-iris=false

config:
  install-commands:
    (>):
    - |
      mkdir -p "%{install-root}%{libdir}/GL"
      ln -sr '%{install-root}%{datadir}/glvnd' '%{install-root}%{libdir}/GL/glvnd'


public:
  bst:
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libgbm.so'
        - '%{libdir}/libglapi.so'
        - '%{libdir}/libwayland-egl.so'
        - '%{libdir}/libMesaOpenCL.so'
        - '%{libdir}/d3d/d3dadapter9.so'
  cpe:
    product: mesa
    vendor: mesa3d

sources:
- kind: git_tag
  url: gitlab_freedesktop_org:mesa/mesa.git
  track: '20.1'
  ref: mesa-20.1.5-0-g3ba7f954b8a4ca6a8754fec06b8c0d179f2db64a
- kind: patch
  path: patches/mesa/mesa_libdrm_deps.patch
