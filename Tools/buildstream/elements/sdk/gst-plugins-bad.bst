kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-bad/gst-plugins-bad-1.20.4.tar.xz
  ref: a1a3f53b3604d9a04fdd0bf9a1a616c3d2dab5320489e9ecee1178e81e33a16a
# https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/merge_requests/2341/
- kind: patch
  path: patches/gst-plugins-bad-mr-2341.patch
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- sdk/gstreamer.bst
- sdk/gst-plugins-base.bst
- freedesktop-sdk.bst:components/openjpeg.bst
- freedesktop-sdk.bst:components/libdrm.bst
- sdk/libkate.bst
- sdk/libopenh264.bst
- sdk/libusrsctp.bst
- freedesktop-sdk.bst:components/libusb.bst
- freedesktop-sdk.bst:components/aom.bst
- freedesktop-sdk.bst:components/librsvg.bst
- freedesktop-sdk.bst:components/frei0r.bst
- freedesktop-sdk.bst:components/ladspa-sdk.bst
- freedesktop-sdk.bst:components/libfdk-aac.bst
- freedesktop-sdk.bst:components/libglvnd.bst
- freedesktop-sdk.bst:components/libnice.bst
- freedesktop-sdk.bst:components/libsrtp2.bst
- freedesktop-sdk.bst:components/libwebp.bst
- freedesktop-sdk.bst:components/libgudev.bst
- freedesktop-sdk.bst:components/libva.bst
- freedesktop-sdk.bst:components/openal.bst
- freedesktop-sdk.bst:components/orc.bst
- freedesktop-sdk.bst:components/sndfile.bst
- freedesktop-sdk.bst:components/wayland.bst
- sdk/webrtc-audio-processing-1.bst
- freedesktop-sdk.bst:bootstrap-import.bst

variables:
  meson-local: >-
    -Dpackage-origin="webkit-sdk"
    -Dintrospection=disabled
    -Dexamples=disabled
    -Dassrender=disabled
    -Davtp=disabled
    -Dbluez=disabled
    -Dbs2b=disabled
    -Dchromaprint=disabled
    -Dcolormanagement=disabled
    -Dcurl-ssh2=disabled
    -Dcurl=disabled
    -Dd3dvideosink=disabled
    -Ddc1394=disabled
    -Ddecklink=disabled
    -Ddirectfb=disabled
    -Ddirectsound=disabled
    -Ddoc=disabled
    -Ddts=disabled
    -Ddvb=disabled
    -Dfaac=disabled
    -Dfaad=disabled
    -Dfbdev=disabled
    -Dflite=disabled
    -Dfluidsynth=disabled
    -Dgme=disabled
    -Dgs=disabled
    -Dgsm=disabled
    -Diqa=disabled
    -Dladspa=disabled
    -Dldac=disabled
    -Dlibde265=disabled
    -Dlv2=disabled
    -Dmagicleap=disabled
    -Dmicrodns=disabled
    -Dmodplug=disabled
    -Dmpeg2enc=disabled
    -Dmplex=disabled
    -Dmsdk=disabled
    -Dmusepack=disabled
    -Dneon=disabled
    -Dnvcodec=disabled
    -Donnx=disabled
    -Dopenal=disabled
    -Dopenaptx=disabled
    -Dopencv=disabled
    -Dopenexr=disabled
    -Dopenmpt=disabled
    -Dopenni2=disabled
    -Dopensles=disabled
    -Dqroverlay=disabled
    -Dresindvd=disabled
    -Drsvg=disabled
    -Drtmp=disabled
    -Dsbc=disabled
    -Dsndfile=disabled
    -Dsoundtouch=disabled
    -Dspandsp=disabled
    -Dsrt=disabled
    -Dsvthevcenc=disabled
    -Dteletext=disabled
    -Dtinyalsa=disabled
    -Dttml=disabled
    -Dvoaacenc=disabled
    -Dvoamrwbenc=disabled
    -Dvulkan=disabled
    -Dwasapi=disabled
    -Dwasapi2=disabled
    -Dwebp=disabled
    -Dwildmidi=disabled
    -Dwinks=disabled
    -Dwinscreencap=disabled
    -Dwpe=disabled
    -Dx265=disabled
    -Dzbar=disabled
    -Dzxing=disabled
public:
  bst:
    split-rules:
      devel:
        (>):
        - '%{libdir}/libgstadaptivedemux-1.0.so'
        - '%{libdir}/libgstbadaudio-1.0.so'
        - '%{libdir}/libgstbasecamerabinsrc-1.0.so'
        - '%{libdir}/libgstcodecparsers-1.0.so'
        - '%{libdir}/libgstinsertbin-1.0.so'
        - '%{libdir}/libgstisoff-1.0.so'
        - '%{libdir}/libgstmpegts-1.0.so'
        - '%{libdir}/libgstphotography-1.0.so'
        - '%{libdir}/libgstplayer-1.0.so'
        - '%{libdir}/libgstsctp-1.0.so'
        - '%{libdir}/libgsturidownloader-1.0.so'
        - '%{libdir}/libgstwayland-1.0.so'
        - '%{libdir}/libgstwebrtc-1.0.so'
