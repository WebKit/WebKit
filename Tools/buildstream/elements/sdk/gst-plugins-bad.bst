kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-bad/gst-plugins-bad-1.16.2.tar.xz
  ref: f1cb7aa2389569a5343661aae473f0a940a90b872001824bc47fa8072a041e74
- kind: patch
  path: patches/gst-plugins-bad-0001-h264parse-Post-a-WARNING-when-data-is-broken.patch

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- sdk/gstreamer.bst
- sdk/gst-plugins-base.bst
- sdk/openjpeg.bst
- sdk/libkate.bst
- sdk/libusrsctp.bst
- sdk-build-depends/libusb.bst
- filtered-openh264.bst
- freedesktop-sdk.bst:components/aom.bst
- freedesktop-sdk.bst:components/librsvg.bst
- freedesktop-sdk.bst:components/frei0r.bst
- freedesktop-sdk.bst:components/ladspa-sdk.bst
- freedesktop-sdk.bst:components/libdrm.bst
- freedesktop-sdk.bst:components/libfdk-aac.bst
- freedesktop-sdk.bst:components/libglvnd.bst
- freedesktop-sdk.bst:components/libnice.bst
- freedesktop-sdk.bst:components/libsrtp2.bst
- freedesktop-sdk.bst:components/libwebp.bst
- freedesktop-sdk.bst:components/libgudev.bst
- freedesktop-sdk.bst:components/openal.bst
- freedesktop-sdk.bst:components/orc.bst
- freedesktop-sdk.bst:components/sndfile.bst
- freedesktop-sdk.bst:components/wayland.bst
- freedesktop-sdk.bst:components/webrtc-audio-processing.bst
- freedesktop-sdk.bst:bootstrap-import.bst

variables:
  meson-local: >-
    -Dpackage-origin="webkit-sdk"
    -Dintrospection=disabled
    -Dexamples=disabled
    -Dassrender=disabled
    -Dbluez=disabled
    -Dbs2b=disabled
    -Dchromaprint=disabled
    -Dcolormanagement=disabled
    -Dcuda=disabled
    -Dcurl-ssh2=disabled
    -Dcurl=disabled
    -Dd3dvideosink=disabled
    -Ddc1394=disabled
    -Ddecklink=disabled
    -Ddirectfb=disabled
    -Ddirectsound=disabled
    -Ddts=disabled
    -Ddvb=disabled
    -Dfaac=disabled
    -Dfaad=disabled
    -Dfbdev=disabled
    -Dflite=disabled
    -Dfluidsynth=disabled
    -Dgme=disabled
    -Dgsm=disabled
    -Diqa=disabled
    -Dladspa=disabled
    -Dlibde265=disabled
    -Dlibmms=disabled
    -Dlv2=disabled
    -Dmodplug=disabled
    -Dmpeg2enc=disabled
    -Dmplex=disabled
    -Dmsdk=disabled
    -Dmusepack=disabled
    -Dneon=disabled
    -Dnvcodec=disabled
    -Dnvdec=disabled
    -Dnvenc=disabled
    -Dofa=disabled
    -Dopenal=disabled
    -Dopencv=disabled
    -Dopenexr=disabled
    -Dopenh264=disabled
    -Dopenmpt=disabled
    -Dopenni2=disabled
    -Dopensles=disabled
    -Dopensles=disabled
    -Dresindvd=disabled
    -Drsvg=disabled
    -Drtmp=disabled
    -Dsbc=disabled
    -Dsndfile=disabled
    -Dsoundtouch=disabled
    -Dspandsp=disabled
    -Dsrt=disabled
    -Dteletext=disabled
    -Dtinyalsa=disabled
    -Dttml=disabled
    -Dvaacenc=disabled
    -Dvdpau=disabled
    -Dvoaacenc=disabled
    -Dvoamrwbenc=disabled
    -Dvulkan=disabled
    -Dwasapi=disabled
    -Dwebp=disabled
    -Dwebrtcdsp=disabled
    -Dwildmidi=disabled
    -Dwinks=disabled
    -Dwinscreencap=disabled
    -Dwpe=disabled
    -Dx265=disabled
    -Dzbar=disabled
    
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
