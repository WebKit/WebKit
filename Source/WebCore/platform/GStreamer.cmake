if (ENABLE_VIDEO OR ENABLE_WEB_AUDIO)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/mediastream/gstreamer"
        "${WEBCORE_DIR}/platform/graphics/gstreamer"
        "${WEBCORE_DIR}/platform/graphics/gstreamer/mse"
        "${WEBCORE_DIR}/platform/graphics/gstreamer/eme"
        "${WEBCORE_DIR}/platform/gstreamer"
        "${WEBCORE_DIR}/platform/mediarecorder/gstreamer"
    )

    list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
        "platform/SourcesGStreamer.txt"
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/audio/gstreamer/AudioDestinationGStreamer.h

        platform/gstreamer/GStreamerCodecUtilities.h
        platform/gstreamer/GStreamerElementHarness.h
        platform/graphics/gstreamer/GRefPtrGStreamer.h
        platform/graphics/gstreamer/GStreamerCommon.h
        platform/graphics/gstreamer/GUniquePtrGStreamer.h

        platform/mediastream/gstreamer/GStreamerWebRTCProvider.h
        platform/mediastream/libwebrtc/gstreamer/GStreamerVideoDecoderFactory.h
        platform/mediastream/libwebrtc/gstreamer/GStreamerVideoEncoderFactory.h
        platform/mediastream/libwebrtc/gstreamer/LibWebRTCProviderGStreamer.h
    )

    if (USE_GSTREAMER_FULL)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${GSTREAMER_FULL_INCLUDE_DIRS}
        )
        list(APPEND WebCore_LIBRARIES
            ${GSTREAMER_FULL_LIBRARIES}
        )
    else ()
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${GSTREAMER_INCLUDE_DIRS}
            ${GSTREAMER_BASE_INCLUDE_DIRS}
            ${GSTREAMER_ALLOCATORS_INCLUDE_DIRS}
            ${GSTREAMER_APP_INCLUDE_DIRS}
            ${GSTREAMER_PBUTILS_INCLUDE_DIRS}
        )

        list(APPEND WebCore_LIBRARIES
            ${GSTREAMER_ALLOCATORS_LIBRARIES}
            ${GSTREAMER_APP_LIBRARIES}
            ${GSTREAMER_BASE_LIBRARIES}
            ${GSTREAMER_LIBRARIES}
            ${GSTREAMER_PBUTILS_LIBRARIES}
            ${GSTREAMER_AUDIO_LIBRARIES}
        )
    endif ()

    # Avoiding a GLib deprecation warning due to GStreamer API using deprecated classes.
    set_source_files_properties(platform/audio/gstreamer/WebKitWebAudioSourceGStreamer.cpp PROPERTIES COMPILE_DEFINITIONS "GLIB_DISABLE_DEPRECATION_WARNINGS=1")

    if (VIDEO_DECODING_LIMIT)
        # Specify video decoding limits for platform/graphics/gstreamer/GStreamerRegistryScanner.cpp
        list(APPEND WebCore_PRIVATE_DEFINITIONS VIDEO_DECODING_LIMIT="${VIDEO_DECODING_LIMIT}")
    endif ()
endif ()

if (USE_GSTREAMER_TRANSCODER)
    if (NOT USE_GSTREAMER_FULL)
        list(APPEND WebCore_LIBRARIES
            ${GSTREAMER_TRANSCODER_LIBRARIES}
        )
    endif ()
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${GSTREAMER_TRANSCODER_INCLUDE_DIRS}
    )
endif ()

if (ENABLE_VIDEO)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${GSTREAMER_GL_INCLUDE_DIRS}
        ${GSTREAMER_TAG_INCLUDE_DIRS}
        ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    )

    if (NOT USE_GSTREAMER_FULL)
        list(APPEND WebCore_LIBRARIES
           ${GSTREAMER_GL_LIBRARIES}
           ${GSTREAMER_TAG_LIBRARIES}
           ${GSTREAMER_VIDEO_LIBRARIES}
        )
    endif ()

    if (USE_GSTREAMER_MPEGTS AND NOT USE_GSTREAMER_FULL)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${GSTREAMER_MPEGTS_INCLUDE_DIRS}
        )
        list(APPEND WebCore_LIBRARIES
            ${GSTREAMER_MPEGTS_LIBRARIES}
        )
    endif ()

    if (USE_LIBWEBRTC)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${GSTREAMER_CODECPARSERS_INCLUDE_DIRS}
        )
        if (NOT USE_GSTREAMER_FULL)
            list(APPEND WebCore_LIBRARIES
                ${GSTREAMER_CODECPARSERS_LIBRARIES}
            )
        endif ()
    elseif (USE_GSTREAMER_WEBRTC)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${GSTREAMER_RTP_INCLUDE_DIRS}
            ${GSTREAMER_SDP_INCLUDE_DIRS}
            ${GSTREAMER_WEBRTC_INCLUDE_DIRS}
        )
        if (NOT USE_GSTREAMER_FULL)
            list(APPEND WebCore_LIBRARIES
                ${GSTREAMER_RTP_LIBRARIES}
                ${GSTREAMER_SDP_LIBRARIES}
                ${GSTREAMER_WEBRTC_LIBRARIES}
            )
        endif ()

        list(APPEND WebCore_LIBRARIES OpenSSL::Crypto)
    endif ()
endif ()

if (ENABLE_MEDIA_STREAM OR ENABLE_WEB_AUDIO OR ENABLE_WEB_CODECS)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/audio/gstreamer"
    )
endif ()

if (ENABLE_WEB_AUDIO)
    if (NOT USE_GSTREAMER_FULL)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${GSTREAMER_AUDIO_INCLUDE_DIRS}
            ${GSTREAMER_FFT_INCLUDE_DIRS}
        )

        list(APPEND WebCore_LIBRARIES
            ${GSTREAMER_FFT_LIBRARIES}
        )
    endif ()
endif ()

if (ENABLE_ENCRYPTED_MEDIA AND ENABLE_THUNDER)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${THUNDER_INCLUDE_DIRS}
    )

    list(APPEND WebCore_LIBRARIES
        ${THUNDER_LIBRARIES}
    )
endif ()

if (ENABLE_SPEECH_SYNTHESIS)
    if (USE_SPIEL)
        list(APPEND WebCore_SOURCES
            platform/spiel/PlatformSpeechSynthesizerSpiel.cpp
        )
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${SPIEL_INCLUDE_DIRS}
        )
        list(APPEND WebCore_LIBRARIES
            LibSpiel::LibSpiel
        )
    elseif (USE_FLITE)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${Flite_INCLUDE_DIRS}
        )
        list(APPEND WebCore_LIBRARIES
            ${Flite_LIBRARIES}
        )
    endif ()
endif ()

