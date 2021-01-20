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
- filename: freedesktop-sdk.bst:components/nasm.bst
  type: build
- filename: freedesktop-sdk.bst:components/noopenh264.bst

variables:
  ffmpeg-prefix: '%{prefix}'
  ffmpeg-libdir: '%{libdir}'
  ffmpeg-arch: '%{arch}'

  audio-encoders: |
    ac3,alac,flac,libfdk_aac,g723_1,mp2,libmp3lame,libopus,pcm_alaw,pcm_mulaw,\
    pcm_f32le,pcm_s16be,pcm_s24be,pcm_s16le,pcm_s24le,pcm_s32le,\
    pcm_u8,tta,libvorbis,wavpack,%{extra-aud-enc}

  audio-decoders: |
    ac3,alac,flac,g723_1,g729,libfdk_aac,libopus,mp2,mp3,m4a,pcm_alaw,pcm_mulaw,\
    pcm_f32le,pcm_s16be,pcm_s24be,pcm_s16le,pcm_s24le,pcm_s32le,\
    pcm_u8,tta,vorbis,wavpack,%{extra-aud-dec}

  video-encoders: |
    ass,ffv1,libaom_av1,libopenh264,libvpx_vp8,libvpx_vp9,rawvideo,theora,%{extra-vid-enc}

  video-decoders: |
    ass,ffv1,libaom_av1,libdav1d,libopenh264,libvpx_vp8,libvpx_vp9,rawvideo,theora,vp8,vp9,%{extra-vid-dec}

  image-formats: |
    gif,png,tiff,webp,%{extra-image-formats}

  hwaccels: '%{extra-hwaccels}'

  muxers: |
    ac3,ass,flac,gif,matroska,mp3,mpegvideo,ogg,opus,wav,%{extra-muxers}

  demuxers: |
    aac,ac3,ass,flac,gif,matroska,mov,mp3,mpegvideo,ogg,wav,%{extra-demuxers}

  parsers: |
    aac,ac3,flac,mpegaudio,mpeg4video,opus,vp3,vp8,vorbis,%{extra-parsers}

  filters: |
    crop,scale,%{extra-filters}

  conf-local: |
    --prefix="%{ffmpeg-prefix}" \
    --libdir="%{ffmpeg-libdir}" \
    --disable-doc \
    --disable-static \
    --enable-optimizations \
    --enable-shared \
    --disable-everything \
    --enable-ffplay \
    --enable-ffprobe \
    --enable-gnutls \
    --enable-libaom \
    --enable-libdav1d \
    --enable-libfdk-aac \
    --enable-libmp3lame \
    --enable-libfontconfig \
    --enable-libfreetype \
    --enable-libopenh264 \
    --enable-libopus \
    --enable-libpulse \
    --enable-libspeex \
    --enable-libtheora \
    --enable-libvorbis \
    --enable-libvpx \
    --enable-libwebp \
    --enable-openal \
    --enable-opengl \
    --enable-sdl2 \
    --enable-encoder=%{audio-encoders} \
    --enable-encoder=%{video-encoders} \
    --enable-decoder=%{audio-decoders} \
    --enable-decoder=%{video-decoders} \
    --enable-encoder=%{image-formats} \
    --enable-decoder=%{image-formats} \
    --enable-hwaccel=%{hwaccels} \
    --enable-parser=%{parsers} \
    --enable-muxer=%{muxers} \
    --enable-demuxer=%{demuxers} \
    --enable-filter=%{filters} \
    --enable-protocol=file \
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
  track: release/4.2
  ref: n4.2.3-5-g221e490d42bc1c20c61aee385d5bfe003a1a8c87
