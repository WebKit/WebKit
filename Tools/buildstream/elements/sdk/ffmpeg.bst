kind: manual

depends:
- filename: freedesktop-sdk.bst:bootstrap-import.bst
- filename: freedesktop-sdk.bst:components/dav1d.bst
- filename: freedesktop-sdk.bst:components/fontconfig.bst
- filename: freedesktop-sdk.bst:components/freetype.bst
- filename: freedesktop-sdk.bst:components/libfdk-aac.bst
- filename: freedesktop-sdk.bst:components/lame.bst
- filename: freedesktop-sdk.bst:components/libva.bst
- filename: freedesktop-sdk.bst:components/libvdpau.bst
- filename: freedesktop-sdk.bst:components/mpg123.bst
- filename: freedesktop-sdk.bst:components/openal.bst
- filename: freedesktop-sdk.bst:components/libpulse.bst
- filename: freedesktop-sdk.bst:components/sdl2.bst
- filename: freedesktop-sdk.bst:components/aom.bst
- filename: freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
  type: build
- filename: freedesktop-sdk.bst:components/gnutls.bst
- filename: freedesktop-sdk.bst:components/speex.bst
- filename: freedesktop-sdk.bst:components/libtheora.bst
- filename: freedesktop-sdk.bst:components/libvorbis.bst
- filename: freedesktop-sdk.bst:components/libvpx.bst
- filename: freedesktop-sdk.bst:components/opus.bst
- filename: freedesktop-sdk.bst:components/vulkan-icd-loader.bst
- filename: freedesktop-sdk.bst:components/v4l-utils.bst
- filename: freedesktop-sdk.bst:components/xorg-lib-xcb.bst
- filename: freedesktop-sdk.bst:components/nasm.bst
  type: build
- filename: freedesktop-sdk.bst:components/vulkan-headers.bst
  type: build
- filename: freedesktop-sdk.bst:components/noopenh264.bst

variables:
  ffmpeg-prefix: '%{prefix}'
  ffmpeg-libdir: '%{libdir}'
  ffmpeg-arch: '%{arch}'

  audio-encoders: |
    ac3,alac,flac,libfdk_aac,g723_1,mp2,libmp3lame,libopus,libspeex,pcm_alaw,pcm_mulaw,\
    pcm_f32le,pcm_s16be,pcm_s24be,pcm_s16le,pcm_s24le,pcm_s32le,\
    pcm_u8,tta,libvorbis,wavpack,%{extra-aud-enc}

  audio-decoders: |
    ac3,adpcm_g722,alac,flac,g723_1,g729,libfdk_aac,libopus,libspeex,\
    mp2,mp3,m4a,pcm_alaw,pcm_mulaw,pcm_f16le,pcm_f24le,pcm_f32be,\
    pcm_f32le,pcm_f64be,pcm_f64le,pcm_s16be,pcm_s16be_planar,pcm_s24be,\
    pcm_s16le,pcm_s16le_planar,pcm_s24le,pcm_s24le_planar,pcm_s32le,\
    pcm_s32le_planar,pcm_s64be,pcm_s64le,pcm_s8,pcm_s8_planar,\
    pcm_u8,pcm_u24be,pcm_u24le,pcm_u32be,pcm_u32le,tta,vorbis,wavpack,%{extra-aud-dec}

  video-encoders: |
    ass,ffv1,libaom_av1,libopenh264,libvpx_vp8,libvpx_vp9,mjpeg_vaapi,rawvideo,\
    theora,vp8_vaapi,%{extra-vid-enc}

  video-decoders: |
    ass,ffv1,libaom_av1,libdav1d,libopenh264,libvpx_vp8,libvpx_vp9,\
    rawvideo,theora,vp8,vp9,%{extra-vid-dec}

  image-formats: |
    bmp,gif,jpegls,tiff,webp,%{extra-image-formats}

  hwaccels: |
    vp8_vaapi,mjpeg_vaapi,%{extra-hwaccels}

  muxers: |
    ac3,ass,flac,g722,gif,matroska,mp3,mpegvideo,rtp,ogg,opus,\
    pcm_s16be,pcm_s16le,wav,webm,%{extra-muxers}

  demuxers: |
    aac,ac3,ass,flac,g722,gif,image_jpeg_pipe,image_png_pipe,\
    image_webp_pipe,matroska,mjpeg,mov,mp3,mpegvideo,ogg,pcm_mulaw,\
    pcm_alaw,pcm_s16be,pcm_s16le,rtp,wav,%{extra-demuxers}

  parsers: |
    aac,ac3,flac,mjpeg,mpegaudio,mpeg4video,opus,vp3,vp8,vp9,vorbis,%{extra-parsers}

  filters: |
    crop,scale,overlay,amix,amerge,aresample,format,aformat,fps,transpose,pad,%{extra-filters}

  protocols: |
    crypto,file,pipe,rtp,srtp,rtsp,tcp,udp,unix,%{extra-protocols}

  conf-local: >-
    --prefix="%{ffmpeg-prefix}"
    --libdir="%{ffmpeg-libdir}"
    --disable-debug
    --disable-doc
    --disable-static
    --enable-optimizations
    --enable-shared
    --disable-everything
    --enable-ffplay
    --enable-ffprobe
    --enable-gnutls
    --enable-libaom
    --enable-libdav1d
    --enable-libfdk-aac
    --enable-libmp3lame
    --enable-libfontconfig
    --enable-libfreetype
    --enable-libopus
    --enable-libpulse
    --enable-libspeex
    --enable-libtheora
    --enable-libvorbis
    --enable-libvpx
    --enable-libwebp
    --enable-openal
    --enable-opengl
    --enable-sdl2
    --enable-vulkan
    --enable-zlib
    --enable-libv4l2
    --enable-libxcb
    --enable-vdpau
    --enable-vaapi
    --enable-encoder=%{audio-encoders}
    --enable-encoder=%{video-encoders}
    --enable-decoder=%{audio-decoders}
    --enable-decoder=%{video-decoders}
    --enable-encoder=%{image-formats}
    --enable-decoder=%{image-formats}
    --enable-hwaccel=%{hwaccels}
    --enable-parser=%{parsers}
    --enable-muxer=%{muxers}
    --enable-demuxer=%{demuxers}
    --enable-filter=%{filters}
    --enable-protocol=%{protocols}
    --arch="%{ffmpeg-arch}"

  conf-extra: ''

  extra-aud-enc: ''
  extra-aud-dec: ''
  extra-vid-enc: 'h263,h264'
  extra-vid-dec: 'flv,hevc,h263,h264,mpeg2video,mpeg4,msmpeg4,msmpeg4v1,msmpeg4v2,msmpeg4v3,vp6,vp6a,vp6f'
  extra-image-formats: ''
  extra-hwaccels: ''
  extra-muxers: ''
  extra-demuxers: 'avi,h264,m4v'
  extra-parsers: 'hevc,h264'
  extra-filters: ''
  extra-protocols: ''

public:
  bst:
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libavdevice.so'
        - '%{libdir}/libavcodec.so'
        - '%{libdir}/libswscale.so'
        - '%{libdir}/libpostproc.so'
        - '%{libdir}/libavutil.so'
        - '%{libdir}/libavfilter.so'
        - '%{libdir}/libavformat.so'
        - '%{libdir}/libswresample.so'
        - '%{datadir}/ffmpeg/examples'
        - '%{datadir}/ffmpeg/examples/**'

# ffmpeg is not using autotools, but a configure and Makefile files
config:
  configure-commands:
  - ./configure %{conf-local} %{conf-extra}

  build-commands:
  - make

  install-commands:
  - make -j1 DESTDIR="%{install-root}" install

sources:
- kind: git_tag
  url: ffmpeg:ffmpeg.git
  track: release/5.0
  ref: n5.0-0-g390d6853d0ef408007feb39c0040682c81c02751
