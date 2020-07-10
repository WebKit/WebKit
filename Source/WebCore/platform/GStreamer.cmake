if (ENABLE_VIDEO OR ENABLE_WEB_AUDIO)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/gstreamer"
        "${WEBCORE_DIR}/platform/graphics/gstreamer/mse"
        "${WEBCORE_DIR}/platform/graphics/gstreamer/eme"
    )

    list(APPEND WebCore_SOURCES
        platform/graphics/gstreamer/AudioTrackPrivateGStreamer.cpp
        platform/graphics/gstreamer/GLVideoSinkGStreamer.cpp
        platform/graphics/gstreamer/GRefPtrGStreamer.cpp
        platform/graphics/gstreamer/GStreamerCommon.cpp
        platform/graphics/gstreamer/GstAllocatorFastMalloc.cpp
        platform/graphics/gstreamer/GStreamerRegistryScanner.cpp
        platform/graphics/gstreamer/GStreamerVideoFrameHolder.cpp
        platform/graphics/gstreamer/ImageDecoderGStreamer.cpp
        platform/graphics/gstreamer/InbandTextTrackPrivateGStreamer.cpp
        platform/graphics/gstreamer/MediaEngineConfigurationFactoryGStreamer.cpp
        platform/graphics/gstreamer/MediaPlayerPrivateGStreamer.cpp
        platform/graphics/gstreamer/MediaSampleGStreamer.cpp
        platform/graphics/gstreamer/TextCombinerGStreamer.cpp
        platform/graphics/gstreamer/TextSinkGStreamer.cpp
        platform/graphics/gstreamer/TrackPrivateBaseGStreamer.cpp
        platform/graphics/gstreamer/VideoSinkGStreamer.cpp
        platform/graphics/gstreamer/VideoTrackPrivateGStreamer.cpp
        platform/graphics/gstreamer/WebKitWebSourceGStreamer.cpp

        platform/graphics/gstreamer/eme/WebKitClearKeyDecryptorGStreamer.cpp
        platform/graphics/gstreamer/eme/WebKitCommonEncryptionDecryptorGStreamer.cpp

        platform/graphics/gstreamer/mse/AppendPipeline.cpp
        platform/graphics/gstreamer/mse/GStreamerMediaDescription.cpp
        platform/graphics/gstreamer/mse/GStreamerRegistryScannerMSE.cpp
        platform/graphics/gstreamer/mse/MediaPlayerPrivateGStreamerMSE.cpp
        platform/graphics/gstreamer/mse/MediaSourcePrivateGStreamer.cpp
        platform/graphics/gstreamer/mse/PlaybackPipeline.cpp
        platform/graphics/gstreamer/mse/SourceBufferPrivateGStreamer.cpp
        platform/graphics/gstreamer/mse/WebKitMediaSourceGStreamer.cpp

        platform/mediastream/libwebrtc/GStreamerVideoDecoderFactory.cpp
        platform/mediastream/libwebrtc/GStreamerVideoEncoder.cpp
        platform/mediastream/libwebrtc/GStreamerVideoEncoderFactory.cpp
        platform/mediastream/libwebrtc/LibWebRTCAudioModule.cpp
        platform/mediastream/libwebrtc/LibWebRTCProviderGStreamer.cpp

        platform/mediastream/gstreamer/GStreamerAudioCaptureSource.cpp
        platform/mediastream/gstreamer/GStreamerAudioCapturer.cpp
        platform/mediastream/gstreamer/GStreamerCaptureDeviceManager.cpp
        platform/mediastream/gstreamer/GStreamerCapturer.cpp
        platform/mediastream/gstreamer/GStreamerMediaStreamSource.cpp
        platform/mediastream/gstreamer/GStreamerVideoCaptureSource.cpp
        platform/mediastream/gstreamer/GStreamerVideoCapturer.cpp
        platform/mediastream/gstreamer/GStreamerVideoFrameLibWebRTC.cpp
        platform/mediastream/gstreamer/MockRealtimeAudioSourceGStreamer.cpp
        platform/mediastream/gstreamer/MockRealtimeVideoSourceGStreamer.cpp
        platform/mediastream/gstreamer/RealtimeIncomingAudioSourceLibWebRTC.cpp
        platform/mediastream/gstreamer/RealtimeIncomingVideoSourceLibWebRTC.cpp
        platform/mediastream/gstreamer/RealtimeMediaSourceCenterLibWebRTC.cpp
        platform/mediastream/gstreamer/RealtimeOutgoingAudioSourceLibWebRTC.cpp
        platform/mediastream/gstreamer/RealtimeOutgoingVideoSourceLibWebRTC.cpp
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/gstreamer/GRefPtrGStreamer.h
        platform/graphics/gstreamer/GStreamerCommon.h
        platform/graphics/gstreamer/GUniquePtrGStreamer.h
        platform/graphics/gstreamer/MediaPlayerRequestInstallMissingPluginsCallback.h

        platform/mediastream/libwebrtc/GStreamerVideoDecoderFactory.h
        platform/mediastream/libwebrtc/GStreamerVideoEncoderFactory.h
        platform/mediastream/libwebrtc/LibWebRTCProviderGStreamer.h
    )

    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${GSTREAMER_INCLUDE_DIRS}
        ${GSTREAMER_BASE_INCLUDE_DIRS}
        ${GSTREAMER_APP_INCLUDE_DIRS}
        ${GSTREAMER_PBUTILS_INCLUDE_DIRS}
    )

    list(APPEND WebCore_LIBRARIES
        ${GSTREAMER_APP_LIBRARIES}
        ${GSTREAMER_BASE_LIBRARIES}
        ${GSTREAMER_LIBRARIES}
        ${GSTREAMER_PBUTILS_LIBRARIES}
        ${GSTREAMER_AUDIO_LIBRARIES}
    )

    # Avoiding a GLib deprecation warning due to GStreamer API using deprecated classes.
    set_source_files_properties(platform/audio/gstreamer/WebKitWebAudioSourceGStreamer.cpp PROPERTIES COMPILE_DEFINITIONS "GLIB_DISABLE_DEPRECATION_WARNINGS=1")
endif ()

if (ENABLE_VIDEO)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${GSTREAMER_TAG_INCLUDE_DIRS}
        ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    )

    list(APPEND WebCore_LIBRARIES
        ${GSTREAMER_TAG_LIBRARIES}
        ${GSTREAMER_VIDEO_LIBRARIES}
    )

    if (USE_GSTREAMER_MPEGTS)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${GSTREAMER_MPEGTS_INCLUDE_DIRS}
        )
        list(APPEND WebCore_LIBRARIES
            ${GSTREAMER_MPEGTS_LIBRARIES}
        )
    endif ()

    if (USE_GSTREAMER_GL)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${GSTREAMER_GL_INCLUDE_DIRS}
        )
        list(APPEND WebCore_LIBRARIES
            ${GSTREAMER_GL_LIBRARIES}
        )
        list(APPEND WebCore_SOURCES
            platform/graphics/gstreamer/PlatformDisplayGStreamer.cpp
            platform/graphics/gstreamer/VideoTextureCopierGStreamer.cpp
        )
    endif ()

    if (ENABLE_MEDIA_STREAM OR ENABLE_WEB_RTC)
        if (PC_GSTREAMER_VERSION VERSION_LESS "1.10")
            message(FATAL_ERROR "GStreamer 1.10 is needed for ENABLE_MEDIA_STREAM or ENABLE_WEB_RTC")
        else ()
            list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
                ${GSTREAMER_CODECPARSERS_INCLUDE_DIRS}
            )
            list(APPEND WebCore_LIBRARIES
                ${GSTREAMER_CODECPARSERS_LIBRARIES}
            )
        endif ()
    endif ()
endif ()

if (ENABLE_WEB_AUDIO)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/audio/gstreamer"
    )

    list(APPEND WebCore_SOURCES
        platform/audio/gstreamer/AudioDestinationGStreamer.cpp
        platform/audio/gstreamer/AudioFileReaderGStreamer.cpp
        platform/audio/gstreamer/AudioSourceProviderGStreamer.cpp
        platform/audio/gstreamer/FFTFrameGStreamer.cpp
        platform/audio/gstreamer/WebKitWebAudioSourceGStreamer.cpp
    )

    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${GSTREAMER_AUDIO_INCLUDE_DIRS}
        ${GSTREAMER_FFT_INCLUDE_DIRS}
    )

    list(APPEND WebCore_LIBRARIES
        ${GSTREAMER_FFT_LIBRARIES}
    )
endif ()

if (ENABLE_ENCRYPTED_MEDIA)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/encryptedmedia/clearkey"
    )

    list(APPEND WebCore_SOURCES
        platform/encryptedmedia/CDMProxy.cpp
        platform/encryptedmedia/clearkey/CDMClearKey.cpp
        platform/graphics/gstreamer/eme/CDMFactoryGStreamer.cpp
        platform/graphics/gstreamer/eme/CDMProxyClearKey.cpp
    )

    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${LIBGCRYPT_INCLUDE_DIRS}
    )

    list(APPEND WebCore_LIBRARIES
        ${LIBGCRYPT_LIBRARIES} -lgpg-error
    )
endif ()

if (USE_CAIRO)
    list(APPEND WebCore_SOURCES
        platform/graphics/gstreamer/ImageGStreamerCairo.cpp
    )
endif ()
