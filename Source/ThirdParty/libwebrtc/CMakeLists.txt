cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)
enable_language(ASM)

if (NOT LIBWEBRTC_INPUT_DIR)
    set(LIBWEBRTC_INPUT_DIR ${CMAKE_SOURCE_DIR}/Source)
endif ()
if (NOT LIBWEBRTC_OUTPUT_DIR)
    set(LIBWEBRTC_OUTPUT_DIR ${CMAKE_BINARY_DIR})
endif ()

file(WRITE ${LIBWEBRTC_OUTPUT_DIR}/dummy.c "")
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/third_party/libjpeg_turbo/simd_asm)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/third_party/ffmpeg/ffmpeg_yasm)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/sdk)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/third_party/yasm/include)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/audio_coding/neteq)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/logging/rtc_event_log)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/sdk)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/tools/event_log_visualizer)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/audio_coding/neteq)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/logging/rtc_event_log)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_coding/audio_network_adaptor)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_coding/audio_network_adaptor)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_processing)
file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/tools/event_log_visualizer)

find_library(SECURITY_LIBRARY Security)
find_library(OPENGL_LIBRARY OpenGL)
find_library(APPKIT_LIBRARY AppKit)
find_library(CFNETWORK_LIBRARY CFNetwork)
find_library(COCOA_LIBRARY Cocoa)
find_library(ICUCORE_LIBRARY icucore)
find_library(COREGRAPHICS_LIBRARY CoreGraphics)
find_library(IOKIT_LIBRARY IOKit)
find_library(SYSTEMCONFIGURATION_LIBRARY SystemConfiguration)
find_library(COREMEDIA_LIBRARY CoreMedia)
find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
find_library(FOUNDATION_LIBRARY Foundation)
find_library(COREVIDEO_LIBRARY CoreVideo)
find_library(COREAUDIO_LIBRARY CoreAudio)
find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(QUARTZCORE_LIBRARY QuartzCore)
find_library(AVFOUNDATION_LIBRARY AVFoundation)

# Start of target WebrtcLibjingleXmlliteXmllite
add_custom_target(WebrtcLibjingleXmlliteXmllite)
add_dependencies(WebrtcLibjingleXmlliteXmllite WebrtcLibjingleXmlliteRtc_Xmllite)
# End of target WebrtcLibjingleXmlliteXmllite

# Start of target WebrtcModulesMedia_FileMedia_File
set(WebrtcModulesMedia_FileMedia_File_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/media_file/media_file_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/media_file/media_file_utility.cc)
add_library(WebrtcModulesMedia_FileMedia_File STATIC ${WebrtcModulesMedia_FileMedia_File_SOURCES})
target_compile_options(WebrtcModulesMedia_FileMedia_File PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesMedia_FileMedia_File PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesMedia_FileMedia_File PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesMedia_FileMedia_File PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesMedia_FileMedia_File PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/media_file)
set_target_properties(WebrtcModulesMedia_FileMedia_File PROPERTIES OUTPUT_NAME media_file)
target_link_libraries(WebrtcModulesMedia_FileMedia_File WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesMedia_FileMedia_File WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcModulesMedia_FileMedia_File WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesMedia_FileMedia_File PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesMedia_FileMedia_File ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesMedia_FileMedia_File

# Start of target WebrtcP2PRtc_P2P
set(WebrtcP2PRtc_P2P_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/asyncstuntcpsocket.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/basicpacketsocketfactory.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/dtlstransportchannel.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/p2pconstants.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/p2ptransport.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/p2ptransportchannel.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/port.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/portallocator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/pseudotcp.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/relayport.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/session.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/sessiondescription.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/stun.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/stunport.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/stunrequest.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/tcpport.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/transport.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/transportchannel.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/transportcontroller.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/transportdescription.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/transportdescriptionfactory.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/turnport.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/udptransportchannel.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/client/basicportallocator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/client/socketmonitor.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/relayserver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/stunserver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/base/turnserver.cc)
add_library(WebrtcP2PRtc_P2P STATIC ${WebrtcP2PRtc_P2P_SOURCES})
target_compile_options(WebrtcP2PRtc_P2P PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcP2PRtc_P2P PRIVATE FEATURE_ENABLE_SSL FEATURE_ENABLE_VOICEMAIL FEATURE_ENABLE_PSTN V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC FEATURE_ENABLE_VOICEMAIL XML_STATIC SSL_USE_OPENSSL HAVE_OPENSSL_SSL_H FEATURE_ENABLE_SSL)
target_include_directories(WebrtcP2PRtc_P2P PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/expat/files/lib/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcP2PRtc_P2P PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcP2PRtc_P2P PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/p2p)
set_target_properties(WebrtcP2PRtc_P2P PROPERTIES OUTPUT_NAME rtc_p2p)
target_link_libraries(WebrtcP2PRtc_P2P Third_PartyExpatExpat)
target_link_libraries(WebrtcP2PRtc_P2P WebrtcBaseRtc_Base)
target_include_directories(WebrtcP2PRtc_P2P PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcP2PRtc_P2P ${COCOA_LIBRARY})
target_include_directories(WebrtcP2PRtc_P2P PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcP2PRtc_P2P ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcP2PRtc_P2P PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcP2PRtc_P2P ${IOKIT_LIBRARY})
target_include_directories(WebrtcP2PRtc_P2P PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcP2PRtc_P2P ${SECURITY_LIBRARY})
target_include_directories(WebrtcP2PRtc_P2P PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcP2PRtc_P2P ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcP2PRtc_P2P PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcP2PRtc_P2P ${APPLICATIONSERVICES_LIBRARY})
# End of target WebrtcP2PRtc_P2P






# Start of target WebrtcModulesVideo_CodingVideo_Coding
set(WebrtcModulesVideo_CodingVideo_Coding_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/codec_database.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/codec_timer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/decoding_state.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/encoded_frame.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/frame_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/frame_buffer2.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/frame_object.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/generic_decoder.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/generic_encoder.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/h264_sps_pps_tracker.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/histogram.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/inter_frame_delay.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/jitter_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/jitter_estimator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/media_opt_util.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/media_optimization.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/nack_module.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/packet.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/packet_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/percentile_filter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/protection_bitrate_calculator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/receiver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/rtp_frame_reference_finder.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/rtt_filter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/session_info.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/timestamp_map.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/timing.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/video_coding_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/video_receiver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/video_sender.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/codecs/vp9/vp9_noop.cc)
add_library(WebrtcModulesVideo_CodingVideo_Coding STATIC ${WebrtcModulesVideo_CodingVideo_Coding_SOURCES})
target_compile_options(WebrtcModulesVideo_CodingVideo_Coding PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesVideo_CodingVideo_Coding PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC RTC_DISABLE_VP8 RTC_DISABLE_VP9)
target_include_directories(WebrtcModulesVideo_CodingVideo_Coding PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesVideo_CodingVideo_Coding PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesVideo_CodingVideo_Coding PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/video_coding)
set_target_properties(WebrtcModulesVideo_CodingVideo_Coding PROPERTIES OUTPUT_NAME video_coding)
target_link_libraries(WebrtcModulesVideo_CodingVideo_Coding WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesVideo_CodingVideo_Coding WebrtcCommon_VideoCommon_Video)
target_link_libraries(WebrtcModulesVideo_CodingVideo_Coding WebrtcModulesVideo_CodingVideo_Coding_Utility)
target_link_libraries(WebrtcModulesVideo_CodingVideo_Coding WebrtcModulesVideo_CodingWebrtc_H264)
target_link_libraries(WebrtcModulesVideo_CodingVideo_Coding WebrtcModulesVideo_CodingWebrtc_I420)
target_link_libraries(WebrtcModulesVideo_CodingVideo_Coding WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesVideo_CodingVideo_Coding PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CodingVideo_Coding ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcModulesVideo_CodingVideo_Coding PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CodingVideo_Coding ${COREVIDEO_LIBRARY})
# End of target WebrtcModulesVideo_CodingVideo_Coding





# Start of target WebrtcApiApi
add_custom_target(WebrtcApiApi)
add_dependencies(WebrtcApiApi WebrtcApiLibjingle_Peerconnection)
# End of target WebrtcApiApi

# Start of target Third_PartyLibyuvCompare
set(Third_PartyLibyuvCompare_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/util/compare.cc)
add_executable(Third_PartyLibyuvCompare ${Third_PartyLibyuvCompare_SOURCES})
target_compile_options(Third_PartyLibyuvCompare PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyLibyuvCompare PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1)
target_include_directories(Third_PartyLibyuvCompare PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(Third_PartyLibyuvCompare PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9 -Wl,-ObjC -Wl,-pie")
set_target_properties(Third_PartyLibyuvCompare PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/)
set_target_properties(Third_PartyLibyuvCompare PROPERTIES OUTPUT_NAME compare)
target_link_libraries(Third_PartyLibyuvCompare Third_PartyLibyuvLibyuv)
add_dependencies(Third_PartyLibyuvCompare Third_PartyLibyuvLibyuv)
# End of target Third_PartyLibyuvCompare


# Start of target WebrtcApiAudio_Mixer_Api
add_library(WebrtcApiAudio_Mixer_Api OBJECT)
target_compile_options(WebrtcApiAudio_Mixer_Api PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcApiAudio_Mixer_Api PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcApiAudio_Mixer_Api PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcApiAudio_Mixer_Api PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
# End of target WebrtcApiAudio_Mixer_Api



# Start of target WebrtcModulesAudio_CodingAudio_Coding
set(WebrtcModulesAudio_CodingAudio_Coding_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/acm2/acm_receiver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/acm2/acm_resampler.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/acm2/audio_coding_module.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/acm2/call_statistics.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/acm2/codec_manager.cc
    $<TARGET_OBJECTS:WebrtcLoggingRtc_Event_Log_Api>)
add_library(WebrtcModulesAudio_CodingAudio_Coding STATIC ${WebrtcModulesAudio_CodingAudio_Coding_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingAudio_Coding PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingAudio_Coding PRIVATE WEBRTC_CODEC_ILBC WEBRTC_CODEC_OPUS WEBRTC_CODEC_ISAC WEBRTC_CODEC_RED V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingAudio_Coding PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/red/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/pcm16b/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/cng/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingAudio_Coding PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingAudio_Coding PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingAudio_Coding PROPERTIES OUTPUT_NAME audio_coding)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Coding WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Coding WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Coding WebrtcModulesAudio_CodingCng)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Coding WebrtcModulesAudio_CodingIlbc)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Coding WebrtcModulesAudio_CodingIsac)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Coding WebrtcModulesAudio_CodingNeteq)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Coding WebrtcModulesAudio_CodingPcm16B)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Coding WebrtcModulesAudio_CodingRed)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Coding WebrtcModulesAudio_CodingRent_A_Codec)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Coding WebrtcModulesAudio_CodingWebrtc_Opus)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Coding WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesAudio_CodingAudio_Coding PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingAudio_Coding ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingAudio_Coding

# Start of target Third_PartyLibyuvCpuid
set(Third_PartyLibyuvCpuid_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/util/cpuid.c)
add_executable(Third_PartyLibyuvCpuid ${Third_PartyLibyuvCpuid_SOURCES})
target_compile_options(Third_PartyLibyuvCpuid PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyLibyuvCpuid PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1)
target_include_directories(Third_PartyLibyuvCpuid PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(Third_PartyLibyuvCpuid PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9 -Wl,-ObjC -Wl,-pie")
set_target_properties(Third_PartyLibyuvCpuid PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/)
set_target_properties(Third_PartyLibyuvCpuid PROPERTIES OUTPUT_NAME cpuid)
target_link_libraries(Third_PartyLibyuvCpuid Third_PartyLibyuvLibyuv)
add_dependencies(Third_PartyLibyuvCpuid Third_PartyLibyuvLibyuv)
# End of target Third_PartyLibyuvCpuid







# Start of target WebrtcSystem_WrappersSystem_Wrappers
set(WebrtcSystem_WrappersSystem_Wrappers_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/aligned_malloc.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/clock.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/cpu_features.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/cpu_info.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/event.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/event_timer_posix.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/file_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/logging.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/rtp_to_ntp.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/rw_lock.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/rw_lock_posix.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/sleep.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/sort.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/timestamp_extrapolator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/trace_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/trace_posix.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/atomic32_darwin.cc)
add_library(WebrtcSystem_WrappersSystem_Wrappers STATIC ${WebrtcSystem_WrappersSystem_Wrappers_SOURCES})
target_compile_options(WebrtcSystem_WrappersSystem_Wrappers PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcSystem_WrappersSystem_Wrappers PRIVATE WEBRTC_THREAD_RR V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcSystem_WrappersSystem_Wrappers PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcSystem_WrappersSystem_Wrappers PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcSystem_WrappersSystem_Wrappers PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/system_wrappers)
set_target_properties(WebrtcSystem_WrappersSystem_Wrappers PROPERTIES OUTPUT_NAME system_wrappers)
target_link_libraries(WebrtcSystem_WrappersSystem_Wrappers WebrtcWebrtc_Common)
target_link_libraries(WebrtcSystem_WrappersSystem_Wrappers WebrtcBaseRtc_Base_Approved)
target_include_directories(WebrtcSystem_WrappersSystem_Wrappers PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcSystem_WrappersSystem_Wrappers ${FOUNDATION_LIBRARY})
# End of target WebrtcSystem_WrappersSystem_Wrappers




# Start of target WebrtcModulesAudio_MixerAudio_Mixer_Impl
set(WebrtcModulesAudio_MixerAudio_Mixer_Impl_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_mixer/audio_mixer_impl.cc
    $<TARGET_OBJECTS:WebrtcApiAudio_Mixer_Api>)
add_library(WebrtcModulesAudio_MixerAudio_Mixer_Impl STATIC ${WebrtcModulesAudio_MixerAudio_Mixer_Impl_SOURCES})
target_compile_options(WebrtcModulesAudio_MixerAudio_Mixer_Impl PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_MixerAudio_Mixer_Impl PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_MixerAudio_Mixer_Impl PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_MixerAudio_Mixer_Impl PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_MixerAudio_Mixer_Impl PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_mixer)
set_target_properties(WebrtcModulesAudio_MixerAudio_Mixer_Impl PROPERTIES OUTPUT_NAME audio_mixer_impl)
target_link_libraries(WebrtcModulesAudio_MixerAudio_Mixer_Impl WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesAudio_MixerAudio_Mixer_Impl WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcModulesAudio_MixerAudio_Mixer_Impl WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcModulesAudio_MixerAudio_Mixer_Impl WebrtcModulesAudio_MixerAudio_Frame_Manipulator)
target_link_libraries(WebrtcModulesAudio_MixerAudio_Mixer_Impl WebrtcModulesAudio_ProcessingAudio_Processing)
target_link_libraries(WebrtcModulesAudio_MixerAudio_Mixer_Impl WebrtcModulesUtilityUtility)
target_link_libraries(WebrtcModulesAudio_MixerAudio_Mixer_Impl WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesAudio_MixerAudio_Mixer_Impl PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_MixerAudio_Mixer_Impl ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_MixerAudio_Mixer_Impl



# Start of target WebrtcModulesAudio_CodingAudio_Format
set(WebrtcModulesAudio_CodingAudio_Format_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/audio_format.cc)
add_library(WebrtcModulesAudio_CodingAudio_Format STATIC ${WebrtcModulesAudio_CodingAudio_Format_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingAudio_Format PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingAudio_Format PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingAudio_Format PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingAudio_Format PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingAudio_Format PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingAudio_Format PROPERTIES OUTPUT_NAME audio_format)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Format WebrtcWebrtc_Common)
# End of target WebrtcModulesAudio_CodingAudio_Format


# Start of target WebrtcLoggingLogging
add_custom_target(WebrtcLoggingLogging)
add_dependencies(WebrtcLoggingLogging WebrtcLoggingRtc_Event_Log_Impl WebrtcLoggingRtc_Event_Log_Parser)
# End of target WebrtcLoggingLogging


# Start of target Third_PartyYasmGenerate_X86_Insn
add_custom_command(OUTPUT ${LIBWEBRTC_OUTPUT_DIR}/gen/third_party/yasm/include/x86insns.c ${LIBWEBRTC_OUTPUT_DIR}/gen/third_party/yasm/include/x86insn_gas.gperf ${LIBWEBRTC_OUTPUT_DIR}/gen/third_party/yasm/include/x86insn_nasm.gperf
    COMMAND python ${LIBWEBRTC_INPUT_DIR}/third_party/yasm/source/patched-yasm/modules/arch/x86/gen_x86_insn.py ${LIBWEBRTC_OUTPUT_DIR}/gen/third_party/yasm/include
    VERBATIM)
add_custom_target(Third_PartyYasmGenerate_X86_Insn DEPENDS  ${LIBWEBRTC_OUTPUT_DIR}/gen/third_party/yasm/include/x86insns.c ${LIBWEBRTC_OUTPUT_DIR}/gen/third_party/yasm/include/x86insn_gas.gperf ${LIBWEBRTC_OUTPUT_DIR}/gen/third_party/yasm/include/x86insn_nasm.gperf)
# End of target Third_PartyYasmGenerate_X86_Insn


# Start of target WebrtcToolsChart_Proto_Gen
add_custom_command(OUTPUT ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/tools/event_log_visualizer/chart.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/tools/event_log_visualizer/chart.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/tools/event_log_visualizer/chart_pb2.py
    DEPENDS Third_PartyProtobufProtoc
    COMMAND python ${LIBWEBRTC_INPUT_DIR}/tools/protoc_wrapper/protoc_wrapper.py chart.proto --protoc ./protoc --proto-in-dir ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/event_log_visualizer --cc-out-dir ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/tools/event_log_visualizer --py-out-dir pyproto/webrtc/tools/event_log_visualizer
    VERBATIM)
add_custom_target(WebrtcToolsChart_Proto_Gen DEPENDS Third_PartyProtobufProtoc ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/tools/event_log_visualizer/chart.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/tools/event_log_visualizer/chart.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/tools/event_log_visualizer/chart_pb2.py)
# End of target WebrtcToolsChart_Proto_Gen

# Start of target WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto_Gen
add_custom_command(OUTPUT ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing/unittest.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing/unittest.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_processing/unittest_pb2.py
    DEPENDS Third_PartyProtobufProtoc
    COMMAND python ${LIBWEBRTC_INPUT_DIR}/tools/protoc_wrapper/protoc_wrapper.py unittest.proto --protoc ./protoc --proto-in-dir ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/test --cc-out-dir ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing --py-out-dir pyproto/webrtc/modules/audio_processing
    VERBATIM)
add_custom_target(WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto_Gen DEPENDS Third_PartyProtobufProtoc ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing/unittest.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing/unittest.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_processing/unittest_pb2.py)
# End of target WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto_Gen


# Start of target WebrtcSystem_WrappersField_Trial_Default
set(WebrtcSystem_WrappersField_Trial_Default_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/field_trial_default.cc)
add_library(WebrtcSystem_WrappersField_Trial_Default STATIC ${WebrtcSystem_WrappersField_Trial_Default_SOURCES})
target_compile_options(WebrtcSystem_WrappersField_Trial_Default PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcSystem_WrappersField_Trial_Default PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcSystem_WrappersField_Trial_Default PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcSystem_WrappersField_Trial_Default PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcSystem_WrappersField_Trial_Default PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/system_wrappers)
set_target_properties(WebrtcSystem_WrappersField_Trial_Default PROPERTIES OUTPUT_NAME field_trial_default)
# End of target WebrtcSystem_WrappersField_Trial_Default

# Start of target Third_PartyProtobufProtoc_Lib
set(Third_PartyProtobufProtoc_Lib_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/code_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/command_line_interface.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_enum.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_enum_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_extension.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_file.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_helpers.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_map_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_message.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_message_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_primitive_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_service.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_string_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_doc_comment.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_enum.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_enum_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_field_base.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_helpers.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_map_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_message.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_message_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_primitive_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_reflection_class.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_repeated_enum_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_repeated_message_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_repeated_primitive_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_source_generator_base.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_wrapper_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_context.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_doc_comment.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_enum.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_enum_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_enum_field_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_enum_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_extension.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_extension_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_file.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_generator_factory.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_helpers.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_lazy_message_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_lazy_message_field_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_map_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_map_field_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_message.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_message_builder.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_message_builder_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_message_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_message_field_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_message_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_name_resolver.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_primitive_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_primitive_field_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_service.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_shared_code_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_string_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/java/java_string_field_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_enum.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_enum_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_extension.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_file.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_helpers.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_map_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_message.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_message_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_primitive_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/js/js_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_enum.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_enum_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_extension.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_file.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_helpers.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_map_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_message.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_message_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_oneof.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_primitive_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/plugin.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/plugin.pb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/python/python_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/ruby/ruby_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/subprocess.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/zip_writer.cc)
add_library(Third_PartyProtobufProtoc_Lib STATIC ${Third_PartyProtobufProtoc_Lib_SOURCES})
target_compile_options(Third_PartyProtobufProtoc_Lib PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Werror" "-Wall" "-Wno-unused-variable" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-Wno-unused-function" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyProtobufProtoc_Lib PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(Third_PartyProtobufProtoc_Lib PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(Third_PartyProtobufProtoc_Lib PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(Third_PartyProtobufProtoc_Lib PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/third_party/protobuf)
set_target_properties(Third_PartyProtobufProtoc_Lib PROPERTIES OUTPUT_NAME protoc_lib)
target_link_libraries(Third_PartyProtobufProtoc_Lib Third_PartyProtobufProtobuf_Full)
# End of target Third_PartyProtobufProtoc_Lib


# Start of target WebrtcToolsEvent_Log_Visualizer_Utils
set(WebrtcToolsEvent_Log_Visualizer_Utils_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/event_log_visualizer/analyzer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/event_log_visualizer/plot_base.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/event_log_visualizer/plot_protobuf.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/event_log_visualizer/plot_python.cc)
add_library(WebrtcToolsEvent_Log_Visualizer_Utils STATIC ${WebrtcToolsEvent_Log_Visualizer_Utils_SOURCES})
target_compile_options(WebrtcToolsEvent_Log_Visualizer_Utils PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcToolsEvent_Log_Visualizer_Utils PRIVATE ENABLE_RTC_EVENT_LOG V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(WebrtcToolsEvent_Log_Visualizer_Utils PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(WebrtcToolsEvent_Log_Visualizer_Utils PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcToolsEvent_Log_Visualizer_Utils PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/tools)
set_target_properties(WebrtcToolsEvent_Log_Visualizer_Utils PROPERTIES OUTPUT_NAME event_log_visualizer_utils)
target_link_libraries(WebrtcToolsEvent_Log_Visualizer_Utils WebrtcLoggingRtc_Event_Log_Impl)
target_link_libraries(WebrtcToolsEvent_Log_Visualizer_Utils WebrtcLoggingRtc_Event_Log_Parser)
target_link_libraries(WebrtcToolsEvent_Log_Visualizer_Utils WebrtcModulesCongestion_ControllerCongestion_Controller)
target_link_libraries(WebrtcToolsEvent_Log_Visualizer_Utils WebrtcModulesRtp_RtcpRtp_Rtcp)
target_link_libraries(WebrtcToolsEvent_Log_Visualizer_Utils WebrtcSystem_WrappersField_Trial_Default)
target_link_libraries(WebrtcToolsEvent_Log_Visualizer_Utils WebrtcSystem_WrappersMetrics_Default)
target_link_libraries(WebrtcToolsEvent_Log_Visualizer_Utils WebrtcSystem_WrappersSystem_Wrappers)
target_link_libraries(WebrtcToolsEvent_Log_Visualizer_Utils WebrtcToolsChart_Proto)
target_include_directories(WebrtcToolsEvent_Log_Visualizer_Utils PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcToolsEvent_Log_Visualizer_Utils ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcToolsEvent_Log_Visualizer_Utils PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcToolsEvent_Log_Visualizer_Utils ${COREVIDEO_LIBRARY})
# End of target WebrtcToolsEvent_Log_Visualizer_Utils




# Start of target Third_PartyProtobufCopy_Google
list(APPEND Third_PartyProtobufCopy_Google ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/__init__.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/__init__.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/__init__.py)
add_custom_target(Third_PartyProtobufCopy_Google
    COMMAND echo "Generating copy targetThird_PartyProtobufCopy_Google"
    VERBATIM)
# End of target Third_PartyProtobufCopy_Google


# Start of target WebrtcToolsFrame_Editing_Lib
set(WebrtcToolsFrame_Editing_Lib_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/frame_editing/frame_editing_lib.cc)
add_library(WebrtcToolsFrame_Editing_Lib STATIC ${WebrtcToolsFrame_Editing_Lib_SOURCES})
target_compile_options(WebrtcToolsFrame_Editing_Lib PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcToolsFrame_Editing_Lib PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcToolsFrame_Editing_Lib PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcToolsFrame_Editing_Lib PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcToolsFrame_Editing_Lib PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/tools)
set_target_properties(WebrtcToolsFrame_Editing_Lib PROPERTIES OUTPUT_NAME frame_editing_lib)
target_link_libraries(WebrtcToolsFrame_Editing_Lib WebrtcCommon_VideoCommon_Video)
target_include_directories(WebrtcToolsFrame_Editing_Lib PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcToolsFrame_Editing_Lib ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcToolsFrame_Editing_Lib PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcToolsFrame_Editing_Lib ${COREVIDEO_LIBRARY})
# End of target WebrtcToolsFrame_Editing_Lib

# Start of target Third_PartyExpatExpat_Xml_Parse_Fuzzer
add_custom_target(Third_PartyExpatExpat_Xml_Parse_Fuzzer)
# End of target Third_PartyExpatExpat_Xml_Parse_Fuzzer

# Start of target WebrtcModulesAudio_MixerAudio_Frame_Manipulator
set(WebrtcModulesAudio_MixerAudio_Frame_Manipulator_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_mixer/audio_frame_manipulator.cc)
add_library(WebrtcModulesAudio_MixerAudio_Frame_Manipulator STATIC ${WebrtcModulesAudio_MixerAudio_Frame_Manipulator_SOURCES})
target_compile_options(WebrtcModulesAudio_MixerAudio_Frame_Manipulator PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_MixerAudio_Frame_Manipulator PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_MixerAudio_Frame_Manipulator PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_MixerAudio_Frame_Manipulator PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_MixerAudio_Frame_Manipulator PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_mixer)
set_target_properties(WebrtcModulesAudio_MixerAudio_Frame_Manipulator PROPERTIES OUTPUT_NAME audio_frame_manipulator)
target_link_libraries(WebrtcModulesAudio_MixerAudio_Frame_Manipulator WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcModulesAudio_MixerAudio_Frame_Manipulator WebrtcModulesUtilityUtility)
target_include_directories(WebrtcModulesAudio_MixerAudio_Frame_Manipulator PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_MixerAudio_Frame_Manipulator ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_MixerAudio_Frame_Manipulator

# Start of target Third_PartyExpatExpat
set(Third_PartyExpatExpat_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/expat/files/lib/xmlparse.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/expat/files/lib/xmlrole.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/expat/files/lib/xmltok.c)
add_library(Third_PartyExpatExpat STATIC ${Third_PartyExpatExpat_SOURCES})
target_compile_options(Third_PartyExpatExpat PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyExpatExpat PRIVATE _LIB HAVE_EXPAT_CONFIG_H V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 XML_STATIC)
target_include_directories(Third_PartyExpatExpat PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/expat/files/lib/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(Third_PartyExpatExpat PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(Third_PartyExpatExpat PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/third_party/expat)
set_target_properties(Third_PartyExpatExpat PROPERTIES OUTPUT_NAME expat)
# End of target Third_PartyExpatExpat


# Start of target WebrtcLoggingRtc_Event_Log_Proto_Gen
add_custom_command(OUTPUT ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/logging/rtc_event_log/rtc_event_log.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/logging/rtc_event_log/rtc_event_log.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/logging/rtc_event_log/rtc_event_log_pb2.py
    DEPENDS Third_PartyProtobufProtoc
    COMMAND python ${LIBWEBRTC_INPUT_DIR}/tools/protoc_wrapper/protoc_wrapper.py rtc_event_log.proto --protoc ./protoc --proto-in-dir ${LIBWEBRTC_INPUT_DIR}/webrtc/logging/rtc_event_log --cc-out-dir ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/logging/rtc_event_log --py-out-dir pyproto/webrtc/logging/rtc_event_log
    VERBATIM)
add_custom_target(WebrtcLoggingRtc_Event_Log_Proto_Gen DEPENDS Third_PartyProtobufProtoc ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/logging/rtc_event_log/rtc_event_log.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/logging/rtc_event_log/rtc_event_log.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/logging/rtc_event_log/rtc_event_log_pb2.py)
# End of target WebrtcLoggingRtc_Event_Log_Proto_Gen


# Start of target WebrtcApiLibjingle_Peerconnection
set(WebrtcApiLibjingle_Peerconnection_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/api/audiotrack.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/datachannel.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/dtmfsender.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/jsepicecandidate.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/jsepsessiondescription.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/localaudiosource.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/mediaconstraintsinterface.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/mediacontroller.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/mediastream.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/mediastreamobserver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/peerconnection.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/peerconnectionfactory.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/remoteaudiosource.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/rtcstatscollector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/rtpreceiver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/rtpsender.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/sctputils.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/statscollector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/statstypes.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/videocapturertracksource.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/videotrack.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/videotracksource.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/webrtcsdp.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/webrtcsession.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/api/webrtcsessiondescriptionfactory.cc
    $<TARGET_OBJECTS:WebrtcApiRtc_Stats_Api>)
add_library(WebrtcApiLibjingle_Peerconnection STATIC ${WebrtcApiLibjingle_Peerconnection_SOURCES})
target_compile_options(WebrtcApiLibjingle_Peerconnection PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcApiLibjingle_Peerconnection PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC HAVE_WEBRTC_VIDEO HAVE_WEBRTC_VOICE HAVE_SCTP HAVE_SRTP WEBRTC_WEBKIT_BUILD)
target_include_directories(WebrtcApiLibjingle_Peerconnection PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcApiLibjingle_Peerconnection PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcApiLibjingle_Peerconnection PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/api)
set_target_properties(WebrtcApiLibjingle_Peerconnection PROPERTIES OUTPUT_NAME jingle_peerconnection)
target_link_libraries(WebrtcApiLibjingle_Peerconnection WebrtcApiCall_Api)
target_link_libraries(WebrtcApiLibjingle_Peerconnection WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcApiLibjingle_Peerconnection WebrtcCallCall)
target_link_libraries(WebrtcApiLibjingle_Peerconnection WebrtcMediaRtc_Media)
target_link_libraries(WebrtcApiLibjingle_Peerconnection WebrtcPcRtc_Pc)
target_link_libraries(WebrtcApiLibjingle_Peerconnection WebrtcStatsRtc_Stats)
target_include_directories(WebrtcApiLibjingle_Peerconnection PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcApiLibjingle_Peerconnection ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcApiLibjingle_Peerconnection PRIVATE ${AVFOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcApiLibjingle_Peerconnection ${AVFOUNDATION_LIBRARY})
target_include_directories(WebrtcApiLibjingle_Peerconnection PRIVATE ${COREMEDIA_INCLUDE_DIR})
target_link_libraries(WebrtcApiLibjingle_Peerconnection ${COREMEDIA_LIBRARY})
target_include_directories(WebrtcApiLibjingle_Peerconnection PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcApiLibjingle_Peerconnection ${COREVIDEO_LIBRARY})
target_include_directories(WebrtcApiLibjingle_Peerconnection PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcApiLibjingle_Peerconnection ${COCOA_LIBRARY})
target_include_directories(WebrtcApiLibjingle_Peerconnection PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcApiLibjingle_Peerconnection ${IOKIT_LIBRARY})
target_include_directories(WebrtcApiLibjingle_Peerconnection PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcApiLibjingle_Peerconnection ${SECURITY_LIBRARY})
target_include_directories(WebrtcApiLibjingle_Peerconnection PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcApiLibjingle_Peerconnection ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcApiLibjingle_Peerconnection PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcApiLibjingle_Peerconnection ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcApiLibjingle_Peerconnection PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcApiLibjingle_Peerconnection ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcApiLibjingle_Peerconnection PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcApiLibjingle_Peerconnection ${COREAUDIO_LIBRARY})
target_include_directories(WebrtcApiLibjingle_Peerconnection PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcApiLibjingle_Peerconnection ${COREGRAPHICS_LIBRARY})
# End of target WebrtcApiLibjingle_Peerconnection

# Start of target WebrtcP2PLibstunprober
set(WebrtcP2PLibstunprober_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/p2p/stunprober/stunprober.cc)
add_library(WebrtcP2PLibstunprober STATIC ${WebrtcP2PLibstunprober_SOURCES})
target_compile_options(WebrtcP2PLibstunprober PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcP2PLibstunprober PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC SSL_USE_OPENSSL HAVE_OPENSSL_SSL_H FEATURE_ENABLE_SSL)
target_include_directories(WebrtcP2PLibstunprober PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcP2PLibstunprober PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcP2PLibstunprober PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/p2p)
set_target_properties(WebrtcP2PLibstunprober PROPERTIES OUTPUT_NAME stunprober)
target_link_libraries(WebrtcP2PLibstunprober WebrtcWebrtc_Common)
target_link_libraries(WebrtcP2PLibstunprober WebrtcBaseRtc_Base)
target_include_directories(WebrtcP2PLibstunprober PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcP2PLibstunprober ${COCOA_LIBRARY})
target_include_directories(WebrtcP2PLibstunprober PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcP2PLibstunprober ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcP2PLibstunprober PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcP2PLibstunprober ${IOKIT_LIBRARY})
target_include_directories(WebrtcP2PLibstunprober PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcP2PLibstunprober ${SECURITY_LIBRARY})
target_include_directories(WebrtcP2PLibstunprober PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcP2PLibstunprober ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcP2PLibstunprober PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcP2PLibstunprober ${APPLICATIONSERVICES_LIBRARY})
# End of target WebrtcP2PLibstunprober


# Start of target WebrtcLibjingleXmppXmpp
add_custom_target(WebrtcLibjingleXmppXmpp)
add_dependencies(WebrtcLibjingleXmppXmpp WebrtcLibjingleXmppRtc_Xmpp)
# End of target WebrtcLibjingleXmppXmpp





# Start of target WebrtcModulesAudio_CodingIsac_Common
set(WebrtcModulesAudio_CodingIsac_Common_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/locked_bandwidth_info.cc)
add_library(WebrtcModulesAudio_CodingIsac_Common STATIC ${WebrtcModulesAudio_CodingIsac_Common_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingIsac_Common PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingIsac_Common PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingIsac_Common PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingIsac_Common PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingIsac_Common PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingIsac_Common PROPERTIES OUTPUT_NAME isac_common)
# End of target WebrtcModulesAudio_CodingIsac_Common

# Start of target WebrtcModulesAudio_CodingNeteq_Unittest_Proto_Gen
add_custom_command(OUTPUT ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/audio_coding/neteq/neteq_unittest.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/audio_coding/neteq/neteq_unittest.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/audio_coding/neteq/neteq_unittest_pb2.py
    DEPENDS Third_PartyProtobufProtoc
    COMMAND python ${LIBWEBRTC_INPUT_DIR}/tools/protoc_wrapper/protoc_wrapper.py neteq_unittest.proto --protoc ./protoc --proto-in-dir ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq --cc-out-dir ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/audio_coding/neteq --py-out-dir pyproto/webrtc/audio_coding/neteq
    VERBATIM)
add_custom_target(WebrtcModulesAudio_CodingNeteq_Unittest_Proto_Gen DEPENDS Third_PartyProtobufProtoc ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/audio_coding/neteq/neteq_unittest.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/audio_coding/neteq/neteq_unittest.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/audio_coding/neteq/neteq_unittest_pb2.py)
# End of target WebrtcModulesAudio_CodingNeteq_Unittest_Proto_Gen


# Start of target Third_PartyProtobufPy_Proto
add_custom_target(Third_PartyProtobufPy_Proto)
add_dependencies(Third_PartyProtobufPy_Proto Third_PartyProtobufCopy_Google Third_PartyProtobufCopy_Google_Protobuf Third_PartyProtobufCopy_Google_Protobuf_Internal Third_PartyProtobufCopy_Six)
# End of target Third_PartyProtobufPy_Proto


# Start of target WebrtcModulesAudio_CodingAna_Debug_Dump_Proto_Gen
add_custom_command(OUTPUT ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor/debug_dump.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor/debug_dump.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_coding/audio_network_adaptor/debug_dump_pb2.py
    DEPENDS Third_PartyProtobufProtoc
    COMMAND python ${LIBWEBRTC_INPUT_DIR}/tools/protoc_wrapper/protoc_wrapper.py debug_dump.proto --protoc ./protoc --proto-in-dir ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/audio_network_adaptor --cc-out-dir ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor --py-out-dir pyproto/webrtc/modules/audio_coding/audio_network_adaptor
    VERBATIM)
add_custom_target(WebrtcModulesAudio_CodingAna_Debug_Dump_Proto_Gen DEPENDS Third_PartyProtobufProtoc ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor/debug_dump.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor/debug_dump.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_coding/audio_network_adaptor/debug_dump_pb2.py)
# End of target WebrtcModulesAudio_CodingAna_Debug_Dump_Proto_Gen

# Start of target WebrtcModulesPacingPacing
set(WebrtcModulesPacingPacing_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/pacing/alr_detector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/pacing/bitrate_prober.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/pacing/paced_sender.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/pacing/packet_router.cc)
add_library(WebrtcModulesPacingPacing STATIC ${WebrtcModulesPacingPacing_SOURCES})
target_compile_options(WebrtcModulesPacingPacing PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesPacingPacing PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesPacingPacing PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesPacingPacing PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesPacingPacing PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/pacing)
set_target_properties(WebrtcModulesPacingPacing PROPERTIES OUTPUT_NAME pacing)
target_link_libraries(WebrtcModulesPacingPacing WebrtcModulesBitrate_ControllerBitrate_Controller)
target_link_libraries(WebrtcModulesPacingPacing WebrtcModulesRtp_RtcpRtp_Rtcp)
target_link_libraries(WebrtcModulesPacingPacing WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesPacingPacing PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesPacingPacing ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcModulesPacingPacing PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcModulesPacingPacing ${COREVIDEO_LIBRARY})
# End of target WebrtcModulesPacingPacing

# Start of target WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library
add_library(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library SHARED)
target_compile_options(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-arc" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Headers/ ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PROPERTIES LINK_FLAGS "-all_load -stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9 -Wl,-ObjC")
set_target_properties(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/sdk/rtc_sdk_framework_objc_shared_library)
set_target_properties(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PROPERTIES OUTPUT_NAME WebRTC)
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library WebrtcSdkRtc_Sdk_Peerconnection_Objc)
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library WebrtcSystem_WrappersField_Trial_Default)
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library WebrtcSystem_WrappersMetrics_Default)
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${AVFOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${AVFOUNDATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${COREGRAPHICS_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${COREMEDIA_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${COREMEDIA_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${GLKIT_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${GLKIT_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${VIDEOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${VIDEOTOOLBOX_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${COCOA_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${IOKIT_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${SECURITY_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${COREVIDEO_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${OPENGL_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${OPENGL_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library ${COREAUDIO_LIBRARY})
add_dependencies(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library WebrtcSdkRtc_Sdk_Peerconnection_Objc WebrtcSystem_WrappersField_Trial_Default WebrtcSystem_WrappersMetrics_Default)
# End of target WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library


# Start of target WebrtcSdkRtc_Sdk_Framework_Objc
add_library(WebrtcSdkRtc_Sdk_Framework_Objc SHARED)
target_compile_options(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE "")
set_target_properties(WebrtcSdkRtc_Sdk_Framework_Objc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/WebRTC.framework)
set_target_properties(WebrtcSdkRtc_Sdk_Framework_Objc PROPERTIES OUTPUT_NAME WebRTC)
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc WebrtcSdkRtc_Sdk_Peerconnection_Objc)
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc WebrtcSystem_WrappersField_Trial_Default)
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc WebrtcSystem_WrappersMetrics_Default)
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc ${COCOA_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc ${IOKIT_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc ${SECURITY_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE ${AVFOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc ${AVFOUNDATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE ${COREMEDIA_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc ${COREMEDIA_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc ${COREVIDEO_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE ${OPENGL_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc ${OPENGL_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc ${COREAUDIO_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Framework_Objc PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Framework_Objc ${COREGRAPHICS_LIBRARY})
add_dependencies(WebrtcSdkRtc_Sdk_Framework_Objc WebrtcSdkRtc_Sdk_Framework_Objc_Info_Plist_Bundle_Data WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library_Bundle_Data WebrtcSdkRtc_Sdk_Peerconnection_Objc WebrtcSystem_WrappersField_Trial_Default WebrtcSystem_WrappersMetrics_Default)
set_target_properties(WebrtcSdkRtc_Sdk_Framework_Objc PROPERTIES
            FRAMEWORK TRUE
            FRAMEWORK_VERSION C
            MACOSX_FRAMEWORK_INFO_PLIST ${LIBWEBRTC_INPUT_DIR}/../WebKit/rtc_sdk_framework_objc_info_plist.plist)
# End of target WebrtcSdkRtc_Sdk_Framework_Objc

# Start of target WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator
set(WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/remote_bitrate_estimator/aimd_rate_control.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/remote_bitrate_estimator/bwe_defines.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/remote_bitrate_estimator/inter_arrival.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/remote_bitrate_estimator/overuse_detector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/remote_bitrate_estimator/overuse_estimator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/remote_bitrate_estimator/remote_estimator_proxy.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/remote_bitrate_estimator/send_time_history.cc)
add_library(WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator STATIC ${WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator_SOURCES})
target_compile_options(WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator PRIVATE BWE_TEST_LOGGING_COMPILE_TIME_ENABLE=0 V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/remote_bitrate_estimator)
set_target_properties(WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator PROPERTIES OUTPUT_NAME remote_bitrate_estimator)
target_link_libraries(WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator


# Start of target WebrtcToolsPsnr_Ssim_Analyzer
set(WebrtcToolsPsnr_Ssim_Analyzer_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/psnr_ssim_analyzer/psnr_ssim_analyzer.cc)
add_executable(WebrtcToolsPsnr_Ssim_Analyzer ${WebrtcToolsPsnr_Ssim_Analyzer_SOURCES})
target_compile_options(WebrtcToolsPsnr_Ssim_Analyzer PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcToolsPsnr_Ssim_Analyzer PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcToolsPsnr_Ssim_Analyzer PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcToolsPsnr_Ssim_Analyzer PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9 -Wl,-ObjC -Wl,-pie")
set_target_properties(WebrtcToolsPsnr_Ssim_Analyzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/)
set_target_properties(WebrtcToolsPsnr_Ssim_Analyzer PROPERTIES OUTPUT_NAME psnr_ssim_analyzer)
target_link_libraries(WebrtcToolsPsnr_Ssim_Analyzer WebrtcToolsCommand_Line_Parser)
target_link_libraries(WebrtcToolsPsnr_Ssim_Analyzer WebrtcToolsVideo_Quality_Analysis)
target_include_directories(WebrtcToolsPsnr_Ssim_Analyzer PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcToolsPsnr_Ssim_Analyzer ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcToolsPsnr_Ssim_Analyzer PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcToolsPsnr_Ssim_Analyzer ${COREVIDEO_LIBRARY})
add_dependencies(WebrtcToolsPsnr_Ssim_Analyzer BuildConfigSanitizersDeps BuildWinDefault_Exe_Manifest WebrtcToolsCommand_Line_Parser WebrtcToolsVideo_Quality_Analysis)
# End of target WebrtcToolsPsnr_Ssim_Analyzer

# Start of target WebrtcVoice_EngineLevel_Indicator
set(WebrtcVoice_EngineLevel_Indicator_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/level_indicator.cc)
add_library(WebrtcVoice_EngineLevel_Indicator STATIC ${WebrtcVoice_EngineLevel_Indicator_SOURCES})
target_compile_options(WebrtcVoice_EngineLevel_Indicator PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcVoice_EngineLevel_Indicator PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcVoice_EngineLevel_Indicator PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcVoice_EngineLevel_Indicator PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcVoice_EngineLevel_Indicator PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/voice_engine)
set_target_properties(WebrtcVoice_EngineLevel_Indicator PROPERTIES OUTPUT_NAME level_indicator)
target_link_libraries(WebrtcVoice_EngineLevel_Indicator WebrtcWebrtc_Common)
target_link_libraries(WebrtcVoice_EngineLevel_Indicator WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcVoice_EngineLevel_Indicator WebrtcCommon_AudioCommon_Audio)
target_include_directories(WebrtcVoice_EngineLevel_Indicator PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcVoice_EngineLevel_Indicator ${FOUNDATION_LIBRARY})
# End of target WebrtcVoice_EngineLevel_Indicator




# Start of target WebrtcP2PP2P
add_custom_target(WebrtcP2PP2P)
add_dependencies(WebrtcP2PP2P WebrtcP2PLibstunprober WebrtcP2PRtc_P2P)
# End of target WebrtcP2PP2P


# Start of target WebrtcToolsVideo_Quality_Analysis
set(WebrtcToolsVideo_Quality_Analysis_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/frame_analyzer/video_quality_analysis.cc)
add_library(WebrtcToolsVideo_Quality_Analysis STATIC ${WebrtcToolsVideo_Quality_Analysis_SOURCES})
target_compile_options(WebrtcToolsVideo_Quality_Analysis PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcToolsVideo_Quality_Analysis PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcToolsVideo_Quality_Analysis PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcToolsVideo_Quality_Analysis PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcToolsVideo_Quality_Analysis PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/tools)
set_target_properties(WebrtcToolsVideo_Quality_Analysis PROPERTIES OUTPUT_NAME video_quality_analysis)
target_link_libraries(WebrtcToolsVideo_Quality_Analysis WebrtcCommon_VideoCommon_Video)
target_include_directories(WebrtcToolsVideo_Quality_Analysis PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcToolsVideo_Quality_Analysis ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcToolsVideo_Quality_Analysis PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcToolsVideo_Quality_Analysis ${COREVIDEO_LIBRARY})
# End of target WebrtcToolsVideo_Quality_Analysis

# Start of target BuildConfigSanitizersDeps_No_Options
add_custom_target(BuildConfigSanitizersDeps_No_Options)
# End of target BuildConfigSanitizersDeps_No_Options

# Start of target WebrtcSdkSdk
add_custom_target(WebrtcSdkSdk)
add_dependencies(WebrtcSdkSdk WebrtcSdkRtc_Sdk_Framework_Objc)
# End of target WebrtcSdkSdk

# Start of target WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto_Gen
add_custom_command(OUTPUT ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing/debug.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing/debug.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_processing/debug_pb2.py
    DEPENDS Third_PartyProtobufProtoc
    COMMAND python ${LIBWEBRTC_INPUT_DIR}/tools/protoc_wrapper/protoc_wrapper.py debug.proto --protoc ./protoc --proto-in-dir ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing --cc-out-dir ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing --py-out-dir pyproto/webrtc/modules/audio_processing
    VERBATIM)
add_custom_target(WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto_Gen DEPENDS Third_PartyProtobufProtoc ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing/debug.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing/debug.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_processing/debug_pb2.py)
# End of target WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto_Gen


# Start of target Third_PartyLibsrtpLibsrtp
set(Third_PartyLibsrtpLibsrtp_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/cipher/aes_gcm_ossl.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/cipher/aes_icm_ossl.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/cipher/cipher.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/cipher/null_cipher.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/hash/auth.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/hash/hmac_ossl.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/hash/null_auth.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/kernel/alloc.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/kernel/crypto_kernel.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/kernel/err.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/kernel/key.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/math/datatypes.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/math/stat.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/replay/rdb.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/replay/rdbx.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/srtp/ekt.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/srtp/srtp.c)
add_library(Third_PartyLibsrtpLibsrtp STATIC ${Third_PartyLibsrtpLibsrtp_SOURCES})
target_compile_options(Third_PartyLibsrtpLibsrtp PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Werror" "-Wall" "-Wno-unused-variable" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyLibsrtpLibsrtp PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 PACKAGE_STRING="libsrtp2 2.0.0-pre" PACKAGE_VERSION="2.0.0-pre" HAVE_CONFIG_H OPENSSL HAVE_STDLIB_H HAVE_STRING_H HAVE_STDINT_H HAVE_INTTYPES_H HAVE_INT16_T HAVE_INT32_T HAVE_INT8_T HAVE_UINT16_T HAVE_UINT32_T HAVE_UINT64_T HAVE_UINT8_T HAVE_ARPA_INET_H HAVE_NETINET_IN_H HAVE_SYS_TYPES_H HAVE_UNISTD_H)
target_include_directories(Third_PartyLibsrtpLibsrtp PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/config/ ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/ ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(Third_PartyLibsrtpLibsrtp PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(Third_PartyLibsrtpLibsrtp PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/third_party/libsrtp)
set_target_properties(Third_PartyLibsrtpLibsrtp PROPERTIES OUTPUT_NAME srtp)
target_link_libraries(Third_PartyLibsrtpLibsrtp Third_PartyBoringsslBoringssl)
# End of target Third_PartyLibsrtpLibsrtp

# Start of target WebrtcToolsCommand_Line_Parser
set(WebrtcToolsCommand_Line_Parser_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/simple_command_line_parser.cc
    $<TARGET_OBJECTS:WebrtcBaseGtest_Prod>)
add_library(WebrtcToolsCommand_Line_Parser STATIC ${WebrtcToolsCommand_Line_Parser_SOURCES})
target_compile_options(WebrtcToolsCommand_Line_Parser PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcToolsCommand_Line_Parser PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcToolsCommand_Line_Parser PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcToolsCommand_Line_Parser PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcToolsCommand_Line_Parser PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/tools)
set_target_properties(WebrtcToolsCommand_Line_Parser PROPERTIES OUTPUT_NAME command_line_parser)
# End of target WebrtcToolsCommand_Line_Parser

# Start of target WebrtcBaseRtc_Task_Queue
set(WebrtcBaseRtc_Task_Queue_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/base/sequenced_task_checker_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/weak_ptr.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/task_queue_gcd.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/task_queue_posix.cc)
add_library(WebrtcBaseRtc_Task_Queue STATIC ${WebrtcBaseRtc_Task_Queue_SOURCES})
target_compile_options(WebrtcBaseRtc_Task_Queue PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcBaseRtc_Task_Queue PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcBaseRtc_Task_Queue PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcBaseRtc_Task_Queue PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcBaseRtc_Task_Queue PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/base)
set_target_properties(WebrtcBaseRtc_Task_Queue PROPERTIES OUTPUT_NAME rtc_task_queue)
target_link_libraries(WebrtcBaseRtc_Task_Queue WebrtcBaseRtc_Base_Approved)
target_include_directories(WebrtcBaseRtc_Task_Queue PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcBaseRtc_Task_Queue ${FOUNDATION_LIBRARY})
# End of target WebrtcBaseRtc_Task_Queue

# Start of target WebrtcBaseBase
add_custom_target(WebrtcBaseBase)
add_dependencies(WebrtcBaseBase WebrtcBaseRtc_Base WebrtcBaseRtc_Base_Approved WebrtcBaseRtc_Task_Queue)
# End of target WebrtcBaseBase


# Start of target WebrtcVoice_EngineVoice_Engine
set(WebrtcVoice_EngineVoice_Engine_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/channel.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/channel_manager.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/channel_proxy.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/monitor_module.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/output_mixer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/shared_data.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/statistics.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/transmit_mixer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/utility.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/voe_audio_processing_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/voe_base_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/voe_codec_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/voe_external_media_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/voe_file_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/voe_hardware_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/voe_neteq_stats_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/voe_network_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/voe_rtp_rtcp_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/voe_video_sync_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/voe_volume_control_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/voice_engine/voice_engine_impl.cc
    $<TARGET_OBJECTS:WebrtcApiAudio_Mixer_Api>
    $<TARGET_OBJECTS:WebrtcLoggingRtc_Event_Log_Api>
    $<TARGET_OBJECTS:WebrtcModulesAudio_CodingAudio_Decoder_Factory_Interface>)
add_library(WebrtcVoice_EngineVoice_Engine STATIC ${WebrtcVoice_EngineVoice_Engine_SOURCES})
target_compile_options(WebrtcVoice_EngineVoice_Engine PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcVoice_EngineVoice_Engine PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcVoice_EngineVoice_Engine PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/dummy/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_conference_mixer/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcVoice_EngineVoice_Engine PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcVoice_EngineVoice_Engine PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/voice_engine)
set_target_properties(WebrtcVoice_EngineVoice_Engine PROPERTIES OUTPUT_NAME voice_engine)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcWebrtc_Common)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcApiCall_Api)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesAudio_CodingAudio_Coding)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesAudio_CodingAudio_Decoder_Interface)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesAudio_CodingAudio_Format)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesAudio_CodingAudio_Format_Conversion)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesAudio_CodingAudio_Format_Conversion)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesAudio_CodingRent_A_Codec)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesAudio_DeviceAudio_Device)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesAudio_ProcessingAudio_Processing)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesBitrate_ControllerBitrate_Controller)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesMedia_FileMedia_File)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesPacingPacing)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesRtp_RtcpRtp_Rtcp)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcModulesUtilityUtility)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcSystem_WrappersSystem_Wrappers)
target_link_libraries(WebrtcVoice_EngineVoice_Engine WebrtcVoice_EngineLevel_Indicator)
target_include_directories(WebrtcVoice_EngineVoice_Engine PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcVoice_EngineVoice_Engine ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcVoice_EngineVoice_Engine PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcVoice_EngineVoice_Engine ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcVoice_EngineVoice_Engine PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcVoice_EngineVoice_Engine ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcVoice_EngineVoice_Engine PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcVoice_EngineVoice_Engine ${COREAUDIO_LIBRARY})
target_include_directories(WebrtcVoice_EngineVoice_Engine PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcVoice_EngineVoice_Engine ${COREGRAPHICS_LIBRARY})
target_include_directories(WebrtcVoice_EngineVoice_Engine PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcVoice_EngineVoice_Engine ${COREVIDEO_LIBRARY})
# End of target WebrtcVoice_EngineVoice_Engine


# Start of target Third_PartyProtobufCopy_Google_Protobuf_Internal
list(APPEND Third_PartyProtobufCopy_Google_Protobuf_Internal ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/__init__.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/_parameterized.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/api_implementation.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/containers.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/decoder.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/encoder.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/enum_type_wrapper.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/message_listener.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/python_message.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/type_checkers.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/well_known_types.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/wire_format.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/internal/__init__.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/__init__.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/internal/_parameterized.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/_parameterized.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/internal/api_implementation.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/api_implementation.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/internal/containers.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/containers.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/internal/decoder.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/decoder.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/internal/encoder.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/encoder.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/internal/enum_type_wrapper.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/enum_type_wrapper.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/internal/message_listener.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/message_listener.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/internal/python_message.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/python_message.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/internal/type_checkers.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/type_checkers.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/internal/well_known_types.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/well_known_types.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/internal/wire_format.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/internal/wire_format.py)
add_custom_target(Third_PartyProtobufCopy_Google_Protobuf_Internal
    COMMAND echo "Generating copy targetThird_PartyProtobufCopy_Google_Protobuf_Internal"
    VERBATIM)
# End of target Third_PartyProtobufCopy_Google_Protobuf_Internal

# Start of target WebrtcModulesAudio_CodingIsac_Fix
set(WebrtcModulesAudio_CodingIsac_Fix_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/arith_routines.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/arith_routines_hist.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/arith_routines_logist.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/audio_decoder_isacfix.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/audio_encoder_isacfix.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/bandwidth_estimator.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/decode.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/decode_bwe.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/decode_plc.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/encode.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/entropy_coding.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/fft.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/filterbank_tables.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/filterbanks.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/filters.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/initialize.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/isacfix.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/lattice.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/lattice_c.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/lpc_masking_model.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/lpc_tables.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/pitch_estimator.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/pitch_estimator_c.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/pitch_filter.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/pitch_filter_c.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/pitch_gain_tables.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/pitch_lag_tables.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/spectrum_ar_model_tables.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/transform.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/source/transform_tables.c)
add_library(WebrtcModulesAudio_CodingIsac_Fix STATIC ${WebrtcModulesAudio_CodingIsac_Fix_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingIsac_Fix PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingIsac_Fix PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingIsac_Fix PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/fix/include/)
set_target_properties(WebrtcModulesAudio_CodingIsac_Fix PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingIsac_Fix PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingIsac_Fix PROPERTIES OUTPUT_NAME isac_fix)
target_link_libraries(WebrtcModulesAudio_CodingIsac_Fix WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcModulesAudio_CodingIsac_Fix WebrtcModulesAudio_CodingAudio_Decoder_Interface)
target_link_libraries(WebrtcModulesAudio_CodingIsac_Fix WebrtcModulesAudio_CodingAudio_Encoder_Interface)
target_link_libraries(WebrtcModulesAudio_CodingIsac_Fix WebrtcModulesAudio_CodingIsac_Common)
target_link_libraries(WebrtcModulesAudio_CodingIsac_Fix WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesAudio_CodingIsac_Fix PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingIsac_Fix ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingIsac_Fix




# Start of target WebrtcModulesAudio_CodingAna_Config_Proto
set(WebrtcModulesAudio_CodingAna_Config_Proto_SOURCES ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor/config.pb.cc
    ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_coding/audio_network_adaptor/config_pb2.py)
add_library(WebrtcModulesAudio_CodingAna_Config_Proto STATIC ${WebrtcModulesAudio_CodingAna_Config_Proto_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingAna_Config_Proto PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingAna_Config_Proto PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(WebrtcModulesAudio_CodingAna_Config_Proto PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(WebrtcModulesAudio_CodingAna_Config_Proto PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingAna_Config_Proto PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingAna_Config_Proto PROPERTIES OUTPUT_NAME ana_config_proto)
target_link_libraries(WebrtcModulesAudio_CodingAna_Config_Proto Third_PartyProtobufProtobuf_Lite)
# End of target WebrtcModulesAudio_CodingAna_Config_Proto


# Start of target WebrtcPcRtc_Pc
set(WebrtcPcRtc_Pc_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/pc/audiomonitor.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/pc/bundlefilter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/pc/channel.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/pc/channelmanager.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/pc/currentspeakermonitor.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/pc/mediamonitor.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/pc/mediasession.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/pc/rtcpmuxfilter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/pc/srtpfilter.cc)
add_library(WebrtcPcRtc_Pc STATIC ${WebrtcPcRtc_Pc_SOURCES})
target_compile_options(WebrtcPcRtc_Pc PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcPcRtc_Pc PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC HAVE_SCTP HAVE_SRTP SSL_USE_OPENSSL HAVE_OPENSSL_SSL_H FEATURE_ENABLE_SSL HAVE_WEBRTC_VIDEO HAVE_WEBRTC_VOICE PACKAGE_STRING="libsrtp2 2.0.0-pre" PACKAGE_VERSION="2.0.0-pre" HAVE_CONFIG_H OPENSSL HAVE_STDLIB_H HAVE_STRING_H HAVE_STDINT_H HAVE_INTTYPES_H HAVE_INT16_T HAVE_INT32_T HAVE_INT8_T HAVE_UINT16_T HAVE_UINT32_T HAVE_UINT64_T HAVE_UINT8_T HAVE_ARPA_INET_H HAVE_NETINET_IN_H HAVE_SYS_TYPES_H HAVE_UNISTD_H)
target_include_directories(WebrtcPcRtc_Pc PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/config/ ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/ ${LIBWEBRTC_INPUT_DIR}/third_party/libsrtp/crypto/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcPcRtc_Pc PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcPcRtc_Pc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/pc)
set_target_properties(WebrtcPcRtc_Pc PROPERTIES OUTPUT_NAME rtc_pc)
target_link_libraries(WebrtcPcRtc_Pc Third_PartyLibsrtpLibsrtp)
target_link_libraries(WebrtcPcRtc_Pc WebrtcApiCall_Api)
target_link_libraries(WebrtcPcRtc_Pc WebrtcBaseRtc_Base)
target_link_libraries(WebrtcPcRtc_Pc WebrtcMediaRtc_Media)
target_include_directories(WebrtcPcRtc_Pc PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcPcRtc_Pc ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcPcRtc_Pc PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcPcRtc_Pc ${COCOA_LIBRARY})
target_include_directories(WebrtcPcRtc_Pc PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcPcRtc_Pc ${IOKIT_LIBRARY})
target_include_directories(WebrtcPcRtc_Pc PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcPcRtc_Pc ${SECURITY_LIBRARY})
target_include_directories(WebrtcPcRtc_Pc PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcPcRtc_Pc ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcPcRtc_Pc PRIVATE ${AVFOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcPcRtc_Pc ${AVFOUNDATION_LIBRARY})
target_include_directories(WebrtcPcRtc_Pc PRIVATE ${COREMEDIA_INCLUDE_DIR})
target_link_libraries(WebrtcPcRtc_Pc ${COREMEDIA_LIBRARY})
target_include_directories(WebrtcPcRtc_Pc PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcPcRtc_Pc ${COREVIDEO_LIBRARY})
target_include_directories(WebrtcPcRtc_Pc PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcPcRtc_Pc ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcPcRtc_Pc PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcPcRtc_Pc ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcPcRtc_Pc PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcPcRtc_Pc ${COREAUDIO_LIBRARY})
target_include_directories(WebrtcPcRtc_Pc PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcPcRtc_Pc ${COREGRAPHICS_LIBRARY})
# End of target WebrtcPcRtc_Pc

# Start of target WebrtcModulesAudio_CodingNeteq_Unittest_Proto
set(WebrtcModulesAudio_CodingNeteq_Unittest_Proto_SOURCES ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/audio_coding/neteq/neteq_unittest.pb.cc
    ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/audio_coding/neteq/neteq_unittest_pb2.py)
add_library(WebrtcModulesAudio_CodingNeteq_Unittest_Proto STATIC ${WebrtcModulesAudio_CodingNeteq_Unittest_Proto_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingNeteq_Unittest_Proto PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingNeteq_Unittest_Proto PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(WebrtcModulesAudio_CodingNeteq_Unittest_Proto PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(WebrtcModulesAudio_CodingNeteq_Unittest_Proto PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingNeteq_Unittest_Proto PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingNeteq_Unittest_Proto PROPERTIES OUTPUT_NAME neteq_unittest_proto)
target_link_libraries(WebrtcModulesAudio_CodingNeteq_Unittest_Proto Third_PartyProtobufProtobuf_Lite)
# End of target WebrtcModulesAudio_CodingNeteq_Unittest_Proto



# Start of target WebrtcToolsFrame_Editor
set(WebrtcToolsFrame_Editor_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/frame_editing/frame_editing.cc)
add_executable(WebrtcToolsFrame_Editor ${WebrtcToolsFrame_Editor_SOURCES})
target_compile_options(WebrtcToolsFrame_Editor PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcToolsFrame_Editor PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcToolsFrame_Editor PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcToolsFrame_Editor PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9 -Wl,-ObjC -Wl,-pie")
set_target_properties(WebrtcToolsFrame_Editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/)
set_target_properties(WebrtcToolsFrame_Editor PROPERTIES OUTPUT_NAME frame_editor)
target_link_libraries(WebrtcToolsFrame_Editor WebrtcToolsCommand_Line_Parser)
target_link_libraries(WebrtcToolsFrame_Editor WebrtcToolsFrame_Editing_Lib)
target_include_directories(WebrtcToolsFrame_Editor PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcToolsFrame_Editor ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcToolsFrame_Editor PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcToolsFrame_Editor ${COREVIDEO_LIBRARY})
add_dependencies(WebrtcToolsFrame_Editor BuildConfigSanitizersDeps BuildWinDefault_Exe_Manifest WebrtcToolsCommand_Line_Parser WebrtcToolsFrame_Editing_Lib)
# End of target WebrtcToolsFrame_Editor









# Start of target Third_PartyLibyuvConvert
set(Third_PartyLibyuvConvert_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/util/convert.cc)
add_executable(Third_PartyLibyuvConvert ${Third_PartyLibyuvConvert_SOURCES})
target_compile_options(Third_PartyLibyuvConvert PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyLibyuvConvert PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1)
target_include_directories(Third_PartyLibyuvConvert PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(Third_PartyLibyuvConvert PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9 -Wl,-ObjC -Wl,-pie")
set_target_properties(Third_PartyLibyuvConvert PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/)
set_target_properties(Third_PartyLibyuvConvert PROPERTIES OUTPUT_NAME convert)
target_link_libraries(Third_PartyLibyuvConvert Third_PartyLibyuvLibyuv)
add_dependencies(Third_PartyLibyuvConvert Third_PartyLibyuvLibyuv)
# End of target Third_PartyLibyuvConvert



# Start of target Third_PartyProtobufProtoc
set(Third_PartyProtobufProtoc_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/main.cc)
add_executable(Third_PartyProtobufProtoc ${Third_PartyProtobufProtoc_SOURCES})
target_compile_options(Third_PartyProtobufProtoc PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Werror" "-Wall" "-Wno-unused-variable" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyProtobufProtoc PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(Third_PartyProtobufProtoc PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(Third_PartyProtobufProtoc PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9 -Wl,-ObjC -Wl,-pie")
set_target_properties(Third_PartyProtobufProtoc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/)
set_target_properties(Third_PartyProtobufProtoc PROPERTIES OUTPUT_NAME protoc)
target_link_libraries(Third_PartyProtobufProtoc Third_PartyProtobufProtoc_Lib)
add_dependencies(Third_PartyProtobufProtoc BuildWinDefault_Exe_Manifest Third_PartyProtobufProtoc_Lib)
# End of target Third_PartyProtobufProtoc


# Start of target Third_PartyBoringsslBoringssl_Asm
set(Third_PartyBoringsslBoringssl_Asm_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/aes/aes-x86_64.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/aes/aesni-x86_64.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/aes/bsaes-x86_64.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/aes/vpaes-x86_64.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/bn/rsaz-avx2.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/bn/rsaz-x86_64.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/bn/x86_64-mont.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/bn/x86_64-mont5.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/chacha/chacha-x86_64.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/ec/p256-x86_64-asm.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/md5/md5-x86_64.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/modes/aesni-gcm-x86_64.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/modes/ghash-x86_64.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/rand/rdrand-x86_64.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/sha/sha1-x86_64.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/sha/sha256-x86_64.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/mac-x86_64/crypto/sha/sha512-x86_64.S
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/curve25519/asm/x25519-asm-x86_64.S)
add_library(Third_PartyBoringsslBoringssl_Asm OBJECT ${Third_PartyBoringsslBoringssl_Asm_SOURCES})
target_compile_options(Third_PartyBoringsslBoringssl_Asm PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyBoringsslBoringssl_Asm PRIVATE BORINGSSL_CLANG_SUPPORTS_DOT_ARCH V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1)
target_include_directories(Third_PartyBoringsslBoringssl_Asm PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/include/)
set_target_properties(Third_PartyBoringsslBoringssl_Asm PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
# End of target Third_PartyBoringsslBoringssl_Asm

# Start of target WebrtcModulesAudio_CodingRent_A_Codec
set(WebrtcModulesAudio_CodingRent_A_Codec_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/acm2/acm_codec_database.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/acm2/rent_a_codec.cc)
add_library(WebrtcModulesAudio_CodingRent_A_Codec STATIC ${WebrtcModulesAudio_CodingRent_A_Codec_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingRent_A_Codec PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingRent_A_Codec PRIVATE WEBRTC_CODEC_ILBC WEBRTC_CODEC_OPUS WEBRTC_CODEC_ISAC WEBRTC_CODEC_RED V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingRent_A_Codec PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/red/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/pcm16b/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/cng/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingRent_A_Codec PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingRent_A_Codec PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingRent_A_Codec PROPERTIES OUTPUT_NAME rent_a_codec)
target_link_libraries(WebrtcModulesAudio_CodingRent_A_Codec WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesAudio_CodingRent_A_Codec WebrtcModulesAudio_CodingCng)
target_link_libraries(WebrtcModulesAudio_CodingRent_A_Codec WebrtcModulesAudio_CodingIlbc)
target_link_libraries(WebrtcModulesAudio_CodingRent_A_Codec WebrtcModulesAudio_CodingIsac)
target_link_libraries(WebrtcModulesAudio_CodingRent_A_Codec WebrtcModulesAudio_CodingPcm16B)
target_link_libraries(WebrtcModulesAudio_CodingRent_A_Codec WebrtcModulesAudio_CodingRed)
target_link_libraries(WebrtcModulesAudio_CodingRent_A_Codec WebrtcModulesAudio_CodingWebrtc_Opus)
target_include_directories(WebrtcModulesAudio_CodingRent_A_Codec PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingRent_A_Codec ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingRent_A_Codec

# Start of target WebrtcMediaMedia
add_custom_target(WebrtcMediaMedia)
add_dependencies(WebrtcMediaMedia WebrtcMediaRtc_Media)
# End of target WebrtcMediaMedia



# Start of target BuildConfigPosixPosix
add_custom_target(BuildConfigPosixPosix)
# End of target BuildConfigPosixPosix




# Start of target WebrtcModulesAudio_CodingPcm16B
set(WebrtcModulesAudio_CodingPcm16B_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/pcm16b/audio_decoder_pcm16b.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/pcm16b/audio_encoder_pcm16b.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/pcm16b/pcm16b.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/g711/audio_encoder_pcm.cc)
add_library(WebrtcModulesAudio_CodingPcm16B STATIC ${WebrtcModulesAudio_CodingPcm16B_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingPcm16B PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingPcm16B PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingPcm16B PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/pcm16b/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingPcm16B PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingPcm16B PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingPcm16B PROPERTIES OUTPUT_NAME pcm16b)
target_link_libraries(WebrtcModulesAudio_CodingPcm16B WebrtcModulesAudio_CodingAudio_Decoder_Interface)
target_link_libraries(WebrtcModulesAudio_CodingPcm16B WebrtcModulesAudio_CodingAudio_Encoder_Interface)
target_include_directories(WebrtcModulesAudio_CodingPcm16B PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingPcm16B ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingPcm16B

# Start of target Third_PartyUsrsctpUsrsctp
set(Third_PartyUsrsctpUsrsctp_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_asconf.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_auth.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_bsd_addr.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_callout.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_cc_functions.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_crc32.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_indata.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_input.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_output.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_pcb.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_peeloff.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_sha1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_ss_functions.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_sysctl.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_timer.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_userspace.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_usrreq.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctputil.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet6/sctp6_usrreq.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/user_environment.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/user_mbuf.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/user_recv_thread.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/user_socket.c)
add_library(Third_PartyUsrsctpUsrsctp STATIC ${Third_PartyUsrsctpUsrsctp_SOURCES})
target_compile_options(Third_PartyUsrsctpUsrsctp PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-UINET" "-UINET6" "-U__APPLE__" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Werror" "-Wall" "-Wno-unused-variable" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-Wno-incompatible-pointer-types" "-Wno-unused-function" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyUsrsctpUsrsctp PRIVATE SCTP_PROCESS_LEVEL_LOCKS SCTP_SIMPLE_ALLOCATOR SCTP_USE_OPENSSL_SHA1 __Userspace__ HAVE_SA_LEN HAVE_SCONN_LEN __APPLE_USE_RFC_2292 __Userspace_os_Darwin NON_WINDOWS_DEFINE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1)
target_include_directories(Third_PartyUsrsctpUsrsctp PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/ ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/include/)
set_target_properties(Third_PartyUsrsctpUsrsctp PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(Third_PartyUsrsctpUsrsctp PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/third_party/usrsctp)
set_target_properties(Third_PartyUsrsctpUsrsctp PROPERTIES OUTPUT_NAME usrsctp)
target_link_libraries(Third_PartyUsrsctpUsrsctp Third_PartyBoringsslBoringssl)
# End of target Third_PartyUsrsctpUsrsctp


# Start of target WebrtcModulesDesktop_CaptureDesktop_Capture
set(WebrtcModulesDesktop_CaptureDesktop_Capture_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/cropped_desktop_frame.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/cropping_window_capturer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/desktop_and_cursor_composer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/desktop_capture_options.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/desktop_capturer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/mac/desktop_configuration.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/mac/desktop_configuration_monitor.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/mac/full_screen_chrome_window_detector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/mac/scoped_pixel_buffer_object.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/mac/window_list_utils.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/mouse_cursor.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/mouse_cursor_monitor_mac.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/screen_capturer_helper.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/screen_capturer_mac.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/shared_memory.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/window_capturer_mac.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/desktop_capturer_differ_wrapper.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/differ_block.cc)
add_library(WebrtcModulesDesktop_CaptureDesktop_Capture STATIC ${WebrtcModulesDesktop_CaptureDesktop_Capture_SOURCES})
target_compile_options(WebrtcModulesDesktop_CaptureDesktop_Capture PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesDesktop_CaptureDesktop_Capture PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC SSL_USE_OPENSSL HAVE_OPENSSL_SSL_H FEATURE_ENABLE_SSL)
target_include_directories(WebrtcModulesDesktop_CaptureDesktop_Capture PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesDesktop_CaptureDesktop_Capture PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesDesktop_CaptureDesktop_Capture PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/desktop_capture)
set_target_properties(WebrtcModulesDesktop_CaptureDesktop_Capture PROPERTIES OUTPUT_NAME desktop_capture)
target_link_libraries(WebrtcModulesDesktop_CaptureDesktop_Capture WebrtcBaseRtc_Base)
target_link_libraries(WebrtcModulesDesktop_CaptureDesktop_Capture WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2)
target_link_libraries(WebrtcModulesDesktop_CaptureDesktop_Capture WebrtcModulesDesktop_CapturePrimitives)
target_link_libraries(WebrtcModulesDesktop_CaptureDesktop_Capture WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesDesktop_CaptureDesktop_Capture PRIVATE ${APPKIT_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CaptureDesktop_Capture ${APPKIT_LIBRARY})
target_include_directories(WebrtcModulesDesktop_CaptureDesktop_Capture PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CaptureDesktop_Capture ${IOKIT_LIBRARY})
target_include_directories(WebrtcModulesDesktop_CaptureDesktop_Capture PRIVATE ${OPENGL_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CaptureDesktop_Capture ${OPENGL_LIBRARY})
target_include_directories(WebrtcModulesDesktop_CaptureDesktop_Capture PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CaptureDesktop_Capture ${COCOA_LIBRARY})
target_include_directories(WebrtcModulesDesktop_CaptureDesktop_Capture PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CaptureDesktop_Capture ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcModulesDesktop_CaptureDesktop_Capture PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CaptureDesktop_Capture ${SECURITY_LIBRARY})
target_include_directories(WebrtcModulesDesktop_CaptureDesktop_Capture PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CaptureDesktop_Capture ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcModulesDesktop_CaptureDesktop_Capture PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CaptureDesktop_Capture ${APPLICATIONSERVICES_LIBRARY})
# End of target WebrtcModulesDesktop_CaptureDesktop_Capture

# Start of target WebrtcAudioAudio
set(WebrtcAudioAudio_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/audio/audio_receive_stream.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/audio/audio_send_stream.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/audio/audio_state.cc
    $<TARGET_OBJECTS:WebrtcApiAudio_Mixer_Api>)
add_library(WebrtcAudioAudio STATIC ${WebrtcAudioAudio_SOURCES})
target_compile_options(WebrtcAudioAudio PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcAudioAudio PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcAudioAudio PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcAudioAudio PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcAudioAudio PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/audio)
set_target_properties(WebrtcAudioAudio PROPERTIES OUTPUT_NAME audio)
target_link_libraries(WebrtcAudioAudio WebrtcWebrtc_Common)
target_link_libraries(WebrtcAudioAudio WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcAudioAudio WebrtcApiCall_Api)
target_link_libraries(WebrtcAudioAudio WebrtcSystem_WrappersSystem_Wrappers)
target_link_libraries(WebrtcAudioAudio WebrtcVoice_EngineVoice_Engine)
target_include_directories(WebrtcAudioAudio PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcAudioAudio ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcAudioAudio PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcAudioAudio ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcAudioAudio PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcAudioAudio ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcAudioAudio PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcAudioAudio ${COREAUDIO_LIBRARY})
target_include_directories(WebrtcAudioAudio PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcAudioAudio ${COREGRAPHICS_LIBRARY})
target_include_directories(WebrtcAudioAudio PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcAudioAudio ${COREVIDEO_LIBRARY})
# End of target WebrtcAudioAudio

# Start of target WebrtcModulesVideo_CodingWebrtc_I420
set(WebrtcModulesVideo_CodingWebrtc_I420_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/codecs/i420/i420.cc)
add_library(WebrtcModulesVideo_CodingWebrtc_I420 STATIC ${WebrtcModulesVideo_CodingWebrtc_I420_SOURCES})
target_compile_options(WebrtcModulesVideo_CodingWebrtc_I420 PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesVideo_CodingWebrtc_I420 PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesVideo_CodingWebrtc_I420 PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesVideo_CodingWebrtc_I420 PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesVideo_CodingWebrtc_I420 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/video_coding)
set_target_properties(WebrtcModulesVideo_CodingWebrtc_I420 PROPERTIES OUTPUT_NAME webrtc_i420)
target_link_libraries(WebrtcModulesVideo_CodingWebrtc_I420 WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesVideo_CodingWebrtc_I420 PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CodingWebrtc_I420 ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesVideo_CodingWebrtc_I420

# Start of target Third_PartyLibyuvLibyuv
set(Third_PartyLibyuvLibyuv_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/compare.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/compare_common.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/compare_gcc.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/convert.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/convert_argb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/convert_from.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/convert_from_argb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/convert_jpeg.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/convert_to_argb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/convert_to_i420.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/cpu_id.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/mjpeg_decoder.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/mjpeg_validate.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/planar_functions.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/rotate.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/rotate_any.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/rotate_argb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/rotate_common.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/rotate_mips.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/rotate_gcc.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/row_any.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/row_common.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/row_mips.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/row_gcc.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/scale.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/scale_any.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/scale_argb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/scale_common.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/scale_mips.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/scale_gcc.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/source/video_common.cc)
add_library(Third_PartyLibyuvLibyuv STATIC ${Third_PartyLibyuvLibyuv_SOURCES})
target_compile_options(Third_PartyLibyuvLibyuv PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyLibyuvLibyuv PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1)
target_include_directories(Third_PartyLibyuvLibyuv PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/libjpeg_turbo/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(Third_PartyLibyuvLibyuv PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(Third_PartyLibyuvLibyuv PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/third_party/libyuv)
set_target_properties(Third_PartyLibyuvLibyuv PROPERTIES OUTPUT_NAME yuv)
# End of target Third_PartyLibyuvLibyuv


# Start of target WebrtcModulesAudio_CodingAudio_Encoder_Interface
set(WebrtcModulesAudio_CodingAudio_Encoder_Interface_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/audio_encoder.cc)
add_library(WebrtcModulesAudio_CodingAudio_Encoder_Interface STATIC ${WebrtcModulesAudio_CodingAudio_Encoder_Interface_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingAudio_Encoder_Interface PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingAudio_Encoder_Interface PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingAudio_Encoder_Interface PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingAudio_Encoder_Interface PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingAudio_Encoder_Interface PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingAudio_Encoder_Interface PROPERTIES OUTPUT_NAME audio_encoder_interface)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Encoder_Interface WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Encoder_Interface WebrtcBaseRtc_Base_Approved)
target_include_directories(WebrtcModulesAudio_CodingAudio_Encoder_Interface PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingAudio_Encoder_Interface ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingAudio_Encoder_Interface

# Start of target WebrtcWebrtc_Common
set(WebrtcWebrtc_Common_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/common_types.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/config.cc)
add_library(WebrtcWebrtc_Common STATIC ${WebrtcWebrtc_Common_SOURCES})
target_compile_options(WebrtcWebrtc_Common PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcWebrtc_Common PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcWebrtc_Common PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcWebrtc_Common PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcWebrtc_Common PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc)
set_target_properties(WebrtcWebrtc_Common PROPERTIES OUTPUT_NAME webrtc_common)
# End of target WebrtcWebrtc_Common


# Start of target BuildConfigSanitizersDeps
add_custom_target(BuildConfigSanitizersDeps)
add_dependencies(BuildConfigSanitizersDeps BuildConfigSanitizersDeps_No_Options)
# End of target BuildConfigSanitizersDeps


# Start of target Third_PartyGflagsGflags
set(Third_PartyGflagsGflags_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/gflags/src/src/gflags.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/gflags/src/src/gflags_completions.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/gflags/src/src/gflags_reporting.cc)
add_library(Third_PartyGflagsGflags OBJECT ${Third_PartyGflagsGflags_SOURCES})
target_compile_options(Third_PartyGflagsGflags PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wno-microsoft-include" "-O0" "-fvisibility=hidden" "-Werror" "-Wall" "-Wno-unused-variable" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-Wno-unused-local-typedef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyGflagsGflags PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 GFLAGS_DLL_DECL= GFLAGS_DLL_DECLARE_FLAG= GFLAGS_DLL_DEFINE_FLAG=)
target_include_directories(Third_PartyGflagsGflags PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/gflags/gen/posix/include/gflags/ ${LIBWEBRTC_INPUT_DIR}/third_party/gflags/src/src/ ${LIBWEBRTC_INPUT_DIR}/third_party/gflags/gen/posix/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/gflags/gen/posix/include/private/)
set_target_properties(Third_PartyGflagsGflags PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
# End of target Third_PartyGflagsGflags

# Start of target Third_PartyLibyuvPsnr
set(Third_PartyLibyuvPsnr_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/util/psnr_main.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/util/psnr.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/util/ssim.cc)
add_executable(Third_PartyLibyuvPsnr ${Third_PartyLibyuvPsnr_SOURCES})
target_compile_options(Third_PartyLibyuvPsnr PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyLibyuvPsnr PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1)
target_include_directories(Third_PartyLibyuvPsnr PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(Third_PartyLibyuvPsnr PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9 -Wl,-ObjC -Wl,-pie")
set_target_properties(Third_PartyLibyuvPsnr PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/)
set_target_properties(Third_PartyLibyuvPsnr PROPERTIES OUTPUT_NAME psnr)
target_link_libraries(Third_PartyLibyuvPsnr Third_PartyLibyuvLibyuv)
add_dependencies(Third_PartyLibyuvPsnr Third_PartyLibyuvLibyuv)
# End of target Third_PartyLibyuvPsnr


# Start of target WebrtcModulesAudio_CodingAudio_Network_Adaptor
set(WebrtcModulesAudio_CodingAudio_Network_Adaptor_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/audio_network_adaptor/audio_network_adaptor.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/audio_network_adaptor/audio_network_adaptor_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/audio_network_adaptor/bitrate_controller.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/audio_network_adaptor/channel_controller.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/audio_network_adaptor/controller.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/audio_network_adaptor/controller_manager.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/audio_network_adaptor/debug_dump_writer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/audio_network_adaptor/dtx_controller.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/audio_network_adaptor/fec_controller.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/audio_network_adaptor/frame_length_controller.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/audio_network_adaptor/smoothing_filter.cc)
add_library(WebrtcModulesAudio_CodingAudio_Network_Adaptor STATIC ${WebrtcModulesAudio_CodingAudio_Network_Adaptor_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingAudio_Network_Adaptor PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingAudio_Network_Adaptor PRIVATE WEBRTC_AUDIO_NETWORK_ADAPTOR_DEBUG_DUMP V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(WebrtcModulesAudio_CodingAudio_Network_Adaptor PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(WebrtcModulesAudio_CodingAudio_Network_Adaptor PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingAudio_Network_Adaptor PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingAudio_Network_Adaptor PROPERTIES OUTPUT_NAME audio_network_adaptor)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Network_Adaptor WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Network_Adaptor WebrtcModulesAudio_CodingAna_Config_Proto)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Network_Adaptor WebrtcModulesAudio_CodingAna_Debug_Dump_Proto)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Network_Adaptor WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesAudio_CodingAudio_Network_Adaptor PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingAudio_Network_Adaptor ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingAudio_Network_Adaptor

# Start of target WebrtcCommon_VideoCommon_Video
set(WebrtcCommon_VideoCommon_Video_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/bitrate_adjuster.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/h264/h264_bitstream_parser.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/h264/h264_common.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/h264/pps_parser.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/h264/profile_level_id.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/h264/sps_parser.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/h264/sps_vui_rewriter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/i420_buffer_pool.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/incoming_video_stream.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/webrtc_libyuv.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/video_frame.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/video_frame_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/video_render_frames.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/corevideo_frame_buffer.cc)
add_library(WebrtcCommon_VideoCommon_Video STATIC ${WebrtcCommon_VideoCommon_Video_SOURCES})
target_compile_options(WebrtcCommon_VideoCommon_Video PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcCommon_VideoCommon_Video PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcCommon_VideoCommon_Video PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/interface/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcCommon_VideoCommon_Video PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcCommon_VideoCommon_Video PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/common_video)
set_target_properties(WebrtcCommon_VideoCommon_Video PROPERTIES OUTPUT_NAME common_video)
target_link_libraries(WebrtcCommon_VideoCommon_Video Third_PartyLibyuvLibyuv)
target_link_libraries(WebrtcCommon_VideoCommon_Video WebrtcWebrtc_Common)
target_link_libraries(WebrtcCommon_VideoCommon_Video WebrtcBaseRtc_Task_Queue)
target_link_libraries(WebrtcCommon_VideoCommon_Video WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcCommon_VideoCommon_Video PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcCommon_VideoCommon_Video ${COREVIDEO_LIBRARY})
target_include_directories(WebrtcCommon_VideoCommon_Video PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcCommon_VideoCommon_Video ${FOUNDATION_LIBRARY})
# End of target WebrtcCommon_VideoCommon_Video

# Start of target WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2
set(WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/differ_vector_sse2.cc)
add_library(WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2 STATIC ${WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2_SOURCES})
target_compile_options(WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2 PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-msse2" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2 PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2 PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2 PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/desktop_capture)
set_target_properties(WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2 PROPERTIES OUTPUT_NAME desktop_capture_differ_sse2)
# End of target WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2





# Start of target WebrtcModulesAudio_CodingAudio_Format_Conversion
set(WebrtcModulesAudio_CodingAudio_Format_Conversion_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/audio_format_conversion.cc)
add_library(WebrtcModulesAudio_CodingAudio_Format_Conversion STATIC ${WebrtcModulesAudio_CodingAudio_Format_Conversion_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingAudio_Format_Conversion PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingAudio_Format_Conversion PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingAudio_Format_Conversion PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingAudio_Format_Conversion PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingAudio_Format_Conversion PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingAudio_Format_Conversion PROPERTIES OUTPUT_NAME audio_format_conversion)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Format_Conversion WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Format_Conversion WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Format_Conversion WebrtcModulesAudio_CodingAudio_Format)
target_include_directories(WebrtcModulesAudio_CodingAudio_Format_Conversion PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingAudio_Format_Conversion ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingAudio_Format_Conversion



# Start of target WebrtcModulesVideo_ProcessingVideo_Processing
set(WebrtcModulesVideo_ProcessingVideo_Processing_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_processing/frame_preprocessor.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_processing/spatial_resampler.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_processing/util/denoiser_filter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_processing/util/denoiser_filter_c.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_processing/util/noise_estimation.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_processing/util/skin_detection.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_processing/video_decimator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_processing/video_denoiser.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_processing/video_processing_impl.cc)
add_library(WebrtcModulesVideo_ProcessingVideo_Processing STATIC ${WebrtcModulesVideo_ProcessingVideo_Processing_SOURCES})
target_compile_options(WebrtcModulesVideo_ProcessingVideo_Processing PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesVideo_ProcessingVideo_Processing PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesVideo_ProcessingVideo_Processing PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesVideo_ProcessingVideo_Processing PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesVideo_ProcessingVideo_Processing PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/video_processing)
set_target_properties(WebrtcModulesVideo_ProcessingVideo_Processing PROPERTIES OUTPUT_NAME video_processing)
target_link_libraries(WebrtcModulesVideo_ProcessingVideo_Processing WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcModulesVideo_ProcessingVideo_Processing WebrtcCommon_VideoCommon_Video)
target_link_libraries(WebrtcModulesVideo_ProcessingVideo_Processing WebrtcModulesUtilityUtility)
target_link_libraries(WebrtcModulesVideo_ProcessingVideo_Processing WebrtcModulesVideo_ProcessingVideo_Processing_Sse2)
target_link_libraries(WebrtcModulesVideo_ProcessingVideo_Processing WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesVideo_ProcessingVideo_Processing PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_ProcessingVideo_Processing ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcModulesVideo_ProcessingVideo_Processing PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_ProcessingVideo_Processing ${COREVIDEO_LIBRARY})
# End of target WebrtcModulesVideo_ProcessingVideo_Processing

# Start of target WebrtcModulesAudio_CodingAudio_Decoder_Factory_Interface
add_library(WebrtcModulesAudio_CodingAudio_Decoder_Factory_Interface OBJECT)
target_compile_options(WebrtcModulesAudio_CodingAudio_Decoder_Factory_Interface PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingAudio_Decoder_Factory_Interface PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingAudio_Decoder_Factory_Interface PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingAudio_Decoder_Factory_Interface PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
# End of target WebrtcModulesAudio_CodingAudio_Decoder_Factory_Interface

# Start of target WebrtcCallCall
set(WebrtcCallCall_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/call/bitrate_allocator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/call/call.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/call/flexfec_receive_stream.cc)
add_library(WebrtcCallCall STATIC ${WebrtcCallCall_SOURCES})
target_compile_options(WebrtcCallCall PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcCallCall PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcCallCall PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcCallCall PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcCallCall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/call)
set_target_properties(WebrtcCallCall PROPERTIES OUTPUT_NAME call)
target_link_libraries(WebrtcCallCall WebrtcWebrtc_Common)
target_link_libraries(WebrtcCallCall WebrtcApiCall_Api)
target_link_libraries(WebrtcCallCall WebrtcAudioAudio)
target_link_libraries(WebrtcCallCall WebrtcBaseRtc_Task_Queue)
target_link_libraries(WebrtcCallCall WebrtcLoggingRtc_Event_Log_Impl)
target_link_libraries(WebrtcCallCall WebrtcModulesCongestion_ControllerCongestion_Controller)
target_link_libraries(WebrtcCallCall WebrtcModulesRtp_RtcpRtp_Rtcp)
target_link_libraries(WebrtcCallCall WebrtcSystem_WrappersSystem_Wrappers)
target_link_libraries(WebrtcCallCall WebrtcVideoVideo)
target_include_directories(WebrtcCallCall PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcCallCall ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcCallCall PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcCallCall ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcCallCall PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcCallCall ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcCallCall PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcCallCall ${COREAUDIO_LIBRARY})
target_include_directories(WebrtcCallCall PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcCallCall ${COREGRAPHICS_LIBRARY})
target_include_directories(WebrtcCallCall PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcCallCall ${COREVIDEO_LIBRARY})
# End of target WebrtcCallCall

# Start of target WebrtcModulesAudio_ProcessingAudio_Processing
set(WebrtcModulesAudio_ProcessingAudio_Processing_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/aec/aec_core.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/aec/aec_resampler.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/aec/echo_cancellation.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/aecm/aecm_core.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/aecm/echo_control_mobile.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/agc/agc.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/agc/agc_manager_direct.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/agc/legacy/analog_agc.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/agc/legacy/digital_agc.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/agc/loudness_histogram.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/agc/utility.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/audio_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/audio_processing_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/beamformer/array_util.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/beamformer/covariance_matrix_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/beamformer/nonlinear_beamformer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/echo_cancellation_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/echo_control_mobile_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/echo_detector/circular_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/echo_detector/mean_variance_estimator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/echo_detector/normalized_covariance_estimator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/gain_control_for_experimental_agc.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/gain_control_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/high_pass_filter_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/include/audio_processing.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/include/config.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/level_controller/biquad_filter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/level_controller/down_sampler.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/level_controller/gain_applier.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/level_controller/gain_selector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/level_controller/level_controller.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/level_controller/noise_level_estimator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/level_controller/noise_spectrum_estimator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/level_controller/peak_level_estimator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/level_controller/saturating_gain_estimator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/level_controller/signal_classifier.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/level_estimator_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/logging/apm_data_dumper.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/noise_suppression_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/residual_echo_detector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/rms_level.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/splitting_filter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/three_band_filter_bank.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/transient/moving_moments.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/transient/transient_detector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/transient/transient_suppressor.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/transient/wpd_node.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/transient/wpd_tree.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/typing_detection.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/utility/block_mean_calculator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/utility/delay_estimator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/utility/delay_estimator_wrapper.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/utility/ooura_fft.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/vad/gmm.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/vad/pitch_based_vad.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/vad/pitch_internal.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/vad/pole_zero_filter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/vad/standalone_vad.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/vad/vad_audio_proc.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/vad/vad_circular_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/vad/voice_activity_detector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/voice_detection_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/ns/noise_suppression.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/ns/ns_core.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/aecm/aecm_core_c.cc)
add_library(WebrtcModulesAudio_ProcessingAudio_Processing STATIC ${WebrtcModulesAudio_ProcessingAudio_Processing_SOURCES})
target_compile_options(WebrtcModulesAudio_ProcessingAudio_Processing PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_ProcessingAudio_Processing PRIVATE WEBRTC_APM_DEBUG_DUMP=0 WEBRTC_AUDIOPROC_DEBUG_DUMP WEBRTC_INTELLIGIBILITY_ENHANCER=0 WEBRTC_NS_FLOAT V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(WebrtcModulesAudio_ProcessingAudio_Processing PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_ProcessingAudio_Processing PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_ProcessingAudio_Processing PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_processing)
set_target_properties(WebrtcModulesAudio_ProcessingAudio_Processing PROPERTIES OUTPUT_NAME audio_processing)
target_link_libraries(WebrtcModulesAudio_ProcessingAudio_Processing WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesAudio_ProcessingAudio_Processing WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcModulesAudio_ProcessingAudio_Processing WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcModulesAudio_ProcessingAudio_Processing WebrtcModulesAudio_CodingIsac)
target_link_libraries(WebrtcModulesAudio_ProcessingAudio_Processing WebrtcModulesAudio_ProcessingAudio_Processing_Sse2)
target_link_libraries(WebrtcModulesAudio_ProcessingAudio_Processing WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto)
target_link_libraries(WebrtcModulesAudio_ProcessingAudio_Processing WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesAudio_ProcessingAudio_Processing PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_ProcessingAudio_Processing ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_ProcessingAudio_Processing

# Start of target WebrtcModulesVideo_CaptureVideo_Capture
set(WebrtcModulesVideo_CaptureVideo_Capture_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_capture/external/device_info_external.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_capture/external/video_capture_external.cc)
add_library(WebrtcModulesVideo_CaptureVideo_Capture STATIC ${WebrtcModulesVideo_CaptureVideo_Capture_SOURCES})
target_compile_options(WebrtcModulesVideo_CaptureVideo_Capture PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesVideo_CaptureVideo_Capture PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesVideo_CaptureVideo_Capture PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesVideo_CaptureVideo_Capture PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesVideo_CaptureVideo_Capture PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/video_capture)
set_target_properties(WebrtcModulesVideo_CaptureVideo_Capture PROPERTIES OUTPUT_NAME video_capture)
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture WebrtcModulesVideo_CaptureVideo_Capture_Module)
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesVideo_CaptureVideo_Capture PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcModulesVideo_CaptureVideo_Capture PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture ${COREVIDEO_LIBRARY})
# End of target WebrtcModulesVideo_CaptureVideo_Capture


# Start of target WebrtcMediaRtc_Media
set(WebrtcMediaRtc_Media_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/adaptedvideotracksource.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/codec.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/mediaconstants.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/mediaengine.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/rtpdataengine.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/rtpdump.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/rtputils.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/streamparams.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/turnutils.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/videoadapter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/videobroadcaster.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/videocapturer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/videocommon.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/base/videosourcebase.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/engine/payload_type_mapper.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/engine/simulcast.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/engine/videoencodersoftwarefallbackwrapper.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/engine/webrtcmediaengine.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/engine/webrtcvideocapturer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/engine/webrtcvideocapturerfactory.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/engine/webrtcvideoencoderfactory.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/engine/webrtcvideoengine2.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/engine/webrtcvoiceengine.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/media/sctp/sctpdataengine.cc)
add_library(WebrtcMediaRtc_Media STATIC ${WebrtcMediaRtc_Media_SOURCES})
target_compile_options(WebrtcMediaRtc_Media PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-Wno-deprecated-declarations" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcMediaRtc_Media PRIVATE WEBRTC_INTELLIGIBILITY_ENHANCER=0 V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC HAVE_WEBRTC_VIDEO HAVE_WEBRTC_VOICE FEATURE_ENABLE_VOICEMAIL XML_STATIC RTC_DISABLE_VP8 RTC_DISABLE_VP9)
target_include_directories(WebrtcMediaRtc_Media PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/expat/files/lib/ ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/netinet/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/usrsctp/usrsctplib/usrsctplib/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcMediaRtc_Media PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcMediaRtc_Media PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/media)
set_target_properties(WebrtcMediaRtc_Media PROPERTIES OUTPUT_NAME rtc_media)
target_link_libraries(WebrtcMediaRtc_Media Third_PartyLibyuvLibyuv)
target_link_libraries(WebrtcMediaRtc_Media Third_PartyUsrsctpUsrsctp)
target_link_libraries(WebrtcMediaRtc_Media WebrtcWebrtc_Common)
target_link_libraries(WebrtcMediaRtc_Media WebrtcApiCall_Api)
target_link_libraries(WebrtcMediaRtc_Media WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcMediaRtc_Media WebrtcCallCall)
target_link_libraries(WebrtcMediaRtc_Media WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl)
target_link_libraries(WebrtcMediaRtc_Media WebrtcModulesVideo_CodingVideo_Coding)
target_link_libraries(WebrtcMediaRtc_Media WebrtcP2PLibstunprober)
target_link_libraries(WebrtcMediaRtc_Media WebrtcP2PRtc_P2P)
target_link_libraries(WebrtcMediaRtc_Media WebrtcSystem_WrappersSystem_Wrappers)
target_link_libraries(WebrtcMediaRtc_Media WebrtcVideoVideo)
target_link_libraries(WebrtcMediaRtc_Media WebrtcVoice_EngineVoice_Engine)
target_include_directories(WebrtcMediaRtc_Media PRIVATE ${AVFOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcMediaRtc_Media ${AVFOUNDATION_LIBRARY})
target_include_directories(WebrtcMediaRtc_Media PRIVATE ${COREMEDIA_INCLUDE_DIR})
target_link_libraries(WebrtcMediaRtc_Media ${COREMEDIA_LIBRARY})
target_include_directories(WebrtcMediaRtc_Media PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcMediaRtc_Media ${COREVIDEO_LIBRARY})
target_include_directories(WebrtcMediaRtc_Media PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcMediaRtc_Media ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcMediaRtc_Media PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcMediaRtc_Media ${COCOA_LIBRARY})
target_include_directories(WebrtcMediaRtc_Media PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcMediaRtc_Media ${IOKIT_LIBRARY})
target_include_directories(WebrtcMediaRtc_Media PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcMediaRtc_Media ${SECURITY_LIBRARY})
target_include_directories(WebrtcMediaRtc_Media PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcMediaRtc_Media ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcMediaRtc_Media PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcMediaRtc_Media ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcMediaRtc_Media PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcMediaRtc_Media ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcMediaRtc_Media PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcMediaRtc_Media ${COREAUDIO_LIBRARY})
target_include_directories(WebrtcMediaRtc_Media PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcMediaRtc_Media ${COREGRAPHICS_LIBRARY})
# End of target WebrtcMediaRtc_Media



# Start of target WebrtcModulesAudio_CodingWebrtc_Opus
set(WebrtcModulesAudio_CodingWebrtc_Opus_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/opus/audio_decoder_opus.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/opus/audio_encoder_opus.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/opus/opus_interface.c)
add_library(WebrtcModulesAudio_CodingWebrtc_Opus STATIC ${WebrtcModulesAudio_CodingWebrtc_Opus_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingWebrtc_Opus PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingWebrtc_Opus PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingWebrtc_Opus PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingWebrtc_Opus PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingWebrtc_Opus PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingWebrtc_Opus PROPERTIES OUTPUT_NAME webrtc_opus)
target_link_libraries(WebrtcModulesAudio_CodingWebrtc_Opus Third_PartyOpusOpus)
target_link_libraries(WebrtcModulesAudio_CodingWebrtc_Opus WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcModulesAudio_CodingWebrtc_Opus WebrtcModulesAudio_CodingAudio_Decoder_Interface)
target_link_libraries(WebrtcModulesAudio_CodingWebrtc_Opus WebrtcModulesAudio_CodingAudio_Encoder_Interface)
target_link_libraries(WebrtcModulesAudio_CodingWebrtc_Opus WebrtcModulesAudio_CodingAudio_Network_Adaptor)
target_include_directories(WebrtcModulesAudio_CodingWebrtc_Opus PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingWebrtc_Opus ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingWebrtc_Opus

# Start of target WebrtcToolsChart_Proto
set(WebrtcToolsChart_Proto_SOURCES ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/tools/event_log_visualizer/chart.pb.cc
    ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/tools/event_log_visualizer/chart_pb2.py)
add_library(WebrtcToolsChart_Proto STATIC ${WebrtcToolsChart_Proto_SOURCES})
target_compile_options(WebrtcToolsChart_Proto PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcToolsChart_Proto PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(WebrtcToolsChart_Proto PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(WebrtcToolsChart_Proto PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcToolsChart_Proto PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/tools)
set_target_properties(WebrtcToolsChart_Proto PROPERTIES OUTPUT_NAME chart_proto)
target_link_libraries(WebrtcToolsChart_Proto Third_PartyProtobufProtobuf_Lite)
# End of target WebrtcToolsChart_Proto

# Start of target WebrtcApiRtc_Stats_Api
add_library(WebrtcApiRtc_Stats_Api OBJECT)
target_compile_options(WebrtcApiRtc_Stats_Api PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcApiRtc_Stats_Api PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcApiRtc_Stats_Api PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcApiRtc_Stats_Api PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
# End of target WebrtcApiRtc_Stats_Api


# Start of target WebrtcModulesAudio_CodingAna_Debug_Dump_Proto
set(WebrtcModulesAudio_CodingAna_Debug_Dump_Proto_SOURCES ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor/debug_dump.pb.cc
    ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_coding/audio_network_adaptor/debug_dump_pb2.py)
add_library(WebrtcModulesAudio_CodingAna_Debug_Dump_Proto STATIC ${WebrtcModulesAudio_CodingAna_Debug_Dump_Proto_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingAna_Debug_Dump_Proto PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingAna_Debug_Dump_Proto PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(WebrtcModulesAudio_CodingAna_Debug_Dump_Proto PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(WebrtcModulesAudio_CodingAna_Debug_Dump_Proto PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingAna_Debug_Dump_Proto PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingAna_Debug_Dump_Proto PROPERTIES OUTPUT_NAME ana_debug_dump_proto)
target_link_libraries(WebrtcModulesAudio_CodingAna_Debug_Dump_Proto Third_PartyProtobufProtobuf_Lite)
# End of target WebrtcModulesAudio_CodingAna_Debug_Dump_Proto


# Start of target Third_PartyProtobufCopy_Six
list(APPEND Third_PartyProtobufCopy_Six ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/third_party/six/six.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/third_party/six/six.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/third_party/six/six.py)
add_custom_target(Third_PartyProtobufCopy_Six
    COMMAND echo "Generating copy targetThird_PartyProtobufCopy_Six"
    VERBATIM)
# End of target Third_PartyProtobufCopy_Six

# Start of target WebrtcPcPc
add_custom_target(WebrtcPcPc)
add_dependencies(WebrtcPcPc WebrtcPcRtc_Pc)
# End of target WebrtcPcPc

# Start of target WebrtcLibjingleXmppRtc_Xmpp
set(WebrtcLibjingleXmppRtc_Xmpp_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/constants.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/jid.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/saslmechanism.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/xmppclient.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/xmppengineimpl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/xmppengineimpl_iq.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/xmpplogintask.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/xmppstanzaparser.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/xmpptask.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/chatroommoduleimpl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/discoitemsquerytask.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/hangoutpubsubclient.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/iqtask.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/moduleimpl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/mucroomconfigtask.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/mucroomdiscoverytask.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/mucroomlookuptask.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/mucroomuniquehangoutidtask.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/pingtask.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/presenceouttask.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/presencereceivetask.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/presencestatus.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/pubsub_task.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/pubsubclient.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/pubsubstateclient.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/pubsubtasks.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/receivetask.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/rostermoduleimpl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/xmppauth.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/xmpppump.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/xmppsocket.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmpp/xmppthread.cc)
add_library(WebrtcLibjingleXmppRtc_Xmpp STATIC ${WebrtcLibjingleXmppRtc_Xmpp_SOURCES})
target_compile_options(WebrtcLibjingleXmppRtc_Xmpp PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcLibjingleXmppRtc_Xmpp PRIVATE FEATURE_ENABLE_SSL FEATURE_ENABLE_VOICEMAIL FEATURE_ENABLE_PSTN _DEBUG V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC FEATURE_ENABLE_SSL FEATURE_ENABLE_VOICEMAIL XML_STATIC SSL_USE_OPENSSL HAVE_OPENSSL_SSL_H FEATURE_ENABLE_SSL)
target_include_directories(WebrtcLibjingleXmppRtc_Xmpp PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/expat/files/lib/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcLibjingleXmppRtc_Xmpp PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcLibjingleXmppRtc_Xmpp PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/libjingle/xmpp)
set_target_properties(WebrtcLibjingleXmppRtc_Xmpp PROPERTIES OUTPUT_NAME rtc_xmpp)
target_link_libraries(WebrtcLibjingleXmppRtc_Xmpp Third_PartyExpatExpat)
target_link_libraries(WebrtcLibjingleXmppRtc_Xmpp WebrtcBaseRtc_Base)
target_link_libraries(WebrtcLibjingleXmppRtc_Xmpp WebrtcLibjingleXmlliteRtc_Xmllite)
target_include_directories(WebrtcLibjingleXmppRtc_Xmpp PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcLibjingleXmppRtc_Xmpp ${COCOA_LIBRARY})
target_include_directories(WebrtcLibjingleXmppRtc_Xmpp PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcLibjingleXmppRtc_Xmpp ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcLibjingleXmppRtc_Xmpp PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcLibjingleXmppRtc_Xmpp ${IOKIT_LIBRARY})
target_include_directories(WebrtcLibjingleXmppRtc_Xmpp PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcLibjingleXmppRtc_Xmpp ${SECURITY_LIBRARY})
target_include_directories(WebrtcLibjingleXmppRtc_Xmpp PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcLibjingleXmppRtc_Xmpp ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcLibjingleXmppRtc_Xmpp PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcLibjingleXmppRtc_Xmpp ${APPLICATIONSERVICES_LIBRARY})
# End of target WebrtcLibjingleXmppRtc_Xmpp


# Start of target WebrtcSdkRtc_Sdk_Peerconnection_Objc
set(WebrtcSdkRtc_Sdk_Peerconnection_Objc_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCAVFoundationVideoSource.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCAudioSource.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCAudioTrack.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCConfiguration.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCDataChannel.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCDataChannelConfiguration.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCI420Shader.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCIceCandidate.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCIceServer.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCLegacyStatsReport.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCMediaConstraints.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCMediaSource.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCMediaStream.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCMediaStreamTrack.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCOpenGLVideoRenderer.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCPeerConnection+DataChannel.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCPeerConnection+Stats.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCPeerConnection.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCPeerConnectionFactory.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCRtpCodecParameters.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCRtpEncodingParameters.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCRtpParameters.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCRtpReceiver.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCRtpSender.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCSessionDescription.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCShader.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCVideoFrame.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCVideoRendererAdapter.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCVideoSource.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCVideoTrack.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/avfoundationvideocapturer.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/videotoolboxvideocodecfactory.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCNSGLVideoView.m)
add_library(WebrtcSdkRtc_Sdk_Peerconnection_Objc STATIC ${WebrtcSdkRtc_Sdk_Peerconnection_Objc_SOURCES})
target_compile_options(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-arc" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Headers/ ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcSdkRtc_Sdk_Peerconnection_Objc PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcSdkRtc_Sdk_Peerconnection_Objc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/sdk)
set_target_properties(WebrtcSdkRtc_Sdk_Peerconnection_Objc PROPERTIES OUTPUT_NAME rtc_sdk_peerconnection_objc)
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc Third_PartyLibyuvLibyuv)
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc WebrtcApiLibjingle_Peerconnection)
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc WebrtcSdkRtc_Sdk_Common_Objc)
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${COREMEDIA_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc ${COREMEDIA_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${OPENGL_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc ${OPENGL_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${AVFOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc ${AVFOUNDATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc ${COCOA_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc ${IOKIT_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc ${SECURITY_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc ${COREVIDEO_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc ${COREAUDIO_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Peerconnection_Objc PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Peerconnection_Objc ${COREGRAPHICS_LIBRARY})
# End of target WebrtcSdkRtc_Sdk_Peerconnection_Objc

# Start of target WebrtcToolsForce_Mic_Volume_Max
set(WebrtcToolsForce_Mic_Volume_Max_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/force_mic_volume_max/force_mic_volume_max.cc)
add_executable(WebrtcToolsForce_Mic_Volume_Max ${WebrtcToolsForce_Mic_Volume_Max_SOURCES})
target_compile_options(WebrtcToolsForce_Mic_Volume_Max PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcToolsForce_Mic_Volume_Max PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcToolsForce_Mic_Volume_Max PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/dummy/)
set_target_properties(WebrtcToolsForce_Mic_Volume_Max PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9 -Wl,-ObjC -Wl,-pie")
set_target_properties(WebrtcToolsForce_Mic_Volume_Max PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/)
set_target_properties(WebrtcToolsForce_Mic_Volume_Max PROPERTIES OUTPUT_NAME force_mic_volume_max)
target_link_libraries(WebrtcToolsForce_Mic_Volume_Max WebrtcModulesAudio_DeviceAudio_Device)
target_link_libraries(WebrtcToolsForce_Mic_Volume_Max WebrtcSystem_WrappersField_Trial_Default)
target_link_libraries(WebrtcToolsForce_Mic_Volume_Max WebrtcSystem_WrappersMetrics_Default)
target_link_libraries(WebrtcToolsForce_Mic_Volume_Max WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcToolsForce_Mic_Volume_Max PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcToolsForce_Mic_Volume_Max ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcToolsForce_Mic_Volume_Max PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcToolsForce_Mic_Volume_Max ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcToolsForce_Mic_Volume_Max PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcToolsForce_Mic_Volume_Max ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcToolsForce_Mic_Volume_Max PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcToolsForce_Mic_Volume_Max ${COREAUDIO_LIBRARY})
target_include_directories(WebrtcToolsForce_Mic_Volume_Max PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcToolsForce_Mic_Volume_Max ${COREGRAPHICS_LIBRARY})
add_dependencies(WebrtcToolsForce_Mic_Volume_Max BuildConfigSanitizersDeps BuildWinDefault_Exe_Manifest WebrtcModulesAudio_DeviceAudio_Device WebrtcSystem_WrappersSystem_Wrappers_Default)
# End of target WebrtcToolsForce_Mic_Volume_Max



# Start of target WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils
set(WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/test/protobuf_utils.cc)
add_library(WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils STATIC ${WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils_SOURCES})
target_compile_options(WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_processing)
set_target_properties(WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils PROPERTIES OUTPUT_NAME audioproc_protobuf_utils)
target_link_libraries(WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto)
# End of target WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils

# Start of target WebrtcLibjingleXmlliteRtc_Xmllite
set(WebrtcLibjingleXmlliteRtc_Xmllite_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmllite/qname.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmllite/xmlbuilder.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmllite/xmlconstants.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmllite/xmlelement.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmllite/xmlnsstack.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmllite/xmlparser.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/libjingle/xmllite/xmlprinter.cc)
add_library(WebrtcLibjingleXmlliteRtc_Xmllite STATIC ${WebrtcLibjingleXmlliteRtc_Xmllite_SOURCES})
target_compile_options(WebrtcLibjingleXmlliteRtc_Xmllite PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcLibjingleXmlliteRtc_Xmllite PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC XML_STATIC SSL_USE_OPENSSL HAVE_OPENSSL_SSL_H FEATURE_ENABLE_SSL)
target_include_directories(WebrtcLibjingleXmlliteRtc_Xmllite PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/expat/files/lib/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcLibjingleXmlliteRtc_Xmllite PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcLibjingleXmlliteRtc_Xmllite PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/libjingle/xmllite)
set_target_properties(WebrtcLibjingleXmlliteRtc_Xmllite PROPERTIES OUTPUT_NAME rtc_xmllite)
target_link_libraries(WebrtcLibjingleXmlliteRtc_Xmllite Third_PartyExpatExpat)
target_link_libraries(WebrtcLibjingleXmlliteRtc_Xmllite WebrtcBaseRtc_Base)
target_include_directories(WebrtcLibjingleXmlliteRtc_Xmllite PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcLibjingleXmlliteRtc_Xmllite ${COCOA_LIBRARY})
target_include_directories(WebrtcLibjingleXmlliteRtc_Xmllite PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcLibjingleXmlliteRtc_Xmllite ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcLibjingleXmlliteRtc_Xmllite PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcLibjingleXmlliteRtc_Xmllite ${IOKIT_LIBRARY})
target_include_directories(WebrtcLibjingleXmlliteRtc_Xmllite PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcLibjingleXmlliteRtc_Xmllite ${SECURITY_LIBRARY})
target_include_directories(WebrtcLibjingleXmlliteRtc_Xmllite PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcLibjingleXmlliteRtc_Xmllite ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcLibjingleXmlliteRtc_Xmllite PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcLibjingleXmlliteRtc_Xmllite ${APPLICATIONSERVICES_LIBRARY})
# End of target WebrtcLibjingleXmlliteRtc_Xmllite


# Start of target WebrtcSdkRtc_Sdk_Framework_Objc_Info_Plist_Bundle_Data
add_custom_target(WebrtcSdkRtc_Sdk_Framework_Objc_Info_Plist_Bundle_Data)
# End of target WebrtcSdkRtc_Sdk_Framework_Objc_Info_Plist_Bundle_Data

# Start of target WebrtcLoggingRtc_Event_Log_Parser
set(WebrtcLoggingRtc_Event_Log_Parser_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/logging/rtc_event_log/rtc_event_log_parser.cc)
add_library(WebrtcLoggingRtc_Event_Log_Parser STATIC ${WebrtcLoggingRtc_Event_Log_Parser_SOURCES})
target_compile_options(WebrtcLoggingRtc_Event_Log_Parser PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcLoggingRtc_Event_Log_Parser PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(WebrtcLoggingRtc_Event_Log_Parser PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(WebrtcLoggingRtc_Event_Log_Parser PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcLoggingRtc_Event_Log_Parser PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/logging)
set_target_properties(WebrtcLoggingRtc_Event_Log_Parser PROPERTIES OUTPUT_NAME rtc_event_log_parser)
target_link_libraries(WebrtcLoggingRtc_Event_Log_Parser WebrtcWebrtc_Common)
target_link_libraries(WebrtcLoggingRtc_Event_Log_Parser WebrtcLoggingRtc_Event_Log_Proto)
# End of target WebrtcLoggingRtc_Event_Log_Parser

# Start of target WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer
set(WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_conference_mixer/source/audio_conference_mixer_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_conference_mixer/source/audio_frame_manipulator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_conference_mixer/source/time_scheduler.cc)
add_library(WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer STATIC ${WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer_SOURCES})
target_compile_options(WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_conference_mixer/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_conference_mixer)
set_target_properties(WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer PROPERTIES OUTPUT_NAME audio_conference_mixer)
target_link_libraries(WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer WebrtcModulesAudio_ProcessingAudio_Processing)
target_link_libraries(WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer WebrtcModulesUtilityUtility)
target_link_libraries(WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer


# Start of target WebrtcModulesRtp_RtcpRtp_Rtcp
set(WebrtcModulesRtp_RtcpRtp_Rtcp_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/dtmf_queue.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/flexfec_header_reader_writer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/flexfec_receiver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/flexfec_sender.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/forward_error_correction.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/forward_error_correction_internal.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/packet_loss_stats.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/playout_delay_oracle.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/receive_statistics_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/remote_ntp_time_estimator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/app.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/common_header.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/compound_packet.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/dlrr.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_jitter_report.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_reports.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/fir.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/nack.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/pli.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/psfb.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/rapid_resync_request.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/remb.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/report_block.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/rpsi.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/rrtr.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/rtpfb.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/sdes.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/sender_report.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/sli.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/target_bitrate.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmb_item.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbn.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbr.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_packet/voip_metric.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_receiver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_sender.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtcp_utility.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_format.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_format_h264.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_format_vp8.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_format_vp9.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_header_extension.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_header_extensions.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_header_parser.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_packet.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_packet_history.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_payload_registry.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_receiver_audio.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_receiver_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_receiver_strategy.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_receiver_video.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_sender.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_sender_audio.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_sender_video.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/rtp_utility.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/ssrc_database.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/time_util.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/tmmbr_help.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/ulpfec_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/ulpfec_header_reader_writer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/ulpfec_receiver_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/rtp_rtcp/source/vp8_partition_aggregator.cc)
add_library(WebrtcModulesRtp_RtcpRtp_Rtcp STATIC ${WebrtcModulesRtp_RtcpRtp_Rtcp_SOURCES})
target_compile_options(WebrtcModulesRtp_RtcpRtp_Rtcp PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesRtp_RtcpRtp_Rtcp PRIVATE BWE_TEST_LOGGING_COMPILE_TIME_ENABLE=0 V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesRtp_RtcpRtp_Rtcp PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesRtp_RtcpRtp_Rtcp PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesRtp_RtcpRtp_Rtcp PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/rtp_rtcp)
set_target_properties(WebrtcModulesRtp_RtcpRtp_Rtcp PROPERTIES OUTPUT_NAME rtp_rtcp)
target_link_libraries(WebrtcModulesRtp_RtcpRtp_Rtcp WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesRtp_RtcpRtp_Rtcp WebrtcCommon_VideoCommon_Video)
target_link_libraries(WebrtcModulesRtp_RtcpRtp_Rtcp WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator)
target_link_libraries(WebrtcModulesRtp_RtcpRtp_Rtcp WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesRtp_RtcpRtp_Rtcp PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesRtp_RtcpRtp_Rtcp ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcModulesRtp_RtcpRtp_Rtcp PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcModulesRtp_RtcpRtp_Rtcp ${COREVIDEO_LIBRARY})
# End of target WebrtcModulesRtp_RtcpRtp_Rtcp

# Start of target WebrtcCommon_AudioCommon_Audio
set(WebrtcCommon_AudioCommon_Audio_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/audio_converter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/audio_ring_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/audio_util.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/blocker.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/channel_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/fft4g.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/fir_filter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/lapped_transform.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/real_fourier.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/real_fourier_ooura.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/push_resampler.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/push_sinc_resampler.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/resampler.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/sinc_resampler.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/ring_buffer.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/auto_corr_to_refl_coef.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/auto_correlation.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/copy_set_operations.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/cross_correlation.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/division_operations.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/dot_product_with_scale.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/downsample_fast.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/energy.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/filter_ar.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/filter_ma_fast_q12.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/get_hanning_window.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/get_scaling_square.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/ilbc_specific_functions.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/levinson_durbin.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/lpc_to_refl_coef.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/min_max_operations.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/randomization_functions.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/real_fft.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/refl_coef_to_lpc.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/resample.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/resample_48khz.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/resample_by_2.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/resample_by_2_internal.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/resample_fractional.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/spl_init.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/spl_inl.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/spl_sqrt.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/splitting_filter.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/sqrt_of_one_minus_x_squared.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/vector_scaling_operations.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/sparse_fir_filter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/vad.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/vad_core.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/vad_filterbank.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/vad_gmm.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/vad_sp.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/webrtc_vad.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/wav_file.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/wav_header.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/window_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/complex_fft.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/complex_bit_reverse.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/filter_ar_fast_q12.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/spl_sqrt_floor.c)
add_library(WebrtcCommon_AudioCommon_Audio STATIC ${WebrtcCommon_AudioCommon_Audio_SOURCES})
target_compile_options(WebrtcCommon_AudioCommon_Audio PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcCommon_AudioCommon_Audio PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcCommon_AudioCommon_Audio PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcCommon_AudioCommon_Audio PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcCommon_AudioCommon_Audio PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/common_audio)
set_target_properties(WebrtcCommon_AudioCommon_Audio PROPERTIES OUTPUT_NAME common_audio)
target_link_libraries(WebrtcCommon_AudioCommon_Audio WebrtcCommon_AudioCommon_Audio_Sse2)
target_link_libraries(WebrtcCommon_AudioCommon_Audio WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcCommon_AudioCommon_Audio PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcCommon_AudioCommon_Audio ${FOUNDATION_LIBRARY})
# End of target WebrtcCommon_AudioCommon_Audio

# Start of target WebrtcModulesVideo_ProcessingVideo_Processing_Sse2
set(WebrtcModulesVideo_ProcessingVideo_Processing_Sse2_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_processing/util/denoiser_filter_sse2.cc)
add_library(WebrtcModulesVideo_ProcessingVideo_Processing_Sse2 STATIC ${WebrtcModulesVideo_ProcessingVideo_Processing_Sse2_SOURCES})
target_compile_options(WebrtcModulesVideo_ProcessingVideo_Processing_Sse2 PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-msse2" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesVideo_ProcessingVideo_Processing_Sse2 PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesVideo_ProcessingVideo_Processing_Sse2 PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesVideo_ProcessingVideo_Processing_Sse2 PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesVideo_ProcessingVideo_Processing_Sse2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/video_processing)
set_target_properties(WebrtcModulesVideo_ProcessingVideo_Processing_Sse2 PROPERTIES OUTPUT_NAME video_processing_sse2)
# End of target WebrtcModulesVideo_ProcessingVideo_Processing_Sse2

# Start of target WebrtcModulesAudio_CodingAudio_Decoder_Interface
set(WebrtcModulesAudio_CodingAudio_Decoder_Interface_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/audio_decoder.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/legacy_encoded_audio_frame.cc)
add_library(WebrtcModulesAudio_CodingAudio_Decoder_Interface STATIC ${WebrtcModulesAudio_CodingAudio_Decoder_Interface_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingAudio_Decoder_Interface PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingAudio_Decoder_Interface PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingAudio_Decoder_Interface PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingAudio_Decoder_Interface PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingAudio_Decoder_Interface PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingAudio_Decoder_Interface PROPERTIES OUTPUT_NAME audio_decoder_interface)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Decoder_Interface WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesAudio_CodingAudio_Decoder_Interface WebrtcBaseRtc_Base_Approved)
target_include_directories(WebrtcModulesAudio_CodingAudio_Decoder_Interface PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingAudio_Decoder_Interface ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingAudio_Decoder_Interface

# Start of target WebrtcLoggingRtc_Event_Log_Proto
set(WebrtcLoggingRtc_Event_Log_Proto_SOURCES ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/logging/rtc_event_log/rtc_event_log.pb.cc
    ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/logging/rtc_event_log/rtc_event_log_pb2.py)
add_library(WebrtcLoggingRtc_Event_Log_Proto STATIC ${WebrtcLoggingRtc_Event_Log_Proto_SOURCES})
target_compile_options(WebrtcLoggingRtc_Event_Log_Proto PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcLoggingRtc_Event_Log_Proto PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(WebrtcLoggingRtc_Event_Log_Proto PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(WebrtcLoggingRtc_Event_Log_Proto PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcLoggingRtc_Event_Log_Proto PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/logging)
set_target_properties(WebrtcLoggingRtc_Event_Log_Proto PROPERTIES OUTPUT_NAME rtc_event_log_proto)
target_link_libraries(WebrtcLoggingRtc_Event_Log_Proto Third_PartyProtobufProtobuf_Lite)
# End of target WebrtcLoggingRtc_Event_Log_Proto

# Start of target BuildWinDefault_Exe_Manifest
add_custom_target(BuildWinDefault_Exe_Manifest)
# End of target BuildWinDefault_Exe_Manifest

# Start of target WebrtcModulesVideo_CodingWebrtc_H264
set(WebrtcModulesVideo_CodingWebrtc_H264_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/codecs/h264/h264.cc)
add_library(WebrtcModulesVideo_CodingWebrtc_H264 STATIC ${WebrtcModulesVideo_CodingWebrtc_H264_SOURCES})
target_compile_options(WebrtcModulesVideo_CodingWebrtc_H264 PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesVideo_CodingWebrtc_H264 PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesVideo_CodingWebrtc_H264 PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesVideo_CodingWebrtc_H264 PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesVideo_CodingWebrtc_H264 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/video_coding)
set_target_properties(WebrtcModulesVideo_CodingWebrtc_H264 PROPERTIES OUTPUT_NAME webrtc_h264)
target_link_libraries(WebrtcModulesVideo_CodingWebrtc_H264 WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesVideo_CodingWebrtc_H264 PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CodingWebrtc_H264 ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesVideo_CodingWebrtc_H264

# Start of target WebrtcSdkRtc_Sdk_Framework_Objc+Link
add_custom_target(WebrtcSdkRtc_Sdk_Framework_Objc+Link)
add_dependencies(WebrtcSdkRtc_Sdk_Framework_Objc+Link WebrtcSdkRtc_Sdk_Framework_Objc)
# End of target WebrtcSdkRtc_Sdk_Framework_Objc+Link

# Start of target WebrtcModulesAudio_MixerAudio_Mixer
add_custom_target(WebrtcModulesAudio_MixerAudio_Mixer)
add_dependencies(WebrtcModulesAudio_MixerAudio_Mixer WebrtcModulesAudio_MixerAudio_Frame_Manipulator WebrtcModulesAudio_MixerAudio_Mixer_Impl)
# End of target WebrtcModulesAudio_MixerAudio_Mixer


# Start of target WebrtcBaseRtc_Base
set(WebrtcBaseRtc_Base_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/base/applefilesystem.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/asyncfile.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/asyncinvoker.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/asyncpacketsocket.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/asyncresolverinterface.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/asyncsocket.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/asynctcpsocket.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/asyncudpsocket.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/autodetectproxy.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/common.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/crc32.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/cryptstring.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/diskcache.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/filerotatingstream.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/fileutils.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/firewallsocketserver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/flags.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/helpers.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/httpbase.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/httpclient.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/httpcommon.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/httprequest.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/ipaddress.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/messagedigest.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/messagehandler.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/messagequeue.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/nethelpers.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/network.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/networkmonitor.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/nullsocketserver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/openssladapter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/openssldigest.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/opensslidentity.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/opensslstreamadapter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/pathutils.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/physicalsocketserver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/proxydetect.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/proxyinfo.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/ratelimiter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/rtccertificate.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/rtccertificategenerator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/sha1digest.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/sharedexclusivelock.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/signalthread.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/sigslot.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/socketadapters.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/socketaddress.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/socketaddresspair.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/socketpool.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/socketstream.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/ssladapter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/sslfingerprint.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/sslidentity.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/sslsocketfactory.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/sslstreamadapter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/stream.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/task.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/taskparent.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/taskrunner.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/thread.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/urlencode.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/httpserver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/json.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/logsinks.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/natserver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/natsocketfactory.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/nattypes.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/optionsfile.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/proxyserver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/testclient.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/transformadapter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/virtualsocketserver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/latebindingsymboltable.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/macwindowpicker.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/maccocoathreadhelper.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/macconversion.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/macifaddrs_converter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/scoped_autorelease_pool.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/macutils.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/ifaddrs_converter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/unixfilesystem.cc
    $<TARGET_OBJECTS:Third_PartyJsoncppJsoncpp>)
add_library(WebrtcBaseRtc_Base STATIC ${WebrtcBaseRtc_Base_SOURCES})
target_compile_options(WebrtcBaseRtc_Base PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wno-uninitialized" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wextra" "-Wno-unused-parameter" "-Wno-missing-field-initializers" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-Werror" "-Wall" "-Wno-unused-variable" "-Wpartial-availability" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcBaseRtc_Base PRIVATE _DEBUG V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 SSL_USE_OPENSSL HAVE_OPENSSL_SSL_H FEATURE_ENABLE_SSL WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcBaseRtc_Base PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/jsoncpp/source/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/jsoncpp/overrides/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/include/)
set_target_properties(WebrtcBaseRtc_Base PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcBaseRtc_Base PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/base)
set_target_properties(WebrtcBaseRtc_Base PROPERTIES OUTPUT_NAME rtc_base)
target_link_libraries(WebrtcBaseRtc_Base Third_PartyBoringsslBoringssl)
target_link_libraries(WebrtcBaseRtc_Base WebrtcWebrtc_Common)
target_link_libraries(WebrtcBaseRtc_Base WebrtcBaseRtc_Base_Approved)
target_include_directories(WebrtcBaseRtc_Base PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcBaseRtc_Base ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcBaseRtc_Base PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcBaseRtc_Base ${COCOA_LIBRARY})
target_include_directories(WebrtcBaseRtc_Base PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcBaseRtc_Base ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcBaseRtc_Base PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcBaseRtc_Base ${IOKIT_LIBRARY})
target_include_directories(WebrtcBaseRtc_Base PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcBaseRtc_Base ${SECURITY_LIBRARY})
target_include_directories(WebrtcBaseRtc_Base PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcBaseRtc_Base ${SYSTEMCONFIGURATION_LIBRARY})
# End of target WebrtcBaseRtc_Base

# Start of target WebrtcModulesDesktop_CapturePrimitives
set(WebrtcModulesDesktop_CapturePrimitives_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/desktop_frame.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/desktop_geometry.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/desktop_region.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/desktop_capture/shared_desktop_frame.cc)
add_library(WebrtcModulesDesktop_CapturePrimitives STATIC ${WebrtcModulesDesktop_CapturePrimitives_SOURCES})
target_compile_options(WebrtcModulesDesktop_CapturePrimitives PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesDesktop_CapturePrimitives PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC SSL_USE_OPENSSL HAVE_OPENSSL_SSL_H FEATURE_ENABLE_SSL)
target_include_directories(WebrtcModulesDesktop_CapturePrimitives PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesDesktop_CapturePrimitives PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesDesktop_CapturePrimitives PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/desktop_capture)
set_target_properties(WebrtcModulesDesktop_CapturePrimitives PROPERTIES OUTPUT_NAME primitives)
target_link_libraries(WebrtcModulesDesktop_CapturePrimitives WebrtcBaseRtc_Base)
target_include_directories(WebrtcModulesDesktop_CapturePrimitives PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CapturePrimitives ${COCOA_LIBRARY})
target_include_directories(WebrtcModulesDesktop_CapturePrimitives PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CapturePrimitives ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcModulesDesktop_CapturePrimitives PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CapturePrimitives ${IOKIT_LIBRARY})
target_include_directories(WebrtcModulesDesktop_CapturePrimitives PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CapturePrimitives ${SECURITY_LIBRARY})
target_include_directories(WebrtcModulesDesktop_CapturePrimitives PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CapturePrimitives ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcModulesDesktop_CapturePrimitives PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcModulesDesktop_CapturePrimitives ${APPLICATIONSERVICES_LIBRARY})
# End of target WebrtcModulesDesktop_CapturePrimitives

# Start of target WebrtcBaseGtest_Prod
add_library(WebrtcBaseGtest_Prod OBJECT)
target_compile_options(WebrtcBaseGtest_Prod PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcBaseGtest_Prod PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcBaseGtest_Prod PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcBaseGtest_Prod PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
# End of target WebrtcBaseGtest_Prod

# Start of target WebrtcLoggingRtc_Event_Log_Api
add_library(WebrtcLoggingRtc_Event_Log_Api OBJECT)
target_compile_options(WebrtcLoggingRtc_Event_Log_Api PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcLoggingRtc_Event_Log_Api PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcLoggingRtc_Event_Log_Api PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcLoggingRtc_Event_Log_Api PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
# End of target WebrtcLoggingRtc_Event_Log_Api

# Start of target WebrtcSystem_WrappersSystem_Wrappers_Default
add_custom_target(WebrtcSystem_WrappersSystem_Wrappers_Default)
add_dependencies(WebrtcSystem_WrappersSystem_Wrappers_Default WebrtcSystem_WrappersField_Trial_Default WebrtcSystem_WrappersMetrics_Default WebrtcSystem_WrappersSystem_Wrappers)
# End of target WebrtcSystem_WrappersSystem_Wrappers_Default


# Start of target WebrtcLibjingleLibjingle
add_custom_target(WebrtcLibjingleLibjingle)
add_dependencies(WebrtcLibjingleLibjingle WebrtcLibjingleXmlliteXmllite WebrtcLibjingleXmppXmpp)
# End of target WebrtcLibjingleLibjingle



# Start of target WebrtcStatsStats
add_custom_target(WebrtcStatsStats)
add_dependencies(WebrtcStatsStats WebrtcStatsRtc_Stats)
# End of target WebrtcStatsStats





# Start of target Third_PartyOpusOpus_Demo
set(Third_PartyOpusOpus_Demo_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/opus_demo.c)
add_executable(Third_PartyOpusOpus_Demo ${Third_PartyOpusOpus_Demo_SOURCES})
target_compile_options(Third_PartyOpusOpus_Demo PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Werror" "-Wall" "-Wno-unused-variable" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-Wno-absolute-value" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyOpusOpus_Demo PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1)
target_include_directories(Third_PartyOpusOpus_Demo PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/)
set_target_properties(Third_PartyOpusOpus_Demo PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9 -Wl,-ObjC -Wl,-pie")
set_target_properties(Third_PartyOpusOpus_Demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/)
set_target_properties(Third_PartyOpusOpus_Demo PROPERTIES OUTPUT_NAME opus_demo)
target_link_libraries(Third_PartyOpusOpus_Demo Third_PartyOpusOpus)
add_dependencies(Third_PartyOpusOpus_Demo BuildConfigSanitizersDeps BuildWinDefault_Exe_Manifest Third_PartyOpusOpus)
# End of target Third_PartyOpusOpus_Demo

# Start of target WebrtcApiCall_Api
set(WebrtcApiCall_Api_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/api/call/audio_send_stream.cc
    $<TARGET_OBJECTS:WebrtcApiAudio_Mixer_Api>)
add_library(WebrtcApiCall_Api STATIC ${WebrtcApiCall_Api_SOURCES})
target_compile_options(WebrtcApiCall_Api PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcApiCall_Api PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcApiCall_Api PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcApiCall_Api PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
target_link_libraries(WebrtcApiCall_Api WebrtcWebrtc_Common)
target_link_libraries(WebrtcApiCall_Api WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcApiCall_Api WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcApiCall_Api WebrtcModulesAudio_CodingAudio_Encoder_Interface)
target_include_directories(WebrtcApiCall_Api PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcApiCall_Api ${FOUNDATION_LIBRARY})
# End of target WebrtcApiCall_Api





# Start of target WebrtcWebrtc
set(WebrtcWebrtc_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/build/no_op_function.cc)
add_library(WebrtcWebrtc STATIC ${WebrtcWebrtc_SOURCES})
target_compile_options(WebrtcWebrtc PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcWebrtc PRIVATE ENABLE_RTC_EVENT_LOG V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC SSL_USE_OPENSSL HAVE_OPENSSL_SSL_H FEATURE_ENABLE_SSL XML_STATIC FEATURE_ENABLE_SSL FEATURE_ENABLE_VOICEMAIL GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD HAVE_WEBRTC_VIDEO HAVE_WEBRTC_VOICE FEATURE_ENABLE_VOICEMAIL HAVE_SCTP HAVE_SRTP)
target_include_directories(WebrtcWebrtc PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/expat/files/lib/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/dummy/ ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/ ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Headers/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_conference_mixer/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcWebrtc PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcWebrtc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc)
set_target_properties(WebrtcWebrtc PROPERTIES OUTPUT_NAME webrtc)
target_link_libraries(WebrtcWebrtc WebrtcWebrtc_Common)
target_link_libraries(WebrtcWebrtc WebrtcApiLibjingle_Peerconnection)
target_link_libraries(WebrtcWebrtc WebrtcAudioAudio)
target_link_libraries(WebrtcWebrtc WebrtcBaseRtc_Base)
target_link_libraries(WebrtcWebrtc WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcWebrtc WebrtcBaseRtc_Task_Queue)
target_link_libraries(WebrtcWebrtc WebrtcCallCall)
target_link_libraries(WebrtcWebrtc WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcWebrtc WebrtcCommon_VideoCommon_Video)
target_link_libraries(WebrtcWebrtc WebrtcLibjingleXmlliteRtc_Xmllite)
target_link_libraries(WebrtcWebrtc WebrtcLibjingleXmppRtc_Xmpp)
target_link_libraries(WebrtcWebrtc WebrtcLoggingRtc_Event_Log_Impl)
target_link_libraries(WebrtcWebrtc WebrtcLoggingRtc_Event_Log_Parser)
target_link_libraries(WebrtcWebrtc WebrtcLoggingRtc_Event_Log_Proto)
target_link_libraries(WebrtcWebrtc WebrtcMediaRtc_Media)
target_link_libraries(WebrtcWebrtc WebrtcModulesAudio_CodingAudio_Coding)
target_link_libraries(WebrtcWebrtc WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer)
target_link_libraries(WebrtcWebrtc WebrtcModulesAudio_DeviceAudio_Device)
target_link_libraries(WebrtcWebrtc WebrtcModulesAudio_MixerAudio_Frame_Manipulator)
target_link_libraries(WebrtcWebrtc WebrtcModulesAudio_MixerAudio_Mixer_Impl)
target_link_libraries(WebrtcWebrtc WebrtcModulesAudio_ProcessingAudio_Processing)
target_link_libraries(WebrtcWebrtc WebrtcModulesBitrate_ControllerBitrate_Controller)
target_link_libraries(WebrtcWebrtc WebrtcModulesCongestion_ControllerCongestion_Controller)
target_link_libraries(WebrtcWebrtc WebrtcModulesDesktop_CaptureDesktop_Capture)
target_link_libraries(WebrtcWebrtc WebrtcModulesMedia_FileMedia_File)
target_link_libraries(WebrtcWebrtc WebrtcModulesPacingPacing)
target_link_libraries(WebrtcWebrtc WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator)
target_link_libraries(WebrtcWebrtc WebrtcModulesRtp_RtcpRtp_Rtcp)
target_link_libraries(WebrtcWebrtc WebrtcModulesUtilityUtility)
target_link_libraries(WebrtcWebrtc WebrtcModulesVideo_CaptureVideo_Capture)
target_link_libraries(WebrtcWebrtc WebrtcModulesVideo_CodingVideo_Coding)
target_link_libraries(WebrtcWebrtc WebrtcModulesVideo_ProcessingVideo_Processing)
target_link_libraries(WebrtcWebrtc WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl)
target_link_libraries(WebrtcWebrtc WebrtcP2PLibstunprober)
target_link_libraries(WebrtcWebrtc WebrtcP2PRtc_P2P)
target_link_libraries(WebrtcWebrtc WebrtcPcRtc_Pc)
target_link_libraries(WebrtcWebrtc WebrtcStatsRtc_Stats)
target_link_libraries(WebrtcWebrtc WebrtcSystem_WrappersSystem_Wrappers)
target_link_libraries(WebrtcWebrtc WebrtcVideoVideo)
target_link_libraries(WebrtcWebrtc WebrtcVoice_EngineVoice_Engine)
target_include_directories(WebrtcWebrtc PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcWebrtc PRIVATE ${AVFOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${AVFOUNDATION_LIBRARY})
target_include_directories(WebrtcWebrtc PRIVATE ${COREMEDIA_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${COREMEDIA_LIBRARY})
target_include_directories(WebrtcWebrtc PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${COREVIDEO_LIBRARY})
target_include_directories(WebrtcWebrtc PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${COCOA_LIBRARY})
target_include_directories(WebrtcWebrtc PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${IOKIT_LIBRARY})
target_include_directories(WebrtcWebrtc PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${SECURITY_LIBRARY})
target_include_directories(WebrtcWebrtc PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcWebrtc PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcWebrtc PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcWebrtc PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${COREAUDIO_LIBRARY})
target_include_directories(WebrtcWebrtc PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${COREGRAPHICS_LIBRARY})
target_include_directories(WebrtcWebrtc PRIVATE ${APPKIT_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${APPKIT_LIBRARY})
target_include_directories(WebrtcWebrtc PRIVATE ${OPENGL_INCLUDE_DIR})
target_link_libraries(WebrtcWebrtc ${OPENGL_LIBRARY})
# End of target WebrtcWebrtc


# Start of target WebrtcBaseRtc_Base_Approved
set(WebrtcBaseRtc_Base_Approved_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/base/base64.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/bitbuffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/bufferqueue.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/bytebuffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/checks.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/copyonwritebuffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/criticalsection.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/event.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/event_tracer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/exp_filter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/file.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/location.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/md5.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/md5digest.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/optional.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/platform_file.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/platform_thread.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/race_checker.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/random.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/rate_limiter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/rate_statistics.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/ratetracker.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/stringencode.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/stringutils.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/systeminfo.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/thread_checker_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/timestampaligner.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/timeutils.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/file_posix.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/logging.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/base/logging_mac.mm)
add_library(WebrtcBaseRtc_Base_Approved STATIC ${WebrtcBaseRtc_Base_Approved_SOURCES})
target_compile_options(WebrtcBaseRtc_Base_Approved PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcBaseRtc_Base_Approved PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcBaseRtc_Base_Approved PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcBaseRtc_Base_Approved PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcBaseRtc_Base_Approved PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/base)
set_target_properties(WebrtcBaseRtc_Base_Approved PROPERTIES OUTPUT_NAME rtc_base_approved)
target_include_directories(WebrtcBaseRtc_Base_Approved PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcBaseRtc_Base_Approved ${FOUNDATION_LIBRARY})
# End of target WebrtcBaseRtc_Base_Approved

# Start of target WebrtcModulesBitrate_ControllerBitrate_Controller
set(WebrtcModulesBitrate_ControllerBitrate_Controller_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/bitrate_controller/bitrate_controller_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/bitrate_controller/send_side_bandwidth_estimation.cc)
add_library(WebrtcModulesBitrate_ControllerBitrate_Controller STATIC ${WebrtcModulesBitrate_ControllerBitrate_Controller_SOURCES})
target_compile_options(WebrtcModulesBitrate_ControllerBitrate_Controller PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesBitrate_ControllerBitrate_Controller PRIVATE BWE_TEST_LOGGING_COMPILE_TIME_ENABLE=0 V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesBitrate_ControllerBitrate_Controller PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesBitrate_ControllerBitrate_Controller PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesBitrate_ControllerBitrate_Controller PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/bitrate_controller)
set_target_properties(WebrtcModulesBitrate_ControllerBitrate_Controller PROPERTIES OUTPUT_NAME bitrate_controller)
target_link_libraries(WebrtcModulesBitrate_ControllerBitrate_Controller WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesBitrate_ControllerBitrate_Controller PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesBitrate_ControllerBitrate_Controller ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesBitrate_ControllerBitrate_Controller


# Start of target WebrtcStatsRtc_Stats_Test_Utils
set(WebrtcStatsRtc_Stats_Test_Utils_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/stats/test/rtcteststats.cc)
add_library(WebrtcStatsRtc_Stats_Test_Utils OBJECT ${WebrtcStatsRtc_Stats_Test_Utils_SOURCES})
target_compile_options(WebrtcStatsRtc_Stats_Test_Utils PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcStatsRtc_Stats_Test_Utils PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcStatsRtc_Stats_Test_Utils PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcStatsRtc_Stats_Test_Utils PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
# End of target WebrtcStatsRtc_Stats_Test_Utils


# Start of target WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto
set(WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto_SOURCES ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing/unittest.pb.cc
    ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_processing/unittest_pb2.py)
add_library(WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto STATIC ${WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto_SOURCES})
target_compile_options(WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_processing)
set_target_properties(WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto PROPERTIES OUTPUT_NAME audioproc_unittest_proto)
target_link_libraries(WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto Third_PartyProtobufProtobuf_Lite)
# End of target WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto




# Start of target WebrtcModulesAudio_CodingIlbc
set(WebrtcModulesAudio_CodingIlbc_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/abs_quant.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/abs_quant_loop.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/audio_decoder_ilbc.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/audio_encoder_ilbc.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/augmented_cb_corr.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/bw_expand.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/cb_construct.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/cb_mem_energy.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/cb_mem_energy_augmentation.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/cb_mem_energy_calc.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/cb_search.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/cb_search_core.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/cb_update_best_index.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/chebyshev.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/comp_corr.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/constants.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/create_augmented_vec.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/decode.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/decode_residual.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/decoder_interpolate_lsf.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/do_plc.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/encode.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/energy_inverse.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/enh_upsample.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/enhancer.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/enhancer_interface.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/filtered_cb_vecs.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/frame_classify.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/gain_dequant.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/gain_quant.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/get_cd_vec.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/get_lsp_poly.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/get_sync_seq.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/hp_input.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/hp_output.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/ilbc.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/index_conv_dec.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/index_conv_enc.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/init_decode.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/init_encode.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/interpolate.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/interpolate_samples.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/lpc_encode.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/lsf_check.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/lsf_interpolate_to_poly_dec.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/lsf_interpolate_to_poly_enc.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/lsf_to_lsp.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/lsf_to_poly.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/lsp_to_lsf.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/my_corr.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/nearest_neighbor.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/pack_bits.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/poly_to_lsf.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/poly_to_lsp.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/refiner.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/simple_interpolate_lsf.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/simple_lpc_analysis.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/simple_lsf_dequant.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/simple_lsf_quant.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/smooth.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/smooth_out_data.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/sort_sq.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/split_vq.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/state_construct.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/state_search.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/swap_bytes.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/unpack_bits.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/vq3.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/vq4.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/window32_w32.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/xcorr_coef.c)
add_library(WebrtcModulesAudio_CodingIlbc STATIC ${WebrtcModulesAudio_CodingIlbc_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingIlbc PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingIlbc PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingIlbc PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingIlbc PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingIlbc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingIlbc PROPERTIES OUTPUT_NAME ilbc)
target_link_libraries(WebrtcModulesAudio_CodingIlbc WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcModulesAudio_CodingIlbc WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcModulesAudio_CodingIlbc WebrtcModulesAudio_CodingAudio_Decoder_Interface)
target_link_libraries(WebrtcModulesAudio_CodingIlbc WebrtcModulesAudio_CodingAudio_Encoder_Interface)
target_include_directories(WebrtcModulesAudio_CodingIlbc PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingIlbc ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingIlbc





# Start of target WebrtcToolsRtp_Analyzer
list(APPEND WebrtcToolsRtp_Analyzer ${LIBWEBRTC_OUTPUT_DIR}/misc.py ${LIBWEBRTC_OUTPUT_DIR}/pb_parse.py ${LIBWEBRTC_OUTPUT_DIR}/rtp_analyzer.py ${LIBWEBRTC_OUTPUT_DIR}/rtp_analyzer.sh)
file(COPY ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/py_event_log_analyzer/misc.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/misc.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/py_event_log_analyzer/pb_parse.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pb_parse.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/py_event_log_analyzer/rtp_analyzer.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/rtp_analyzer.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/py_event_log_analyzer/rtp_analyzer.sh DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/rtp_analyzer.sh)
add_custom_target(WebrtcToolsRtp_Analyzer
    COMMAND echo "Generating copy targetWebrtcToolsRtp_Analyzer"
    VERBATIM)
add_dependencies(WebrtcToolsRtp_Analyzer WebrtcLoggingRtc_Event_Log_Proto)
# End of target WebrtcToolsRtp_Analyzer

# Start of target WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto
set(WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto_SOURCES ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing/debug.pb.cc
    ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_processing/debug_pb2.py)
add_library(WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto STATIC ${WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto_SOURCES})
target_compile_options(WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_processing)
set_target_properties(WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto PROPERTIES OUTPUT_NAME audioproc_debug_proto)
target_link_libraries(WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto Third_PartyProtobufProtobuf_Lite)
# End of target WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto

# Start of target Third_PartyProtobufCopy_Google_Protobuf
list(APPEND Third_PartyProtobufCopy_Google_Protobuf ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/__init__.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/descriptor.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/descriptor_database.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/descriptor_pool.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/json_format.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/message.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/message_factory.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/proto_builder.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/reflection.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/service.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/service_reflection.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/symbol_database.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/text_encoding.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/text_format.py ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/descriptor_pb2.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/__init__.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/__init__.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/descriptor.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/descriptor.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/descriptor_database.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/descriptor_database.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/descriptor_pool.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/descriptor_pool.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/json_format.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/json_format.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/message.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/message.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/message_factory.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/message_factory.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/proto_builder.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/proto_builder.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/reflection.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/reflection.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/service.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/service.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/service_reflection.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/service_reflection.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/symbol_database.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/symbol_database.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/text_encoding.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/text_encoding.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/text_format.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/text_format.py)
file(COPY ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/python/google/protobuf/descriptor_pb2.py DESTINATION ${LIBWEBRTC_OUTPUT_DIR}/pyproto/google/protobuf/descriptor_pb2.py)
add_custom_target(Third_PartyProtobufCopy_Google_Protobuf
    COMMAND echo "Generating copy targetThird_PartyProtobufCopy_Google_Protobuf"
    VERBATIM)
# End of target Third_PartyProtobufCopy_Google_Protobuf


# Start of target WebrtcModulesAudio_CodingNeteq
set(WebrtcModulesAudio_CodingNeteq_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/accelerate.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/audio_classifier.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/audio_decoder_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/audio_multi_vector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/audio_vector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/background_noise.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/buffer_level_filter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/comfort_noise.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/cross_correlation.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/decision_logic.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/decision_logic_fax.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/decision_logic_normal.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/decoder_database.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/delay_manager.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/delay_peak_detector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/dsp_helper.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/dtmf_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/dtmf_tone_generator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/expand.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/merge.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/nack_tracker.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/neteq.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/neteq_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/normal.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/packet.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/packet_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/post_decode_vad.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/preemptive_expand.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/random_vector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/red_payload_splitter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/rtcp.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/statistics_calculator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/sync_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/tick_timer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/time_stretch.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/neteq/timestamp_scaler.cc)
add_library(WebrtcModulesAudio_CodingNeteq STATIC ${WebrtcModulesAudio_CodingNeteq_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingNeteq PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingNeteq PRIVATE WEBRTC_CODEC_ILBC WEBRTC_CODEC_OPUS WEBRTC_CODEC_ISAC V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingNeteq PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/pcm16b/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/cng/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingNeteq PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingNeteq PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingNeteq PROPERTIES OUTPUT_NAME neteq)
target_link_libraries(WebrtcModulesAudio_CodingNeteq WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesAudio_CodingNeteq WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcModulesAudio_CodingNeteq WebrtcModulesAudio_CodingAudio_Decoder_Interface)
target_link_libraries(WebrtcModulesAudio_CodingNeteq WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory)
target_link_libraries(WebrtcModulesAudio_CodingNeteq WebrtcModulesAudio_CodingCng)
target_link_libraries(WebrtcModulesAudio_CodingNeteq WebrtcModulesAudio_CodingIlbc)
target_link_libraries(WebrtcModulesAudio_CodingNeteq WebrtcModulesAudio_CodingIsac)
target_link_libraries(WebrtcModulesAudio_CodingNeteq WebrtcModulesAudio_CodingPcm16B)
target_link_libraries(WebrtcModulesAudio_CodingNeteq WebrtcModulesAudio_CodingRent_A_Codec)
target_link_libraries(WebrtcModulesAudio_CodingNeteq WebrtcModulesAudio_CodingWebrtc_Opus)
target_link_libraries(WebrtcModulesAudio_CodingNeteq WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesAudio_CodingNeteq PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingNeteq ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingNeteq

# Start of target WebrtcLoggingRtc_Event_Log_Impl
set(WebrtcLoggingRtc_Event_Log_Impl_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/logging/rtc_event_log/rtc_event_log.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/logging/rtc_event_log/rtc_event_log_helper_thread.cc
    $<TARGET_OBJECTS:WebrtcLoggingRtc_Event_Log_Api>)
add_library(WebrtcLoggingRtc_Event_Log_Impl STATIC ${WebrtcLoggingRtc_Event_Log_Impl_SOURCES})
target_compile_options(WebrtcLoggingRtc_Event_Log_Impl PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcLoggingRtc_Event_Log_Impl PRIVATE ENABLE_RTC_EVENT_LOG V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(WebrtcLoggingRtc_Event_Log_Impl PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/protoc_out/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(WebrtcLoggingRtc_Event_Log_Impl PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcLoggingRtc_Event_Log_Impl PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/logging)
set_target_properties(WebrtcLoggingRtc_Event_Log_Impl PROPERTIES OUTPUT_NAME rtc_event_log_impl)
target_link_libraries(WebrtcLoggingRtc_Event_Log_Impl WebrtcWebrtc_Common)
target_link_libraries(WebrtcLoggingRtc_Event_Log_Impl WebrtcApiCall_Api)
target_link_libraries(WebrtcLoggingRtc_Event_Log_Impl WebrtcLoggingRtc_Event_Log_Proto)
target_link_libraries(WebrtcLoggingRtc_Event_Log_Impl WebrtcModulesRtp_RtcpRtp_Rtcp)
target_include_directories(WebrtcLoggingRtc_Event_Log_Impl PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcLoggingRtc_Event_Log_Impl ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcLoggingRtc_Event_Log_Impl PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcLoggingRtc_Event_Log_Impl ${COREVIDEO_LIBRARY})
# End of target WebrtcLoggingRtc_Event_Log_Impl

# Start of target WebrtcModulesAudio_CodingRed
set(WebrtcModulesAudio_CodingRed_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/red/audio_encoder_copy_red.cc)
add_library(WebrtcModulesAudio_CodingRed STATIC ${WebrtcModulesAudio_CodingRed_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingRed PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingRed PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingRed PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/red/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingRed PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingRed PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingRed PROPERTIES OUTPUT_NAME red)
target_link_libraries(WebrtcModulesAudio_CodingRed WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcModulesAudio_CodingRed WebrtcModulesAudio_CodingAudio_Encoder_Interface)
target_include_directories(WebrtcModulesAudio_CodingRed PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingRed ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingRed

# Start of target WebrtcToolsFrame_Analyzer
set(WebrtcToolsFrame_Analyzer_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/frame_analyzer/frame_analyzer.cc)
add_executable(WebrtcToolsFrame_Analyzer ${WebrtcToolsFrame_Analyzer_SOURCES})
target_compile_options(WebrtcToolsFrame_Analyzer PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcToolsFrame_Analyzer PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcToolsFrame_Analyzer PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcToolsFrame_Analyzer PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9 -Wl,-ObjC -Wl,-pie")
set_target_properties(WebrtcToolsFrame_Analyzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/)
set_target_properties(WebrtcToolsFrame_Analyzer PROPERTIES OUTPUT_NAME frame_analyzer)
target_link_libraries(WebrtcToolsFrame_Analyzer WebrtcToolsCommand_Line_Parser)
target_link_libraries(WebrtcToolsFrame_Analyzer WebrtcToolsVideo_Quality_Analysis)
target_include_directories(WebrtcToolsFrame_Analyzer PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcToolsFrame_Analyzer ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcToolsFrame_Analyzer PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcToolsFrame_Analyzer ${COREVIDEO_LIBRARY})
add_dependencies(WebrtcToolsFrame_Analyzer BuildConfigSanitizersDeps BuildWinDefault_Exe_Manifest WebrtcToolsCommand_Line_Parser WebrtcToolsVideo_Quality_Analysis)
# End of target WebrtcToolsFrame_Analyzer

# Start of target WebrtcModulesAudio_DeviceAudio_Device
set(WebrtcModulesAudio_DeviceAudio_Device_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/audio_device_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/audio_device_generic.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/dummy/audio_device_dummy.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/dummy/file_audio_device.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/fine_audio_buffer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/audio_device_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/mac/audio_device_mac.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/mac/audio_mixer_manager_mac.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/mac/portaudio/pa_ringbuffer.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/dummy/file_audio_device_factory.cc)
add_library(WebrtcModulesAudio_DeviceAudio_Device STATIC ${WebrtcModulesAudio_DeviceAudio_Device_SOURCES})
target_compile_options(WebrtcModulesAudio_DeviceAudio_Device PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_DeviceAudio_Device PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_DeviceAudio_Device PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/dummy/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/mac/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_device/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_DeviceAudio_Device PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_DeviceAudio_Device PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_device)
set_target_properties(WebrtcModulesAudio_DeviceAudio_Device PROPERTIES OUTPUT_NAME audio_device)
target_link_libraries(WebrtcModulesAudio_DeviceAudio_Device WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesAudio_DeviceAudio_Device WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcModulesAudio_DeviceAudio_Device WebrtcBaseRtc_Task_Queue)
target_link_libraries(WebrtcModulesAudio_DeviceAudio_Device WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcModulesAudio_DeviceAudio_Device WebrtcModulesUtilityUtility)
target_link_libraries(WebrtcModulesAudio_DeviceAudio_Device WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesAudio_DeviceAudio_Device PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_DeviceAudio_Device ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcModulesAudio_DeviceAudio_Device PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_DeviceAudio_Device ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcModulesAudio_DeviceAudio_Device PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_DeviceAudio_Device ${COREAUDIO_LIBRARY})
target_include_directories(WebrtcModulesAudio_DeviceAudio_Device PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_DeviceAudio_Device ${COREGRAPHICS_LIBRARY})
target_include_directories(WebrtcModulesAudio_DeviceAudio_Device PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_DeviceAudio_Device ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_DeviceAudio_Device

# Start of target WebrtcModulesAudio_CodingAna_Config_Proto_Gen
add_custom_command(OUTPUT ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor/config.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor/config.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_coding/audio_network_adaptor/config_pb2.py
    DEPENDS Third_PartyProtobufProtoc
    COMMAND python ${LIBWEBRTC_INPUT_DIR}/tools/protoc_wrapper/protoc_wrapper.py config.proto --protoc ./protoc --proto-in-dir ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/audio_network_adaptor --cc-out-dir ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor --py-out-dir pyproto/webrtc/modules/audio_coding/audio_network_adaptor
    VERBATIM)
add_custom_target(WebrtcModulesAudio_CodingAna_Config_Proto_Gen DEPENDS Third_PartyProtobufProtoc ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor/config.pb.h ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor/config.pb.cc ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_coding/audio_network_adaptor/config_pb2.py)
# End of target WebrtcModulesAudio_CodingAna_Config_Proto_Gen



# Start of target WebrtcModulesModules
add_custom_target(WebrtcModulesModules)
add_dependencies(WebrtcModulesModules WebrtcModulesAudio_CodingAudio_Coding WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer WebrtcModulesAudio_DeviceAudio_Device WebrtcModulesAudio_MixerAudio_Mixer WebrtcModulesAudio_ProcessingAudio_Processing WebrtcModulesBitrate_ControllerBitrate_Controller WebrtcModulesCongestion_ControllerCongestion_Controller WebrtcModulesDesktop_CaptureDesktop_Capture WebrtcModulesMedia_FileMedia_File WebrtcModulesPacingPacing WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator WebrtcModulesRtp_RtcpRtp_Rtcp WebrtcModulesUtilityUtility WebrtcModulesVideo_CaptureVideo_Capture WebrtcModulesVideo_CodingVideo_Coding WebrtcModulesVideo_ProcessingVideo_Processing)
# End of target WebrtcModulesModules

# Start of target Third_PartyProtobufProtobuf_Lite
set(Third_PartyProtobufProtobuf_Lite_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/arena.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/arenastring.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/extension_set.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/generated_message_util.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/io/coded_stream.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/io/zero_copy_stream.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/message_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/repeated_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/atomicops_internals_x86_gcc.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/atomicops_internals_x86_msvc.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/bytestream.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/common.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/int128.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/once.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/status.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/statusor.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/stringpiece.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/stringprintf.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/structurally_valid.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/strutil.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/time.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/wire_format_lite.cc)
add_library(Third_PartyProtobufProtobuf_Lite STATIC ${Third_PartyProtobufProtobuf_Lite_SOURCES})
target_compile_options(Third_PartyProtobufProtobuf_Lite PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Werror" "-Wall" "-Wno-unused-variable" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-Wno-unused-function" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyProtobufProtobuf_Lite PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(Third_PartyProtobufProtobuf_Lite PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(Third_PartyProtobufProtobuf_Lite PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(Third_PartyProtobufProtobuf_Lite PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/third_party/protobuf)
set_target_properties(Third_PartyProtobufProtobuf_Lite PROPERTIES OUTPUT_NAME protobuf_lite)
# End of target Third_PartyProtobufProtobuf_Lite

# Start of target WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory
set(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/builtin_audio_decoder_factory.cc
    $<TARGET_OBJECTS:WebrtcModulesAudio_CodingAudio_Decoder_Factory_Interface>)
add_library(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory STATIC ${WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory PRIVATE WEBRTC_CODEC_ILBC WEBRTC_CODEC_OPUS WEBRTC_CODEC_ISAC WEBRTC_CODEC_RED V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/red/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/pcm16b/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/cng/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/ilbc/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory PROPERTIES OUTPUT_NAME builtin_audio_decoder_factory)
target_link_libraries(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory WebrtcModulesAudio_CodingAudio_Decoder_Interface)
target_link_libraries(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory WebrtcModulesAudio_CodingAudio_Format)
target_link_libraries(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory WebrtcModulesAudio_CodingAudio_Format_Conversion)
target_link_libraries(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory WebrtcModulesAudio_CodingCng)
target_link_libraries(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory WebrtcModulesAudio_CodingIlbc)
target_link_libraries(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory WebrtcModulesAudio_CodingIsac)
target_link_libraries(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory WebrtcModulesAudio_CodingPcm16B)
target_link_libraries(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory WebrtcModulesAudio_CodingRed)
target_link_libraries(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory WebrtcModulesAudio_CodingWebrtc_Opus)
target_include_directories(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory

# Start of target Third_PartyBoringsslBoringssl
set(Third_PartyBoringsslBoringssl_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/err_data.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/aes/aes.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/aes/key_wrap.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/aes/mode_wrappers.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_bitstr.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_bool.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_d2i_fp.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_dup.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_enum.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_gentm.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_i2d_fp.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_int.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_mbstr.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_object.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_octet.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_print.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_strnid.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_time.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_type.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_utctm.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/a_utf8.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/asn1_lib.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/asn1_par.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/asn_pack.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/f_enum.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/f_int.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/f_string.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/t_bitst.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/tasn_dec.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/tasn_enc.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/tasn_fre.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/tasn_new.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/tasn_typ.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/tasn_utl.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/x_bignum.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/asn1/x_long.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/base64/base64.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bio/bio.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bio/bio_mem.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bio/buffer.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bio/connect.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bio/fd.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bio/file.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bio/hexdump.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bio/pair.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bio/printf.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bio/socket.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bio/socket_helper.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/add.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/asm/x86_64-gcc.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/bn.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/bn_asn1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/cmp.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/convert.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/ctx.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/div.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/exponentiation.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/gcd.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/generic.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/kronecker.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/montgomery.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/montgomery_inv.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/mul.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/prime.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/random.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/rsaz_exp.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/shift.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bn/sqrt.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/buf/buf.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bytestring/asn1_compat.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bytestring/ber.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bytestring/cbb.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/bytestring/cbs.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/chacha/chacha.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cipher/aead.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cipher/cipher.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cipher/derive_key.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cipher/e_aes.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cipher/e_chacha20poly1305.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cipher/e_des.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cipher/e_null.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cipher/e_rc2.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cipher/e_rc4.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cipher/e_ssl3.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cipher/e_tls.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cipher/tls_cbc.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cmac/cmac.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/conf/conf.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cpu-aarch64-linux.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cpu-arm-linux.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cpu-arm.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cpu-intel.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/cpu-ppc64le.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/crypto.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/curve25519/curve25519.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/curve25519/spake25519.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/curve25519/x25519-x86_64.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/des/des.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/dh/check.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/dh/dh.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/dh/dh_asn1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/dh/params.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/digest/digest.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/digest/digests.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/dsa/dsa.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/dsa/dsa_asn1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ec/ec.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ec/ec_asn1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ec/ec_key.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ec/ec_montgomery.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ec/oct.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ec/p224-64.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ec/p256-64.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ec/p256-x86_64.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ec/simple.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ec/util-64.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ec/wnaf.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ecdh/ecdh.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ecdsa/ecdsa.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ecdsa/ecdsa_asn1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/engine/engine.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/err/err.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/evp/digestsign.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/evp/evp.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/evp/evp_asn1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/evp/evp_ctx.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/evp/p_dsa_asn1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/evp/p_ec.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/evp/p_ec_asn1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/evp/p_rsa.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/evp/p_rsa_asn1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/evp/pbkdf.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/evp/print.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/evp/sign.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/ex_data.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/hkdf/hkdf.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/hmac/hmac.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/lhash/lhash.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/md4/md4.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/md5/md5.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/mem.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/modes/cbc.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/modes/cfb.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/modes/ctr.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/modes/gcm.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/modes/ofb.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/newhope/error_correction.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/newhope/newhope.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/newhope/ntt.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/newhope/poly.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/newhope/precomp.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/newhope/reduce.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/obj/obj.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/obj/obj_xref.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/pem/pem_all.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/pem/pem_info.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/pem/pem_lib.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/pem/pem_oth.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/pem/pem_pk8.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/pem/pem_pkey.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/pem/pem_x509.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/pem/pem_xaux.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/pkcs8/p5_pbe.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/pkcs8/p5_pbev2.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/pkcs8/p8_pkey.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/pkcs8/pkcs8.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/poly1305/poly1305.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/poly1305/poly1305_arm.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/poly1305/poly1305_vec.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/rand/deterministic.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/rand/rand.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/rand/urandom.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/rand/windows.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/rc4/rc4.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/refcount_c11.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/refcount_lock.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/rsa/blinding.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/rsa/padding.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/rsa/rsa.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/rsa/rsa_asn1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/rsa/rsa_impl.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/sha/sha1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/sha/sha256.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/sha/sha512.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/stack/stack.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/thread.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/thread_none.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/thread_pthread.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/thread_win.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/time_support.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/a_digest.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/a_sign.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/a_strex.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/a_verify.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/algorithm.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/asn1_gen.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/by_dir.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/by_file.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/i2d_pr.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/pkcs7.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/rsa_pss.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/t_crl.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/t_req.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/t_x509.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/t_x509a.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_att.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_cmp.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_d2.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_def.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_ext.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_lu.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_obj.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_r2x.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_req.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_set.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_trs.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_txt.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_v3.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_vfy.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509_vpm.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509cset.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509name.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509rset.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509spki.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x509type.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_algor.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_all.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_attrib.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_crl.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_exten.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_info.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_name.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_pkey.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_pubkey.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_req.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_sig.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_spki.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_val.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_x509.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509/x_x509a.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/pcy_cache.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/pcy_data.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/pcy_lib.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/pcy_map.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/pcy_node.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/pcy_tree.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_akey.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_akeya.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_alt.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_bcons.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_bitst.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_conf.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_cpols.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_crld.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_enum.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_extku.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_genn.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_ia5.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_info.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_int.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_lib.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_ncons.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_pci.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_pcia.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_pcons.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_pku.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_pmaps.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_prn.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_purp.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_skey.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_sxnet.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/crypto/x509v3/v3_utl.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/custom_extensions.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/d1_both.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/d1_lib.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/d1_pkt.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/d1_srtp.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/dtls_method.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/dtls_record.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/handshake_client.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/handshake_server.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/s3_both.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/s3_enc.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/s3_lib.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/s3_pkt.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/ssl_aead_ctx.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/ssl_asn1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/ssl_buffer.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/ssl_cert.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/ssl_cipher.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/ssl_ecdh.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/ssl_file.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/ssl_lib.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/ssl_rsa.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/ssl_session.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/ssl_stat.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/t1_enc.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/t1_lib.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/tls13_both.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/tls13_client.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/tls13_enc.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/tls13_server.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/tls_method.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/ssl/tls_record.c
    $<TARGET_OBJECTS:Third_PartyBoringsslBoringssl_Asm>)
add_library(Third_PartyBoringsslBoringssl STATIC ${Third_PartyBoringsslBoringssl_SOURCES})
target_compile_options(Third_PartyBoringsslBoringssl PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Werror" "-Wall" "-Wno-unused-variable" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyBoringsslBoringssl PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 BORINGSSL_IMPLEMENTATION BORINGSSL_NO_STATIC_INITIALIZER OPENSSL_SMALL _XOPEN_SOURCE=700)
target_include_directories(Third_PartyBoringsslBoringssl PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/boringssl/src/include/)
set_target_properties(Third_PartyBoringsslBoringssl PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(Third_PartyBoringsslBoringssl PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/)
set_target_properties(Third_PartyBoringsslBoringssl PROPERTIES OUTPUT_NAME boringssl)
# End of target Third_PartyBoringsslBoringssl


# Start of target WebrtcVideoVideo
set(WebrtcVideoVideo_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/video/call_stats.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/encoder_rtcp_feedback.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/overuse_frame_detector.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/payload_router.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/receive_statistics_proxy.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/report_block_stats.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/rtp_stream_receiver.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/rtp_streams_synchronizer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/send_delay_stats.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/send_statistics_proxy.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/stats_counter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/stream_synchronization.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/transport_adapter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/video_decoder.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/video_encoder.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/video_receive_stream.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/video_send_stream.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/video_stream_decoder.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/vie_encoder.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/video/vie_remb.cc
    $<TARGET_OBJECTS:WebrtcLoggingRtc_Event_Log_Api>)
add_library(WebrtcVideoVideo STATIC ${WebrtcVideoVideo_SOURCES})
target_compile_options(WebrtcVideoVideo PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcVideoVideo PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcVideoVideo PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcVideoVideo PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcVideoVideo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/video)
set_target_properties(WebrtcVideoVideo PROPERTIES OUTPUT_NAME video)
target_link_libraries(WebrtcVideoVideo WebrtcWebrtc_Common)
target_link_libraries(WebrtcVideoVideo WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcVideoVideo WebrtcBaseRtc_Task_Queue)
target_link_libraries(WebrtcVideoVideo WebrtcCommon_VideoCommon_Video)
target_link_libraries(WebrtcVideoVideo WebrtcModulesBitrate_ControllerBitrate_Controller)
target_link_libraries(WebrtcVideoVideo WebrtcModulesCongestion_ControllerCongestion_Controller)
target_link_libraries(WebrtcVideoVideo WebrtcModulesPacingPacing)
target_link_libraries(WebrtcVideoVideo WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator)
target_link_libraries(WebrtcVideoVideo WebrtcModulesRtp_RtcpRtp_Rtcp)
target_link_libraries(WebrtcVideoVideo WebrtcModulesUtilityUtility)
target_link_libraries(WebrtcVideoVideo WebrtcModulesVideo_CodingVideo_Coding)
target_link_libraries(WebrtcVideoVideo WebrtcModulesVideo_ProcessingVideo_Processing)
target_link_libraries(WebrtcVideoVideo WebrtcSystem_WrappersSystem_Wrappers)
target_link_libraries(WebrtcVideoVideo WebrtcVoice_EngineVoice_Engine)
target_include_directories(WebrtcVideoVideo PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcVideoVideo ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcVideoVideo PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcVideoVideo ${COREVIDEO_LIBRARY})
target_include_directories(WebrtcVideoVideo PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcVideoVideo ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcVideoVideo PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcVideoVideo ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcVideoVideo PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcVideoVideo ${COREAUDIO_LIBRARY})
target_include_directories(WebrtcVideoVideo PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcVideoVideo ${COREGRAPHICS_LIBRARY})
# End of target WebrtcVideoVideo

# Start of target WebrtcSdkRtc_Sdk_Common_Objc
set(WebrtcSdkRtc_Sdk_Common_Objc_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/NSString+StdString.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCDispatcher.m
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCFieldTrials.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCLogging.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCMetrics.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCMetricsSampleInfo.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCSSLAdapter.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCTracing.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/RTCFileLogger.mm)
add_library(WebrtcSdkRtc_Sdk_Common_Objc STATIC ${WebrtcSdkRtc_Sdk_Common_Objc_SOURCES})
target_compile_options(WebrtcSdkRtc_Sdk_Common_Objc PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-arc" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcSdkRtc_Sdk_Common_Objc PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC SSL_USE_OPENSSL HAVE_OPENSSL_SSL_H FEATURE_ENABLE_SSL)
target_include_directories(WebrtcSdkRtc_Sdk_Common_Objc PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Headers/ ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcSdkRtc_Sdk_Common_Objc PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcSdkRtc_Sdk_Common_Objc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/sdk)
set_target_properties(WebrtcSdkRtc_Sdk_Common_Objc PROPERTIES OUTPUT_NAME rtc_sdk_common_objc)
target_link_libraries(WebrtcSdkRtc_Sdk_Common_Objc WebrtcBaseRtc_Base)
target_include_directories(WebrtcSdkRtc_Sdk_Common_Objc PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Common_Objc ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Common_Objc PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Common_Objc ${COCOA_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Common_Objc PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Common_Objc ${IOKIT_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Common_Objc PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Common_Objc ${SECURITY_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Common_Objc PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Common_Objc ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Common_Objc PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Common_Objc ${APPLICATIONSERVICES_LIBRARY})
# End of target WebrtcSdkRtc_Sdk_Common_Objc

# Start of target WebrtcModulesAudio_ProcessingAudio_Processing_Sse2
set(WebrtcModulesAudio_ProcessingAudio_Processing_Sse2_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/aec/aec_core_sse2.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_processing/utility/ooura_fft_sse2.cc)
add_library(WebrtcModulesAudio_ProcessingAudio_Processing_Sse2 STATIC ${WebrtcModulesAudio_ProcessingAudio_Processing_Sse2_SOURCES})
target_compile_options(WebrtcModulesAudio_ProcessingAudio_Processing_Sse2 PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-msse2" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_ProcessingAudio_Processing_Sse2 PRIVATE WEBRTC_APM_DEBUG_DUMP=0 V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_ProcessingAudio_Processing_Sse2 PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_ProcessingAudio_Processing_Sse2 PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_ProcessingAudio_Processing_Sse2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_processing)
set_target_properties(WebrtcModulesAudio_ProcessingAudio_Processing_Sse2 PROPERTIES OUTPUT_NAME audio_processing_sse2)
# End of target WebrtcModulesAudio_ProcessingAudio_Processing_Sse2






# Start of target WebrtcToolsRgba_To_I420_Converter
set(WebrtcToolsRgba_To_I420_Converter_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/converter/converter.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/tools/converter/rgba_to_i420_converter.cc)
add_executable(WebrtcToolsRgba_To_I420_Converter ${WebrtcToolsRgba_To_I420_Converter_SOURCES})
target_compile_options(WebrtcToolsRgba_To_I420_Converter PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcToolsRgba_To_I420_Converter PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcToolsRgba_To_I420_Converter PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcToolsRgba_To_I420_Converter PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9 -Wl,-ObjC -Wl,-pie")
set_target_properties(WebrtcToolsRgba_To_I420_Converter PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/)
set_target_properties(WebrtcToolsRgba_To_I420_Converter PROPERTIES OUTPUT_NAME rgba_to_i420_converter)
target_link_libraries(WebrtcToolsRgba_To_I420_Converter WebrtcCommon_VideoCommon_Video)
target_link_libraries(WebrtcToolsRgba_To_I420_Converter WebrtcToolsCommand_Line_Parser)
target_include_directories(WebrtcToolsRgba_To_I420_Converter PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcToolsRgba_To_I420_Converter ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcToolsRgba_To_I420_Converter PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcToolsRgba_To_I420_Converter ${COREVIDEO_LIBRARY})
add_dependencies(WebrtcToolsRgba_To_I420_Converter BuildConfigSanitizersDeps BuildWinDefault_Exe_Manifest WebrtcCommon_VideoCommon_Video WebrtcToolsCommand_Line_Parser)
# End of target WebrtcToolsRgba_To_I420_Converter


# Start of target WebrtcModulesCongestion_ControllerCongestion_Controller
set(WebrtcModulesCongestion_ControllerCongestion_Controller_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/congestion_controller/congestion_controller.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/congestion_controller/delay_based_bwe.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/congestion_controller/probe_bitrate_estimator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/congestion_controller/probe_controller.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/congestion_controller/transport_feedback_adapter.cc)
add_library(WebrtcModulesCongestion_ControllerCongestion_Controller STATIC ${WebrtcModulesCongestion_ControllerCongestion_Controller_SOURCES})
target_compile_options(WebrtcModulesCongestion_ControllerCongestion_Controller PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesCongestion_ControllerCongestion_Controller PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesCongestion_ControllerCongestion_Controller PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesCongestion_ControllerCongestion_Controller PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesCongestion_ControllerCongestion_Controller PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/congestion_controller)
set_target_properties(WebrtcModulesCongestion_ControllerCongestion_Controller PROPERTIES OUTPUT_NAME congestion_controller)
target_link_libraries(WebrtcModulesCongestion_ControllerCongestion_Controller WebrtcModulesBitrate_ControllerBitrate_Controller)
target_link_libraries(WebrtcModulesCongestion_ControllerCongestion_Controller WebrtcModulesPacingPacing)
target_include_directories(WebrtcModulesCongestion_ControllerCongestion_Controller PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesCongestion_ControllerCongestion_Controller ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcModulesCongestion_ControllerCongestion_Controller PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcModulesCongestion_ControllerCongestion_Controller ${COREVIDEO_LIBRARY})
# End of target WebrtcModulesCongestion_ControllerCongestion_Controller





# Start of target WebrtcStatsRtc_Stats
set(WebrtcStatsRtc_Stats_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/stats/rtcstats.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/stats/rtcstats_objects.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/stats/rtcstatsreport.cc
    $<TARGET_OBJECTS:WebrtcApiRtc_Stats_Api>)
add_library(WebrtcStatsRtc_Stats STATIC ${WebrtcStatsRtc_Stats_SOURCES})
target_compile_options(WebrtcStatsRtc_Stats PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcStatsRtc_Stats PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcStatsRtc_Stats PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcStatsRtc_Stats PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcStatsRtc_Stats PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/stats)
set_target_properties(WebrtcStatsRtc_Stats PROPERTIES OUTPUT_NAME rtc_stats)
target_link_libraries(WebrtcStatsRtc_Stats WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcStatsRtc_Stats WebrtcBaseRtc_Base_Approved)
target_include_directories(WebrtcStatsRtc_Stats PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcStatsRtc_Stats ${FOUNDATION_LIBRARY})
# End of target WebrtcStatsRtc_Stats


# Start of target WebrtcSdkRtc_Sdk_Objc
set(WebrtcSdkRtc_Sdk_Objc_SOURCES ${LIBWEBRTC_OUTPUT_DIR}/dummy.c)
add_library(WebrtcSdkRtc_Sdk_Objc STATIC ${WebrtcSdkRtc_Sdk_Objc_SOURCES})
target_compile_options(WebrtcSdkRtc_Sdk_Objc PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcSdkRtc_Sdk_Objc PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Headers/ ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcSdkRtc_Sdk_Objc PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcSdkRtc_Sdk_Objc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/sdk)
set_target_properties(WebrtcSdkRtc_Sdk_Objc PROPERTIES OUTPUT_NAME rtc_sdk_objc)
target_link_libraries(WebrtcSdkRtc_Sdk_Objc WebrtcSdkRtc_Sdk_Peerconnection_Objc)
target_link_libraries(WebrtcSdkRtc_Sdk_Objc WebrtcSystem_WrappersField_Trial_Default)
target_link_libraries(WebrtcSdkRtc_Sdk_Objc WebrtcSystem_WrappersMetrics_Default)
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Objc ${COCOA_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Objc ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Objc ${IOKIT_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Objc ${SECURITY_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Objc ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${AVFOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Objc ${AVFOUNDATION_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${COREMEDIA_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Objc ${COREMEDIA_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Objc ${COREVIDEO_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${OPENGL_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Objc ${OPENGL_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Objc ${APPLICATIONSERVICES_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${AUDIOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Objc ${AUDIOTOOLBOX_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${COREAUDIO_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Objc ${COREAUDIO_LIBRARY})
target_include_directories(WebrtcSdkRtc_Sdk_Objc PRIVATE ${COREGRAPHICS_INCLUDE_DIR})
target_link_libraries(WebrtcSdkRtc_Sdk_Objc ${COREGRAPHICS_LIBRARY})
# End of target WebrtcSdkRtc_Sdk_Objc

# Start of target WebrtcModulesVideo_CaptureVideo_Capture_Module
set(WebrtcModulesVideo_CaptureVideo_Capture_Module_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_capture/device_info_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_capture/video_capture_factory.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_capture/video_capture_impl.cc)
add_library(WebrtcModulesVideo_CaptureVideo_Capture_Module STATIC ${WebrtcModulesVideo_CaptureVideo_Capture_Module_SOURCES})
target_compile_options(WebrtcModulesVideo_CaptureVideo_Capture_Module PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesVideo_CaptureVideo_Capture_Module PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesVideo_CaptureVideo_Capture_Module PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesVideo_CaptureVideo_Capture_Module PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesVideo_CaptureVideo_Capture_Module PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/video_capture)
set_target_properties(WebrtcModulesVideo_CaptureVideo_Capture_Module PROPERTIES OUTPUT_NAME video_capture_module)
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture_Module WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture_Module WebrtcCommon_VideoCommon_Video)
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture_Module WebrtcModulesUtilityUtility)
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture_Module WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesVideo_CaptureVideo_Capture_Module PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture_Module ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcModulesVideo_CaptureVideo_Capture_Module PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture_Module ${COREVIDEO_LIBRARY})
# End of target WebrtcModulesVideo_CaptureVideo_Capture_Module


# Start of target WebrtcSystem_WrappersMetrics_Default
set(WebrtcSystem_WrappersMetrics_Default_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/system_wrappers/source/metrics_default.cc)
add_library(WebrtcSystem_WrappersMetrics_Default STATIC ${WebrtcSystem_WrappersMetrics_Default_SOURCES})
target_compile_options(WebrtcSystem_WrappersMetrics_Default PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcSystem_WrappersMetrics_Default PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcSystem_WrappersMetrics_Default PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcSystem_WrappersMetrics_Default PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcSystem_WrappersMetrics_Default PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/system_wrappers)
set_target_properties(WebrtcSystem_WrappersMetrics_Default PROPERTIES OUTPUT_NAME metrics_default)
# End of target WebrtcSystem_WrappersMetrics_Default

# Start of target WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library_Bundle_Data
add_custom_target(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library_Bundle_Data)
add_dependencies(WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library_Bundle_Data WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library)
# End of target WebrtcSdkRtc_Sdk_Framework_Objc_Shared_Library_Bundle_Data





# Start of target Third_PartyProtobufProtobuf_Full
set(Third_PartyProtobufProtobuf_Full_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/arena.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/arenastring.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/extension_set.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/generated_message_util.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/io/coded_stream.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/io/zero_copy_stream.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/message_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/repeated_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/atomicops_internals_x86_gcc.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/atomicops_internals_x86_msvc.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/bytestream.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/common.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/int128.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/once.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/status.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/statusor.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/stringpiece.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/stringprintf.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/structurally_valid.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/strutil.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/time.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/wire_format_lite.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/any.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/any.pb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/api.pb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/importer.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/compiler/parser.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/descriptor.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/descriptor.pb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/descriptor_database.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/duration.pb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/dynamic_message.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/empty.pb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/extension_set_heavy.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/field_mask.pb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/generated_message_reflection.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/io/printer.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/io/strtod.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/io/tokenizer.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/map_field.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/message.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/reflection_ops.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/service.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/source_context.pb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/struct.pb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/mathlimits.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/stubs/substitute.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/text_format.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/timestamp.pb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/type.pb.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/unknown_field_set.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/field_comparator.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/field_mask_util.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/datapiece.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/default_value_objectwriter.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/error_listener.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/field_mask_utility.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/json_escaping.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/json_objectwriter.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/json_stream_parser.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/object_writer.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/proto_writer.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/protostream_objectsource.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/protostream_objectwriter.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/type_info.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/type_info_test_helper.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/internal/utility.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/json_util.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/message_differencer.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/time_util.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/util/type_resolver_util.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/wire_format.cc
    ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/google/protobuf/wrappers.pb.cc)
add_library(Third_PartyProtobufProtobuf_Full STATIC ${Third_PartyProtobufProtobuf_Full_SOURCES})
target_compile_options(Third_PartyProtobufProtobuf_Full PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Werror" "-Wall" "-Wno-unused-variable" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-Wno-unused-function" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyProtobufProtobuf_Full PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 GOOGLE_PROTOBUF_NO_RTTI GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER HAVE_PTHREAD)
target_include_directories(Third_PartyProtobufProtobuf_Full PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/protobuf/src/)
set_target_properties(Third_PartyProtobufProtobuf_Full PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(Third_PartyProtobufProtobuf_Full PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/third_party/protobuf)
set_target_properties(Third_PartyProtobufProtobuf_Full PROPERTIES OUTPUT_NAME protobuf_full)
# End of target Third_PartyProtobufProtobuf_Full

# Start of target WebrtcModulesAudio_CodingCng
set(WebrtcModulesAudio_CodingCng_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/cng/audio_encoder_cng.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/cng/webrtc_cng.cc)
add_library(WebrtcModulesAudio_CodingCng STATIC ${WebrtcModulesAudio_CodingCng_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingCng PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingCng PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingCng PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/cng/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingCng PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingCng PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingCng PROPERTIES OUTPUT_NAME cng)
target_link_libraries(WebrtcModulesAudio_CodingCng WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcModulesAudio_CodingCng WebrtcModulesAudio_CodingAudio_Encoder_Interface)
target_include_directories(WebrtcModulesAudio_CodingCng PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingCng ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingCng

# Start of target Third_PartyOpusOpus
set(Third_PartyOpusOpus_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/bands.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/celt.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/celt_decoder.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/celt_encoder.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/celt_lpc.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/cwrs.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/entcode.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/entdec.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/entenc.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/kiss_fft.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/laplace.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/mathops.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/mdct.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/modes.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/pitch.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/quant_bands.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/rate.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/vq.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/A2NLSF.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/CNG.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/HP_variable_cutoff.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/LPC_analysis_filter.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/LPC_inv_pred_gain.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/LP_variable_cutoff.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/NLSF2A.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/NLSF_VQ.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/NLSF_VQ_weights_laroia.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/NLSF_decode.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/NLSF_del_dec_quant.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/NLSF_encode.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/NLSF_stabilize.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/NLSF_unpack.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/NSQ.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/NSQ_del_dec.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/PLC.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/VAD.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/VQ_WMat_EC.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/ana_filt_bank_1.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/biquad_alt.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/bwexpander.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/bwexpander_32.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/check_control_input.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/code_signs.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/control_SNR.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/control_audio_bandwidth.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/control_codec.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/debug.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/dec_API.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/decode_core.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/decode_frame.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/decode_indices.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/decode_parameters.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/decode_pitch.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/decode_pulses.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/decoder_set_fs.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/enc_API.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/encode_indices.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/encode_pulses.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/gain_quant.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/init_decoder.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/init_encoder.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/inner_prod_aligned.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/interpolate.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/lin2log.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/log2lin.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/pitch_est_tables.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/process_NLSFs.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/quant_LTP_gains.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/resampler.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/resampler_down2.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/resampler_down2_3.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/resampler_private_AR2.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/resampler_private_IIR_FIR.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/resampler_private_down_FIR.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/resampler_private_up2_HQ.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/resampler_rom.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/shell_coder.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/sigm_Q15.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/sort.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/stereo_LR_to_MS.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/stereo_MS_to_LR.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/stereo_decode_pred.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/stereo_encode_pred.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/stereo_find_predictor.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/stereo_quant_pred.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/sum_sqr_shift.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/table_LSF_cos.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/tables_LTP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/tables_NLSF_CB_NB_MB.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/tables_NLSF_CB_WB.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/tables_gain.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/tables_other.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/tables_pitch_lag.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/tables_pulses_per_block.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/analysis.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/mlp.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/mlp_data.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/opus.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/opus_decoder.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/opus_encoder.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/opus_multistream.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/opus_multistream_decoder.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/opus_multistream_encoder.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/repacketizer.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/LPC_analysis_filter_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/LPC_inv_pred_gain_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/LTP_analysis_filter_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/LTP_scale_ctrl_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/apply_sine_window_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/autocorrelation_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/burg_modified_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/bwexpander_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/corrMatrix_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/encode_frame_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/energy_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/find_LPC_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/find_LTP_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/find_pitch_lags_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/find_pred_coefs_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/inner_product_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/k2a_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/levinsondurbin_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/noise_shape_analysis_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/pitch_analysis_core_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/prefilter_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/process_gains_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/regularize_correlations_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/residual_energy_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/scale_copy_vector_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/scale_vector_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/schur_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/solve_LS_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/sort_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/warped_autocorrelation_FLP.c
    ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/wrappers_FLP.c)
add_library(Third_PartyOpusOpus STATIC ${Third_PartyOpusOpus_SOURCES})
target_compile_options(Third_PartyOpusOpus PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wno-#pragma-messages" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Werror" "-Wall" "-Wno-unused-variable" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyOpusOpus PRIVATE OPUS_BUILD OPUS_EXPORT= HAVE_LRINT HAVE_LRINTF VAR_ARRAYS V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1)
target_include_directories(Third_PartyOpusOpus PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/float/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(Third_PartyOpusOpus PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(Third_PartyOpusOpus PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/third_party/opus)
set_target_properties(Third_PartyOpusOpus PROPERTIES OUTPUT_NAME opus)
# End of target Third_PartyOpusOpus



# Start of target WebrtcCommon_AudioCommon_Audio_Sse2
set(WebrtcCommon_AudioCommon_Audio_Sse2_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/fir_filter_sse.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/sinc_resampler_sse.cc)
add_library(WebrtcCommon_AudioCommon_Audio_Sse2 STATIC ${WebrtcCommon_AudioCommon_Audio_Sse2_SOURCES})
target_compile_options(WebrtcCommon_AudioCommon_Audio_Sse2 PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-msse2" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcCommon_AudioCommon_Audio_Sse2 PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcCommon_AudioCommon_Audio_Sse2 PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcCommon_AudioCommon_Audio_Sse2 PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcCommon_AudioCommon_Audio_Sse2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/common_audio)
set_target_properties(WebrtcCommon_AudioCommon_Audio_Sse2 PROPERTIES OUTPUT_NAME common_audio_sse2)
# End of target WebrtcCommon_AudioCommon_Audio_Sse2

# Start of target WebrtcModulesVideo_CodingVideo_Coding_Utility
set(WebrtcModulesVideo_CodingVideo_Coding_Utility_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/utility/frame_dropper.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/utility/ivf_file_writer.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/utility/moving_average.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/utility/qp_parser.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/utility/quality_scaler.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/utility/simulcast_rate_allocator.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_coding/utility/vp8_header_parser.cc)
add_library(WebrtcModulesVideo_CodingVideo_Coding_Utility STATIC ${WebrtcModulesVideo_CodingVideo_Coding_Utility_SOURCES})
target_compile_options(WebrtcModulesVideo_CodingVideo_Coding_Utility PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesVideo_CodingVideo_Coding_Utility PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesVideo_CodingVideo_Coding_Utility PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_video/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesVideo_CodingVideo_Coding_Utility PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesVideo_CodingVideo_Coding_Utility PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/video_coding)
set_target_properties(WebrtcModulesVideo_CodingVideo_Coding_Utility PROPERTIES OUTPUT_NAME video_coding_utility)
target_link_libraries(WebrtcModulesVideo_CodingVideo_Coding_Utility WebrtcCommon_VideoCommon_Video)
target_link_libraries(WebrtcModulesVideo_CodingVideo_Coding_Utility WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesVideo_CodingVideo_Coding_Utility PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CodingVideo_Coding_Utility ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcModulesVideo_CodingVideo_Coding_Utility PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CodingVideo_Coding_Utility ${COREVIDEO_LIBRARY})
# End of target WebrtcModulesVideo_CodingVideo_Coding_Utility

# Start of target WebrtcSdkWebrtc_H264_Video_Toolbox
set(WebrtcSdkWebrtc_H264_Video_Toolbox_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/h264_video_toolbox_decoder.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/h264_video_toolbox_encoder.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/h264_video_toolbox_nalu.cc)
add_library(WebrtcSdkWebrtc_H264_Video_Toolbox STATIC ${WebrtcSdkWebrtc_H264_Video_Toolbox_SOURCES})
target_compile_options(WebrtcSdkWebrtc_H264_Video_Toolbox PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-Wno-thread-safety-analysis" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-arc" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcSdkWebrtc_H264_Video_Toolbox PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcSdkWebrtc_H264_Video_Toolbox PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/libyuv/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Headers/ ${LIBWEBRTC_INPUT_DIR}/webrtc/sdk/objc/Framework/Classes/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcSdkWebrtc_H264_Video_Toolbox PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcSdkWebrtc_H264_Video_Toolbox PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/sdk)
set_target_properties(WebrtcSdkWebrtc_H264_Video_Toolbox PROPERTIES OUTPUT_NAME webrtc_h264_video_toolbox)
target_link_libraries(WebrtcSdkWebrtc_H264_Video_Toolbox Third_PartyLibyuvLibyuv)
target_link_libraries(WebrtcSdkWebrtc_H264_Video_Toolbox WebrtcSdkRtc_Sdk_Common_Objc)
target_include_directories(WebrtcSdkWebrtc_H264_Video_Toolbox PRIVATE ${COREFOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkWebrtc_H264_Video_Toolbox ${COREFOUNDATION_LIBRARY})
target_include_directories(WebrtcSdkWebrtc_H264_Video_Toolbox PRIVATE ${COREMEDIA_INCLUDE_DIR})
target_link_libraries(WebrtcSdkWebrtc_H264_Video_Toolbox ${COREMEDIA_LIBRARY})
target_include_directories(WebrtcSdkWebrtc_H264_Video_Toolbox PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcSdkWebrtc_H264_Video_Toolbox ${COREVIDEO_LIBRARY})
target_include_directories(WebrtcSdkWebrtc_H264_Video_Toolbox PRIVATE ${VIDEOTOOLBOX_INCLUDE_DIR})
target_link_libraries(WebrtcSdkWebrtc_H264_Video_Toolbox ${VIDEOTOOLBOX_LIBRARY})
target_include_directories(WebrtcSdkWebrtc_H264_Video_Toolbox PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkWebrtc_H264_Video_Toolbox ${FOUNDATION_LIBRARY})
target_include_directories(WebrtcSdkWebrtc_H264_Video_Toolbox PRIVATE ${COCOA_INCLUDE_DIR})
target_link_libraries(WebrtcSdkWebrtc_H264_Video_Toolbox ${COCOA_LIBRARY})
target_include_directories(WebrtcSdkWebrtc_H264_Video_Toolbox PRIVATE ${IOKIT_INCLUDE_DIR})
target_link_libraries(WebrtcSdkWebrtc_H264_Video_Toolbox ${IOKIT_LIBRARY})
target_include_directories(WebrtcSdkWebrtc_H264_Video_Toolbox PRIVATE ${SECURITY_INCLUDE_DIR})
target_link_libraries(WebrtcSdkWebrtc_H264_Video_Toolbox ${SECURITY_LIBRARY})
target_include_directories(WebrtcSdkWebrtc_H264_Video_Toolbox PRIVATE ${SYSTEMCONFIGURATION_INCLUDE_DIR})
target_link_libraries(WebrtcSdkWebrtc_H264_Video_Toolbox ${SYSTEMCONFIGURATION_LIBRARY})
target_include_directories(WebrtcSdkWebrtc_H264_Video_Toolbox PRIVATE ${APPLICATIONSERVICES_INCLUDE_DIR})
target_link_libraries(WebrtcSdkWebrtc_H264_Video_Toolbox ${APPLICATIONSERVICES_LIBRARY})
# End of target WebrtcSdkWebrtc_H264_Video_Toolbox





# Start of target Third_PartyJsoncppJsoncpp
set(Third_PartyJsoncppJsoncpp_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/jsoncpp/overrides/src/lib_json/json_reader.cpp
    ${LIBWEBRTC_INPUT_DIR}/third_party/jsoncpp/overrides/src/lib_json/json_value.cpp
    ${LIBWEBRTC_INPUT_DIR}/third_party/jsoncpp/source/src/lib_json/json_writer.cpp)
add_library(Third_PartyJsoncppJsoncpp OBJECT ${Third_PartyJsoncppJsoncpp_SOURCES})
target_compile_options(Third_PartyJsoncppJsoncpp PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyJsoncppJsoncpp PRIVATE JSON_USE_EXCEPTION=0 V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1)
target_include_directories(Third_PartyJsoncppJsoncpp PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/jsoncpp/overrides/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/jsoncpp/source/src/lib_json/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/jsoncpp/source/include/)
set_target_properties(Third_PartyJsoncppJsoncpp PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
# End of target Third_PartyJsoncppJsoncpp

# Start of target WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl
set(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_capture/objc/device_info.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_capture/objc/device_info_objc.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_capture/objc/rtc_video_capture_objc.mm
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/video_capture/objc/video_capture.mm)
add_library(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl STATIC ${WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl_SOURCES})
target_compile_options(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-fobjc-arc" "-Wno-deprecated-declarations" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/video_capture)
set_target_properties(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl PROPERTIES OUTPUT_NAME video_capture_internal_impl)
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl WebrtcModulesVideo_CaptureVideo_Capture_Module)
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl PRIVATE ${AVFOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl ${AVFOUNDATION_LIBRARY})
target_include_directories(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl PRIVATE ${COREMEDIA_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl ${COREMEDIA_LIBRARY})
target_include_directories(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl PRIVATE ${COREVIDEO_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl ${COREVIDEO_LIBRARY})
target_include_directories(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl

# Start of target Third_PartyOpusOpus_Compare
set(Third_PartyOpusOpus_Compare_SOURCES ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/src/opus_compare.c)
add_executable(Third_PartyOpusOpus_Compare ${Third_PartyOpusOpus_Compare_SOURCES})
target_compile_options(Third_PartyOpusOpus_Compare PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Werror" "-Wall" "-Wno-unused-variable" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-Wno-absolute-value" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-fobjc-call-cxx-cdtors")
target_compile_definitions(Third_PartyOpusOpus_Compare PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1)
target_include_directories(Third_PartyOpusOpus_Compare PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/silk/ ${LIBWEBRTC_OUTPUT_DIR}/gen/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/celt/)
set_target_properties(Third_PartyOpusOpus_Compare PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9 -Wl,-ObjC -Wl,-pie")
set_target_properties(Third_PartyOpusOpus_Compare PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/)
set_target_properties(Third_PartyOpusOpus_Compare PROPERTIES OUTPUT_NAME opus_compare)
target_link_libraries(Third_PartyOpusOpus_Compare Third_PartyOpusOpus)
add_dependencies(Third_PartyOpusOpus_Compare BuildConfigSanitizersDeps BuildWinDefault_Exe_Manifest Third_PartyOpusOpus)
# End of target Third_PartyOpusOpus_Compare


# Start of target WebrtcModulesUtilityUtility
set(WebrtcModulesUtilityUtility_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/utility/source/audio_frame_operations.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/utility/source/coder.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/utility/source/file_player.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/utility/source/file_recorder.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/utility/source/process_thread_impl.cc)
add_library(WebrtcModulesUtilityUtility STATIC ${WebrtcModulesUtilityUtility_SOURCES})
target_compile_options(WebrtcModulesUtilityUtility PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesUtilityUtility PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesUtilityUtility PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/third_party/opus/src/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesUtilityUtility PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesUtilityUtility PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/utility)
set_target_properties(WebrtcModulesUtilityUtility PROPERTIES OUTPUT_NAME utility)
target_link_libraries(WebrtcModulesUtilityUtility WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesUtilityUtility WebrtcBaseRtc_Task_Queue)
target_link_libraries(WebrtcModulesUtilityUtility WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcModulesUtilityUtility WebrtcModulesAudio_CodingAudio_Coding)
target_link_libraries(WebrtcModulesUtilityUtility WebrtcModulesMedia_FileMedia_File)
target_link_libraries(WebrtcModulesUtilityUtility WebrtcSystem_WrappersSystem_Wrappers)
target_include_directories(WebrtcModulesUtilityUtility PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesUtilityUtility ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesUtilityUtility


# Start of target WebrtcModulesAudio_CodingIsac
set(WebrtcModulesAudio_CodingIsac_SOURCES ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/arith_routines.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/arith_routines_hist.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/arith_routines_logist.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/audio_decoder_isac.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/audio_encoder_isac.cc
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/bandwidth_estimator.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/crc.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/decode.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/decode_bwe.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/encode.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/encode_lpc_swb.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/entropy_coding.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/fft.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/filter_functions.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/filterbank_tables.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/filterbanks.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/intialize.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/isac.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/lattice.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_analysis.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_gain_swb_tables.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_shape_swb12_tables.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_shape_swb16_tables.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_tables.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/pitch_estimator.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/pitch_filter.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/pitch_gain_tables.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/pitch_lag_tables.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/spectrum_ar_model_tables.c
    ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/transform.c)
add_library(WebrtcModulesAudio_CodingIsac STATIC ${WebrtcModulesAudio_CodingIsac_SOURCES})
target_compile_options(WebrtcModulesAudio_CodingIsac PRIVATE "-fno-strict-aliasing" "-fstack-protector-strong" "-fcolor-diagnostics" "-arch" "x86_64" "-gdwarf-2" "-mmacosx-version-min=10.9" "-Wall" "-Werror" "-Wextra" "-Wpartial-availability" "-Wno-missing-field-initializers" "-Wno-unused-parameter" "-Wno-c++11-narrowing" "-Wno-covered-switch-default" "-Wno-deprecated-register" "-Wno-unneeded-internal-declaration" "-Wno-inconsistent-missing-override" "-Wno-shift-negative-value" "-O0" "-fvisibility=hidden" "-Wheader-hygiene" "-Wstring-conversion" "-Wno-strict-overflow" "-Wimplicit-fallthrough" "-Wthread-safety" "-Winconsistent-missing-override" "-Wundef" "-std=c99" "-fno-threadsafe-statics" "-fvisibility-inlines-hidden" "-Wno-undefined-bool-conversion" "-Wno-tautological-undefined-compare" "-stdlib=libc++" "-fno-rtti" "-fno-exceptions" "-Wnon-virtual-dtor" "-Woverloaded-virtual" "-Wobjc-missing-property-synthesis" "-fobjc-call-cxx-cdtors")
target_compile_definitions(WebrtcModulesAudio_CodingIsac PRIVATE V8_DEPRECATION_WARNINGS ENABLE_NOTIFICATIONS ENABLE_PLUGINS=1 ENABLE_PDF=1 ENABLE_PRINTING=1 ENABLE_BASIC_PRINTING=1 ENABLE_PRINT_PREVIEW=1 ENABLE_SPELLCHECK=1 USE_BROWSER_SPELLCHECKER=1 NO_TCMALLOC USE_EXTERNAL_POPUP_MENU=1 ENABLE_WEBRTC=1 ENABLE_EXTENSIONS=1 ENABLE_TASK_MANAGER=1 ENABLE_THEMES=1 ENABLE_SESSION_SERVICE=1 ENABLE_SUPERVISED_USERS=1 FULL_SAFE_BROWSING SAFE_BROWSING_CSD SAFE_BROWSING_DB_LOCAL CHROMIUM_BUILD ENABLE_MEDIA_ROUTER=1 FIELDTRIAL_TESTING_ENABLED CR_CLANG_REVISION=284979-1 CR_XCODE_VERSION=0820 __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0 _DEBUG DYNAMIC_ANNOTATIONS_ENABLED=1 WTF_USE_DYNAMIC_ANNOTATIONS=1 WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE EXPAT_RELATIVE_PATH RTC_DISABLE_VP9 WEBRTC_POSIX WEBRTC_MAC)
target_include_directories(WebrtcModulesAudio_CodingIsac PRIVATE ${LIBWEBRTC_INPUT_DIR}/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/signal_processing/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/resampler/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/modules/audio_coding/codecs/isac/main/include/ ${LIBWEBRTC_INPUT_DIR}/webrtc/common_audio/vad/include/ ${LIBWEBRTC_OUTPUT_DIR}/gen/)
set_target_properties(WebrtcModulesAudio_CodingIsac PROPERTIES LINK_FLAGS "-stdlib=libc++ -arch x86_64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk -mmacosx-version-min=10.9")
set_target_properties(WebrtcModulesAudio_CodingIsac PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/modules/audio_coding)
set_target_properties(WebrtcModulesAudio_CodingIsac PROPERTIES OUTPUT_NAME isac)
target_link_libraries(WebrtcModulesAudio_CodingIsac WebrtcWebrtc_Common)
target_link_libraries(WebrtcModulesAudio_CodingIsac WebrtcBaseRtc_Base_Approved)
target_link_libraries(WebrtcModulesAudio_CodingIsac WebrtcCommon_AudioCommon_Audio)
target_link_libraries(WebrtcModulesAudio_CodingIsac WebrtcModulesAudio_CodingAudio_Decoder_Interface)
target_link_libraries(WebrtcModulesAudio_CodingIsac WebrtcModulesAudio_CodingAudio_Encoder_Interface)
target_link_libraries(WebrtcModulesAudio_CodingIsac WebrtcModulesAudio_CodingIsac_Common)
target_include_directories(WebrtcModulesAudio_CodingIsac PRIVATE ${FOUNDATION_INCLUDE_DIR})
target_link_libraries(WebrtcModulesAudio_CodingIsac ${FOUNDATION_LIBRARY})
# End of target WebrtcModulesAudio_CodingIsac



# Start of target LIBWEBRTC
file(WRITE ${LIBWEBRTC_OUTPUT_DIR}/list_libwebrtc_objects "CMakeFiles/WebrtcModulesMedia_FileMedia_File.dir/Source/webrtc/modules/media_file/media_file_utility.cc.o
CMakeFiles/WebrtcModulesMedia_FileMedia_File.dir/Source/webrtc/modules/media_file/media_file_impl.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/client/basicportallocator.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/relayserver.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/transportcontroller.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/p2ptransport.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/port.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/pseudotcp.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/relayport.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/udptransportchannel.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/stunrequest.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/tcpport.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/asyncstuntcpsocket.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/stunserver.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/p2pconstants.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/transportdescription.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/turnport.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/basicpacketsocketfactory.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/sessiondescription.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/client/socketmonitor.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/portallocator.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/transportdescriptionfactory.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/transportchannel.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/turnserver.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/stunport.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/transport.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/p2ptransportchannel.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/stun.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/session.cc.o
CMakeFiles/WebrtcP2PRtc_P2P.dir/Source/webrtc/p2p/base/dtlstransportchannel.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/rtp_frame_reference_finder.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/media_optimization.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/timing.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/video_sender.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/encoded_frame.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/percentile_filter.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/frame_buffer.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/jitter_estimator.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/packet_buffer.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/rtt_filter.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/nack_module.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/jitter_buffer.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/session_info.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/packet.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/timestamp_map.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/inter_frame_delay.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/h264_sps_pps_tracker.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/frame_buffer2.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/codec_database.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/media_opt_util.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/protection_bitrate_calculator.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/generic_encoder.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/generic_decoder.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/decoding_state.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/video_receiver.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/codec_timer.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/receiver.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/codecs/vp9/vp9_noop.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/histogram.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/video_coding_impl.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding.dir/Source/webrtc/modules/video_coding/frame_object.cc.o
CMakeFiles/Third_PartyLibyuvCompare.dir/Source/third_party/libyuv/util/compare.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Coding.dir/Source/webrtc/modules/audio_coding/acm2/acm_resampler.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Coding.dir/Source/webrtc/modules/audio_coding/acm2/acm_receiver.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Coding.dir/Source/webrtc/modules/audio_coding/acm2/codec_manager.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Coding.dir/Source/webrtc/modules/audio_coding/acm2/call_statistics.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Coding.dir/Source/webrtc/modules/audio_coding/acm2/audio_coding_module.cc.o
CMakeFiles/Third_PartyLibyuvCpuid.dir/Source/third_party/libyuv/util/cpuid.c.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/trace_posix.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/event_timer_posix.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/rw_lock.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/event.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/cpu_features.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/trace_impl.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/file_impl.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/clock.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/logging.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/cpu_info.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/atomic32_darwin.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/rtp_to_ntp.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/rw_lock_posix.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/sort.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/timestamp_extrapolator.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/sleep.cc.o
CMakeFiles/WebrtcSystem_WrappersSystem_Wrappers.dir/Source/webrtc/system_wrappers/source/aligned_malloc.cc.o
CMakeFiles/WebrtcModulesAudio_MixerAudio_Mixer_Impl.dir/Source/webrtc/modules/audio_mixer/audio_mixer_impl.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Format.dir/Source/webrtc/modules/audio_coding/codecs/audio_format.cc.o
CMakeFiles/WebrtcSystem_WrappersField_Trial_Default.dir/Source/webrtc/system_wrappers/source/field_trial_default.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_message_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_string_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_generator.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_map_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/code_generator.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_map_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_name_resolver.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_primitive_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_service.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_doc_comment.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_helpers.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_helpers.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_shared_code_generator.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_generator.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_primitive_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_generator.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_enum.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_message_builder.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/ruby/ruby_generator.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_service.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_file.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_helpers.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_extension.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/plugin.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_file.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/python/python_generator.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_oneof.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_enum_lite.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_repeated_enum_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_message_lite.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_enum_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_reflection_class.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_helpers.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_primitive_field_lite.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_extension.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_message.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_primitive_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_message.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/js/js_generator.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_repeated_message_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_helpers.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/plugin.pb.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_message.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_source_generator_base.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_file.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_string_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_map_field_lite.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_message_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_extension_lite.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_repeated_primitive_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_map_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_message_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_message_builder_lite.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_map_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_enum_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_enum_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_generator_factory.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_enum.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_map_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_enum_field_lite.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_lazy_message_field_lite.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_context.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_file.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/subprocess.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_primitive_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_wrapper_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_enum.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_string_field_lite.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_extension.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_primitive_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_doc_comment.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_enum_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/command_line_interface.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_message_field_lite.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_message_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_message.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/objectivec/objectivec_generator.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/zip_writer.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_message.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_message_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_enum_field.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_field_base.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_generator.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/csharp/csharp_enum.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/javanano/javanano_extension.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_enum.cc.o
CMakeFiles/Third_PartyProtobufProtoc_Lib.dir/Source/third_party/protobuf/src/google/protobuf/compiler/java/java_lazy_message_field.cc.o
CMakeFiles/WebrtcToolsEvent_Log_Visualizer_Utils.dir/Source/webrtc/tools/event_log_visualizer/plot_base.cc.o
CMakeFiles/WebrtcToolsEvent_Log_Visualizer_Utils.dir/Source/webrtc/tools/event_log_visualizer/plot_python.cc.o
CMakeFiles/WebrtcToolsEvent_Log_Visualizer_Utils.dir/Source/webrtc/tools/event_log_visualizer/plot_protobuf.cc.o
CMakeFiles/WebrtcToolsEvent_Log_Visualizer_Utils.dir/Source/webrtc/tools/event_log_visualizer/analyzer.cc.o
CMakeFiles/WebrtcToolsFrame_Editing_Lib.dir/Source/webrtc/tools/frame_editing/frame_editing_lib.cc.o
CMakeFiles/WebrtcModulesAudio_MixerAudio_Frame_Manipulator.dir/Source/webrtc/modules/audio_mixer/audio_frame_manipulator.cc.o
CMakeFiles/Third_PartyExpatExpat.dir/Source/third_party/expat/files/lib/xmlparse.c.o
CMakeFiles/Third_PartyExpatExpat.dir/Source/third_party/expat/files/lib/xmltok.c.o
CMakeFiles/Third_PartyExpatExpat.dir/Source/third_party/expat/files/lib/xmlrole.c.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/dtmfsender.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/rtcstatscollector.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/mediacontroller.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/datachannel.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/peerconnectionfactory.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/mediastream.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/mediastreamobserver.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/jsepsessiondescription.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/rtpsender.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/peerconnection.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/mediaconstraintsinterface.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/audiotrack.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/videotrack.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/statstypes.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/webrtcsessiondescriptionfactory.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/rtpreceiver.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/webrtcsdp.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/statscollector.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/videocapturertracksource.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/jsepicecandidate.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/sctputils.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/webrtcsession.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/remoteaudiosource.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/videotracksource.cc.o
CMakeFiles/WebrtcApiLibjingle_Peerconnection.dir/Source/webrtc/api/localaudiosource.cc.o
CMakeFiles/WebrtcP2PLibstunprober.dir/Source/webrtc/p2p/stunprober/stunprober.cc.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Common.dir/Source/webrtc/modules/audio_coding/codecs/isac/locked_bandwidth_info.cc.o
CMakeFiles/WebrtcModulesPacingPacing.dir/Source/webrtc/modules/pacing/alr_detector.cc.o
CMakeFiles/WebrtcModulesPacingPacing.dir/Source/webrtc/modules/pacing/paced_sender.cc.o
CMakeFiles/WebrtcModulesPacingPacing.dir/Source/webrtc/modules/pacing/bitrate_prober.cc.o
CMakeFiles/WebrtcModulesPacingPacing.dir/Source/webrtc/modules/pacing/packet_router.cc.o
CMakeFiles/WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator.dir/Source/webrtc/modules/remote_bitrate_estimator/aimd_rate_control.cc.o
CMakeFiles/WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator.dir/Source/webrtc/modules/remote_bitrate_estimator/inter_arrival.cc.o
CMakeFiles/WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator.dir/Source/webrtc/modules/remote_bitrate_estimator/send_time_history.cc.o
CMakeFiles/WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator.dir/Source/webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.cc.o
CMakeFiles/WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator.dir/Source/webrtc/modules/remote_bitrate_estimator/bwe_defines.cc.o
CMakeFiles/WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator.dir/Source/webrtc/modules/remote_bitrate_estimator/overuse_estimator.cc.o
CMakeFiles/WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator.dir/Source/webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.cc.o
CMakeFiles/WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator.dir/Source/webrtc/modules/remote_bitrate_estimator/overuse_detector.cc.o
CMakeFiles/WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator.dir/Source/webrtc/modules/remote_bitrate_estimator/remote_estimator_proxy.cc.o
CMakeFiles/WebrtcToolsPsnr_Ssim_Analyzer.dir/Source/webrtc/tools/psnr_ssim_analyzer/psnr_ssim_analyzer.cc.o
CMakeFiles/WebrtcVoice_EngineLevel_Indicator.dir/Source/webrtc/voice_engine/level_indicator.cc.o
CMakeFiles/WebrtcToolsVideo_Quality_Analysis.dir/Source/webrtc/tools/frame_analyzer/video_quality_analysis.cc.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/cipher/cipher.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/math/datatypes.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/kernel/err.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/kernel/key.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/cipher/aes_icm_ossl.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/math/stat.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/kernel/alloc.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/hash/null_auth.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/srtp/ekt.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/replay/rdb.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/cipher/aes_gcm_ossl.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/replay/rdbx.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/cipher/null_cipher.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/kernel/crypto_kernel.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/hash/hmac_ossl.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/crypto/hash/auth.c.o
CMakeFiles/Third_PartyLibsrtpLibsrtp.dir/Source/third_party/libsrtp/srtp/srtp.c.o
CMakeFiles/WebrtcToolsCommand_Line_Parser.dir/Source/webrtc/tools/simple_command_line_parser.cc.o
CMakeFiles/WebrtcBaseRtc_Task_Queue.dir/Source/webrtc/base/weak_ptr.cc.o
CMakeFiles/WebrtcBaseRtc_Task_Queue.dir/Source/webrtc/base/task_queue_posix.cc.o
CMakeFiles/WebrtcBaseRtc_Task_Queue.dir/Source/webrtc/base/task_queue_gcd.cc.o
CMakeFiles/WebrtcBaseRtc_Task_Queue.dir/Source/webrtc/base/sequenced_task_checker_impl.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/channel_proxy.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/output_mixer.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/voe_video_sync_impl.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/shared_data.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/voe_hardware_impl.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/transmit_mixer.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/voe_volume_control_impl.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/voe_rtp_rtcp_impl.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/voe_file_impl.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/channel_manager.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/voe_network_impl.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/voice_engine_impl.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/voe_external_media_impl.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/statistics.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/voe_audio_processing_impl.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/voe_neteq_stats_impl.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/voe_base_impl.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/utility.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/channel.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/monitor_module.cc.o
CMakeFiles/WebrtcVoice_EngineVoice_Engine.dir/Source/webrtc/voice_engine/voe_codec_impl.cc.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/audio_encoder_isacfix.cc.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/pitch_filter.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/transform_tables.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/decode_bwe.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/lattice_c.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/lattice.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/decode.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/pitch_filter_c.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/spectrum_ar_model_tables.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/filterbanks.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/fft.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/pitch_estimator_c.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/audio_decoder_isacfix.cc.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/filters.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/arith_routines.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/pitch_lag_tables.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/filterbank_tables.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/lpc_tables.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/arith_routines_logist.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/bandwidth_estimator.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/decode_plc.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/arith_routines_hist.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/transform.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/pitch_estimator.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/pitch_gain_tables.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/isacfix.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/initialize.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/encode.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/lpc_masking_model.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac_Fix.dir/Source/webrtc/modules/audio_coding/codecs/isac/fix/source/entropy_coding.c.o
CMakeFiles/WebrtcModulesAudio_CodingAna_Config_Proto.dir/gen/webrtc/modules/audio_coding/audio_network_adaptor/config.pb.cc.o
CMakeFiles/WebrtcPcRtc_Pc.dir/Source/webrtc/pc/channelmanager.cc.o
CMakeFiles/WebrtcPcRtc_Pc.dir/Source/webrtc/pc/rtcpmuxfilter.cc.o
CMakeFiles/WebrtcPcRtc_Pc.dir/Source/webrtc/pc/bundlefilter.cc.o
CMakeFiles/WebrtcPcRtc_Pc.dir/Source/webrtc/pc/channel.cc.o
CMakeFiles/WebrtcPcRtc_Pc.dir/Source/webrtc/pc/currentspeakermonitor.cc.o
CMakeFiles/WebrtcPcRtc_Pc.dir/Source/webrtc/pc/srtpfilter.cc.o
CMakeFiles/WebrtcPcRtc_Pc.dir/Source/webrtc/pc/mediasession.cc.o
CMakeFiles/WebrtcPcRtc_Pc.dir/Source/webrtc/pc/audiomonitor.cc.o
CMakeFiles/WebrtcPcRtc_Pc.dir/Source/webrtc/pc/mediamonitor.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq_Unittest_Proto.dir/gen/webrtc/audio_coding/neteq/neteq_unittest.pb.cc.o
CMakeFiles/WebrtcToolsFrame_Editor.dir/Source/webrtc/tools/frame_editing/frame_editing.cc.o
CMakeFiles/Third_PartyLibyuvConvert.dir/Source/third_party/libyuv/util/convert.cc.o
CMakeFiles/Third_PartyProtobufProtoc.dir/Source/third_party/protobuf/src/google/protobuf/compiler/main.cc.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/modes/ghash-x86_64.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/src/crypto/curve25519/asm/x25519-asm-x86_64.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/aes/vpaes-x86_64.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/sha/sha1-x86_64.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/ec/p256-x86_64-asm.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/bn/rsaz-avx2.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/bn/x86_64-mont5.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/chacha/chacha-x86_64.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/sha/sha256-x86_64.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/bn/x86_64-mont.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/sha/sha512-x86_64.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/aes/aesni-x86_64.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/modes/aesni-gcm-x86_64.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/aes/bsaes-x86_64.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/bn/rsaz-x86_64.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/aes/aes-x86_64.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/md5/md5-x86_64.S.o
CMakeFiles/Third_PartyBoringsslBoringssl_Asm.dir/Source/third_party/boringssl/mac-x86_64/crypto/rand/rdrand-x86_64.S.o
CMakeFiles/WebrtcModulesAudio_CodingRent_A_Codec.dir/Source/webrtc/modules/audio_coding/acm2/rent_a_codec.cc.o
CMakeFiles/WebrtcModulesAudio_CodingRent_A_Codec.dir/Source/webrtc/modules/audio_coding/acm2/acm_codec_database.cc.o
CMakeFiles/WebrtcModulesAudio_CodingPcm16B.dir/Source/webrtc/modules/audio_coding/codecs/pcm16b/audio_encoder_pcm16b.cc.o
CMakeFiles/WebrtcModulesAudio_CodingPcm16B.dir/Source/webrtc/modules/audio_coding/codecs/pcm16b/pcm16b.c.o
CMakeFiles/WebrtcModulesAudio_CodingPcm16B.dir/Source/webrtc/modules/audio_coding/codecs/pcm16b/audio_decoder_pcm16b.cc.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctputil.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_output.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_userspace.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/user_environment.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_peeloff.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_asconf.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_ss_functions.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_timer.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_sysctl.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/user_recv_thread.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet6/sctp6_usrreq.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_indata.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_usrreq.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_crc32.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_bsd_addr.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_cc_functions.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/user_socket.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_pcb.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_sha1.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_input.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/user_mbuf.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_auth.c.o
CMakeFiles/Third_PartyUsrsctpUsrsctp.dir/Source/third_party/usrsctp/usrsctplib/usrsctplib/netinet/sctp_callout.c.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/shared_memory.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/window_capturer_mac.mm.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/differ_block.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/mac/desktop_configuration.mm.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/desktop_capture_options.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/mac/scoped_pixel_buffer_object.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/mac/full_screen_chrome_window_detector.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/desktop_capturer.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/mac/desktop_configuration_monitor.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/desktop_capturer_differ_wrapper.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/mouse_cursor.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/desktop_and_cursor_composer.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/cropping_window_capturer.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/screen_capturer_helper.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/mac/window_list_utils.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/mouse_cursor_monitor_mac.mm.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/screen_capturer_mac.mm.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture.dir/Source/webrtc/modules/desktop_capture/cropped_desktop_frame.cc.o
CMakeFiles/WebrtcAudioAudio.dir/Source/webrtc/audio/audio_receive_stream.cc.o
CMakeFiles/WebrtcAudioAudio.dir/Source/webrtc/audio/audio_send_stream.cc.o
CMakeFiles/WebrtcAudioAudio.dir/Source/webrtc/audio/audio_state.cc.o
CMakeFiles/WebrtcModulesVideo_CodingWebrtc_I420.dir/Source/webrtc/modules/video_coding/codecs/i420/i420.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/rotate_common.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/scale_argb.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/convert_jpeg.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/scale.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/convert_to_argb.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/row_any.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/row_gcc.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/mjpeg_validate.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/rotate_gcc.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/rotate_mips.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/row_common.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/cpu_id.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/compare.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/convert_to_i420.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/compare_gcc.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/planar_functions.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/rotate_any.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/scale_any.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/convert_from_argb.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/scale_mips.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/convert_from.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/convert_argb.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/scale_gcc.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/rotate.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/scale_common.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/mjpeg_decoder.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/compare_common.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/convert.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/rotate_argb.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/video_common.cc.o
CMakeFiles/Third_PartyLibyuvLibyuv.dir/Source/third_party/libyuv/source/row_mips.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Encoder_Interface.dir/Source/webrtc/modules/audio_coding/codecs/audio_encoder.cc.o
CMakeFiles/WebrtcWebrtc_Common.dir/Source/webrtc/config.cc.o
CMakeFiles/WebrtcWebrtc_Common.dir/Source/webrtc/common_types.cc.o
CMakeFiles/Third_PartyGflagsGflags.dir/Source/third_party/gflags/src/src/gflags.cc.o
CMakeFiles/Third_PartyGflagsGflags.dir/Source/third_party/gflags/src/src/gflags_completions.cc.o
CMakeFiles/Third_PartyGflagsGflags.dir/Source/third_party/gflags/src/src/gflags_reporting.cc.o
CMakeFiles/Third_PartyLibyuvPsnr.dir/Source/third_party/libyuv/util/psnr.cc.o
CMakeFiles/Third_PartyLibyuvPsnr.dir/Source/third_party/libyuv/util/psnr_main.cc.o
CMakeFiles/Third_PartyLibyuvPsnr.dir/Source/third_party/libyuv/util/ssim.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Network_Adaptor.dir/Source/webrtc/modules/audio_coding/audio_network_adaptor/channel_controller.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Network_Adaptor.dir/Source/webrtc/modules/audio_coding/audio_network_adaptor/dtx_controller.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Network_Adaptor.dir/Source/webrtc/modules/audio_coding/audio_network_adaptor/frame_length_controller.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Network_Adaptor.dir/Source/webrtc/modules/audio_coding/audio_network_adaptor/bitrate_controller.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Network_Adaptor.dir/Source/webrtc/modules/audio_coding/audio_network_adaptor/debug_dump_writer.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Network_Adaptor.dir/Source/webrtc/modules/audio_coding/audio_network_adaptor/audio_network_adaptor.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Network_Adaptor.dir/Source/webrtc/modules/audio_coding/audio_network_adaptor/fec_controller.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Network_Adaptor.dir/Source/webrtc/modules/audio_coding/audio_network_adaptor/audio_network_adaptor_impl.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Network_Adaptor.dir/Source/webrtc/modules/audio_coding/audio_network_adaptor/smoothing_filter.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Network_Adaptor.dir/Source/webrtc/modules/audio_coding/audio_network_adaptor/controller_manager.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Network_Adaptor.dir/Source/webrtc/modules/audio_coding/audio_network_adaptor/controller.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/video_frame_buffer.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/h264/sps_parser.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/i420_buffer_pool.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/h264/h264_common.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/bitrate_adjuster.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/libyuv/webrtc_libyuv.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/video_render_frames.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/h264/sps_vui_rewriter.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/incoming_video_stream.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/h264/h264_bitstream_parser.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/corevideo_frame_buffer.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/video_frame.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/h264/profile_level_id.cc.o
CMakeFiles/WebrtcCommon_VideoCommon_Video.dir/Source/webrtc/common_video/h264/pps_parser.cc.o
CMakeFiles/WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2.dir/Source/webrtc/modules/desktop_capture/differ_vector_sse2.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Format_Conversion.dir/Source/webrtc/modules/audio_coding/codecs/audio_format_conversion.cc.o
CMakeFiles/WebrtcModulesVideo_ProcessingVideo_Processing.dir/Source/webrtc/modules/video_processing/video_decimator.cc.o
CMakeFiles/WebrtcModulesVideo_ProcessingVideo_Processing.dir/Source/webrtc/modules/video_processing/util/denoiser_filter_c.cc.o
CMakeFiles/WebrtcModulesVideo_ProcessingVideo_Processing.dir/Source/webrtc/modules/video_processing/video_processing_impl.cc.o
CMakeFiles/WebrtcModulesVideo_ProcessingVideo_Processing.dir/Source/webrtc/modules/video_processing/util/noise_estimation.cc.o
CMakeFiles/WebrtcModulesVideo_ProcessingVideo_Processing.dir/Source/webrtc/modules/video_processing/spatial_resampler.cc.o
CMakeFiles/WebrtcModulesVideo_ProcessingVideo_Processing.dir/Source/webrtc/modules/video_processing/util/skin_detection.cc.o
CMakeFiles/WebrtcModulesVideo_ProcessingVideo_Processing.dir/Source/webrtc/modules/video_processing/video_denoiser.cc.o
CMakeFiles/WebrtcModulesVideo_ProcessingVideo_Processing.dir/Source/webrtc/modules/video_processing/frame_preprocessor.cc.o
CMakeFiles/WebrtcModulesVideo_ProcessingVideo_Processing.dir/Source/webrtc/modules/video_processing/util/denoiser_filter.cc.o
CMakeFiles/WebrtcCallCall.dir/Source/webrtc/call/flexfec_receive_stream.cc.o
CMakeFiles/WebrtcCallCall.dir/Source/webrtc/call/bitrate_allocator.cc.o
CMakeFiles/WebrtcCallCall.dir/Source/webrtc/call/call.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/level_controller/noise_spectrum_estimator.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/utility/delay_estimator.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/agc/loudness_histogram.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/vad/voice_activity_detector.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/audio_processing_impl.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/ns/noise_suppression.c.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/aec/aec_core.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/high_pass_filter_impl.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/aec/echo_cancellation.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/noise_suppression_impl.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/vad/pitch_internal.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/vad/standalone_vad.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/level_controller/gain_applier.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/aecm/aecm_core_c.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/agc/agc.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/level_controller/peak_level_estimator.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/transient/wpd_node.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/agc/legacy/analog_agc.c.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/echo_detector/circular_buffer.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/splitting_filter.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/ns/ns_core.c.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/vad/pole_zero_filter.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/utility/block_mean_calculator.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/include/config.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/voice_detection_impl.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/echo_cancellation_impl.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/vad/vad_circular_buffer.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/vad/pitch_based_vad.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/level_controller/level_controller.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/echo_detector/normalized_covariance_estimator.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/agc/utility.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/beamformer/covariance_matrix_generator.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/level_controller/signal_classifier.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/vad/vad_audio_proc.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/level_controller/down_sampler.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/level_controller/noise_level_estimator.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/typing_detection.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/gain_control_impl.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/agc/agc_manager_direct.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/three_band_filter_bank.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/aec/aec_resampler.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/rms_level.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/level_estimator_impl.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/aecm/aecm_core.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/transient/wpd_tree.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/agc/legacy/digital_agc.c.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/audio_buffer.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/residual_echo_detector.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/include/audio_processing.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/utility/ooura_fft.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/utility/delay_estimator_wrapper.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/level_controller/biquad_filter.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/logging/apm_data_dumper.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/vad/gmm.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/beamformer/nonlinear_beamformer.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/transient/transient_detector.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/level_controller/gain_selector.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/transient/moving_moments.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/beamformer/array_util.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/level_controller/saturating_gain_estimator.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/echo_control_mobile_impl.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/echo_detector/mean_variance_estimator.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/aecm/echo_control_mobile.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/transient/transient_suppressor.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing.dir/Source/webrtc/modules/audio_processing/gain_control_for_experimental_agc.cc.o
CMakeFiles/WebrtcModulesVideo_CaptureVideo_Capture.dir/Source/webrtc/modules/video_capture/external/video_capture_external.cc.o
CMakeFiles/WebrtcModulesVideo_CaptureVideo_Capture.dir/Source/webrtc/modules/video_capture/external/device_info_external.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/engine/payload_type_mapper.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/engine/webrtcvideocapturerfactory.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/rtpdataengine.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/sctp/sctpdataengine.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/engine/webrtcvideocapturer.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/engine/webrtcvoiceengine.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/engine/webrtcvideoencoderfactory.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/engine/webrtcmediaengine.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/engine/webrtcvideoengine2.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/videobroadcaster.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/mediaengine.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/videocommon.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/mediaconstants.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/rtputils.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/videoadapter.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/videocapturer.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/engine/simulcast.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/rtpdump.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/engine/videoencodersoftwarefallbackwrapper.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/adaptedvideotracksource.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/streamparams.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/turnutils.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/codec.cc.o
CMakeFiles/WebrtcMediaRtc_Media.dir/Source/webrtc/media/base/videosourcebase.cc.o
CMakeFiles/WebrtcModulesAudio_CodingWebrtc_Opus.dir/Source/webrtc/modules/audio_coding/codecs/opus/audio_encoder_opus.cc.o
CMakeFiles/WebrtcModulesAudio_CodingWebrtc_Opus.dir/Source/webrtc/modules/audio_coding/codecs/opus/opus_interface.c.o
CMakeFiles/WebrtcModulesAudio_CodingWebrtc_Opus.dir/Source/webrtc/modules/audio_coding/codecs/opus/audio_decoder_opus.cc.o
CMakeFiles/WebrtcToolsChart_Proto.dir/gen/webrtc/tools/event_log_visualizer/chart.pb.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAna_Debug_Dump_Proto.dir/gen/webrtc/modules/audio_coding/audio_network_adaptor/debug_dump.pb.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/moduleimpl.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/mucroomconfigtask.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/xmppthread.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/discoitemsquerytask.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/pubsubstateclient.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/xmppsocket.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/saslmechanism.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/xmppengineimpl.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/presencestatus.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/mucroomdiscoverytask.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/xmppstanzaparser.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/hangoutpubsubclient.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/pubsub_task.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/pubsubtasks.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/chatroommoduleimpl.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/jid.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/constants.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/receivetask.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/xmpplogintask.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/presenceouttask.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/mucroomlookuptask.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/xmpppump.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/pingtask.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/xmpptask.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/rostermoduleimpl.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/xmppclient.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/mucroomuniquehangoutidtask.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/iqtask.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/xmppengineimpl_iq.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/presencereceivetask.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/xmppauth.cc.o
CMakeFiles/WebrtcLibjingleXmppRtc_Xmpp.dir/Source/webrtc/libjingle/xmpp/pubsubclient.cc.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCAudioTrack.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCDataChannelConfiguration.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCAudioSource.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCVideoTrack.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCVideoFrame.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCRtpSender.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCConfiguration.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCRtpParameters.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCIceCandidate.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCMediaSource.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCShader.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCOpenGLVideoRenderer.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCMediaStream.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCVideoSource.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCVideoRendererAdapter.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCI420Shader.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCSessionDescription.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCPeerConnection+Stats.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCNSGLVideoView.m.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/avfoundationvideocapturer.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCLegacyStatsReport.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCPeerConnection+DataChannel.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/videotoolboxvideocodecfactory.cc.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCDataChannel.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCPeerConnectionFactory.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCRtpReceiver.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCMediaStreamTrack.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCAVFoundationVideoSource.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCMediaConstraints.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCRtpCodecParameters.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCIceServer.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCRtpEncodingParameters.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Peerconnection_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCPeerConnection.mm.o
CMakeFiles/WebrtcToolsForce_Mic_Volume_Max.dir/Source/webrtc/tools/force_mic_volume_max/force_mic_volume_max.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils.dir/Source/webrtc/modules/audio_processing/test/protobuf_utils.cc.o
CMakeFiles/WebrtcLibjingleXmlliteRtc_Xmllite.dir/Source/webrtc/libjingle/xmllite/xmlparser.cc.o
CMakeFiles/WebrtcLibjingleXmlliteRtc_Xmllite.dir/Source/webrtc/libjingle/xmllite/xmlnsstack.cc.o
CMakeFiles/WebrtcLibjingleXmlliteRtc_Xmllite.dir/Source/webrtc/libjingle/xmllite/qname.cc.o
CMakeFiles/WebrtcLibjingleXmlliteRtc_Xmllite.dir/Source/webrtc/libjingle/xmllite/xmlbuilder.cc.o
CMakeFiles/WebrtcLibjingleXmlliteRtc_Xmllite.dir/Source/webrtc/libjingle/xmllite/xmlelement.cc.o
CMakeFiles/WebrtcLibjingleXmlliteRtc_Xmllite.dir/Source/webrtc/libjingle/xmllite/xmlprinter.cc.o
CMakeFiles/WebrtcLibjingleXmlliteRtc_Xmllite.dir/Source/webrtc/libjingle/xmllite/xmlconstants.cc.o
CMakeFiles/WebrtcLoggingRtc_Event_Log_Parser.dir/Source/webrtc/logging/rtc_event_log/rtc_event_log_parser.cc.o
CMakeFiles/WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer.dir/Source/webrtc/modules/audio_conference_mixer/source/time_scheduler.cc.o
CMakeFiles/WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer.dir/Source/webrtc/modules/audio_conference_mixer/source/audio_conference_mixer_impl.cc.o
CMakeFiles/WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer.dir/Source/webrtc/modules/audio_conference_mixer/source/audio_frame_manipulator.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/tmmbr_help.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/fir.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmb_item.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/vp8_partition_aggregator.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_sender.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/psfb.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/dtmf_queue.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/pli.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_format.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/rrtr.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/rapid_resync_request.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/rpsi.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/sli.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/remb.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_format_vp8.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/app.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/target_bitrate.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_utility.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_receiver_impl.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/rtpfb.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/remote_ntp_time_estimator.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_sender_audio.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/sdes.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_sender.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_format_h264.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/ulpfec_header_reader_writer.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/ulpfec_receiver_impl.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_sender_video.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/forward_error_correction.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/playout_delay_oracle.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbr.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/report_block.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/forward_error_correction_internal.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/dlrr.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_header_extensions.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_format_vp9.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/sender_report.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_packet.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_utility.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/ulpfec_generator.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/flexfec_receiver.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_receiver_audio.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/ssrc_database.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/receive_statistics_impl.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/flexfec_sender.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_receiver_strategy.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_jitter_report.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/common_header.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbn.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/compound_packet.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/nack.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_header_extension.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/time_util.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_payload_registry.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_receiver_video.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_packet_history.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtp_header_parser.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/flexfec_header_reader_writer.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_reports.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/packet_loss_stats.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_receiver.cc.o
CMakeFiles/WebrtcModulesRtp_RtcpRtp_Rtcp.dir/Source/webrtc/modules/rtp_rtcp/source/rtcp_packet/voip_metric.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/fir_filter.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/resample_by_2_internal.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/audio_util.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/get_scaling_square.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/resample_by_2.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/filter_ar.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/audio_converter.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/real_fft.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/refl_coef_to_lpc.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/complex_fft.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/vector_scaling_operations.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/copy_set_operations.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/lpc_to_refl_coef.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/vad/vad_core.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/spl_sqrt_floor.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/spl_sqrt.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/wav_header.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/vad/webrtc_vad.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/min_max_operations.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/sparse_fir_filter.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/vad/vad_sp.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/real_fourier.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/vad/vad_filterbank.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/downsample_fast.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/sqrt_of_one_minus_x_squared.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/channel_buffer.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/audio_ring_buffer.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/resampler/push_sinc_resampler.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/cross_correlation.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/levinson_durbin.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/vad/vad_gmm.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/auto_correlation.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/division_operations.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/ring_buffer.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/window_generator.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/auto_corr_to_refl_coef.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/blocker.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/vad/vad.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/wav_file.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/filter_ma_fast_q12.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/energy.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/randomization_functions.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/ilbc_specific_functions.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/resampler/sinc_resampler.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/fft4g.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/spl_inl.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/splitting_filter.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/get_hanning_window.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/dot_product_with_scale.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/spl_init.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/resample_48khz.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/resampler/push_resampler.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/filter_ar_fast_q12.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/resampler/resampler.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/resample.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/complex_bit_reverse.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/lapped_transform.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/signal_processing/resample_fractional.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio.dir/Source/webrtc/common_audio/real_fourier_ooura.cc.o
CMakeFiles/WebrtcModulesVideo_ProcessingVideo_Processing_Sse2.dir/Source/webrtc/modules/video_processing/util/denoiser_filter_sse2.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Decoder_Interface.dir/Source/webrtc/modules/audio_coding/codecs/legacy_encoded_audio_frame.cc.o
CMakeFiles/WebrtcModulesAudio_CodingAudio_Decoder_Interface.dir/Source/webrtc/modules/audio_coding/codecs/audio_decoder.cc.o
CMakeFiles/WebrtcLoggingRtc_Event_Log_Proto.dir/gen/webrtc/logging/rtc_event_log/rtc_event_log.pb.cc.o
CMakeFiles/WebrtcModulesVideo_CodingWebrtc_H264.dir/Source/webrtc/modules/video_coding/codecs/h264/h264.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/sigslot.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/asyncsocket.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/openssladapter.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/scoped_autorelease_pool.mm.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/cryptstring.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/rtccertificategenerator.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/macutils.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/socketaddress.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/sharedexclusivelock.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/fileutils.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/httpcommon.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/proxydetect.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/socketaddresspair.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/json.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/ifaddrs_converter.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/macconversion.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/unixfilesystem.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/nethelpers.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/task.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/messagedigest.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/network.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/sslstreamadapter.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/opensslstreamadapter.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/helpers.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/physicalsocketserver.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/rtccertificate.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/common.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/messagehandler.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/networkmonitor.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/logsinks.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/httpbase.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/asynctcpsocket.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/virtualsocketserver.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/sslsocketfactory.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/httpserver.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/crc32.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/optionsfile.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/autodetectproxy.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/ipaddress.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/proxyinfo.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/natsocketfactory.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/asyncfile.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/macwindowpicker.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/ssladapter.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/sslfingerprint.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/opensslidentity.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/taskrunner.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/httprequest.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/stream.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/socketstream.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/proxyserver.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/testclient.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/latebindingsymboltable.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/asyncresolverinterface.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/applefilesystem.mm.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/sslidentity.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/signalthread.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/natserver.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/asyncpacketsocket.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/openssldigest.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/messagequeue.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/urlencode.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/macifaddrs_converter.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/asyncinvoker.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/flags.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/diskcache.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/filerotatingstream.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/nattypes.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/httpclient.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/asyncudpsocket.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/pathutils.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/ratelimiter.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/socketadapters.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/transformadapter.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/thread.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/nullsocketserver.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/maccocoathreadhelper.mm.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/taskparent.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/firewallsocketserver.cc.o
CMakeFiles/WebrtcBaseRtc_Base.dir/Source/webrtc/base/socketpool.cc.o
CMakeFiles/WebrtcModulesDesktop_CapturePrimitives.dir/Source/webrtc/modules/desktop_capture/shared_desktop_frame.cc.o
CMakeFiles/WebrtcModulesDesktop_CapturePrimitives.dir/Source/webrtc/modules/desktop_capture/desktop_frame.cc.o
CMakeFiles/WebrtcModulesDesktop_CapturePrimitives.dir/Source/webrtc/modules/desktop_capture/desktop_geometry.cc.o
CMakeFiles/WebrtcModulesDesktop_CapturePrimitives.dir/Source/webrtc/modules/desktop_capture/desktop_region.cc.o
CMakeFiles/Third_PartyOpusOpus_Demo.dir/Source/third_party/opus/src/src/opus_demo.c.o
CMakeFiles/WebrtcApiCall_Api.dir/Source/webrtc/api/call/audio_send_stream.cc.o
CMakeFiles/WebrtcWebrtc.dir/Source/webrtc/build/no_op_function.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/timeutils.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/event_tracer.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/platform_file.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/stringencode.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/platform_thread.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/bufferqueue.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/location.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/base64.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/bytebuffer.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/logging_mac.mm.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/stringutils.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/timestampaligner.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/copyonwritebuffer.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/logging.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/file.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/bitbuffer.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/rate_limiter.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/rate_statistics.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/optional.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/event.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/file_posix.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/checks.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/criticalsection.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/systeminfo.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/thread_checker_impl.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/exp_filter.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/ratetracker.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/random.cc.o
CMakeFiles/WebrtcBaseRtc_Base_Approved.dir/Source/webrtc/base/race_checker.cc.o
CMakeFiles/WebrtcModulesBitrate_ControllerBitrate_Controller.dir/Source/webrtc/modules/bitrate_controller/bitrate_controller_impl.cc.o
CMakeFiles/WebrtcModulesBitrate_ControllerBitrate_Controller.dir/Source/webrtc/modules/bitrate_controller/send_side_bandwidth_estimation.cc.o
CMakeFiles/WebrtcStatsRtc_Stats_Test_Utils.dir/Source/webrtc/stats/test/rtcteststats.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto.dir/gen/webrtc/modules/audio_processing/unittest.pb.cc.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/hp_input.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/decoder_interpolate_lsf.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/ilbc.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/abs_quant_loop.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/encode.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/cb_construct.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/decode.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/refiner.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/nearest_neighbor.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/interpolate_samples.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/my_corr.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/swap_bytes.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/simple_lsf_quant.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/get_sync_seq.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/enh_upsample.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/cb_search_core.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/simple_interpolate_lsf.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/simple_lsf_dequant.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/frame_classify.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/lsp_to_lsf.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/index_conv_dec.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/create_augmented_vec.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/cb_search.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/cb_mem_energy_calc.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/comp_corr.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/poly_to_lsp.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/split_vq.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/lsf_interpolate_to_poly_enc.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/filtered_cb_vecs.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/state_search.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/get_lsp_poly.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/lpc_encode.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/abs_quant.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/audio_decoder_ilbc.cc.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/state_construct.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/lsf_interpolate_to_poly_dec.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/enhancer_interface.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/vq3.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/xcorr_coef.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/chebyshev.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/lsf_to_lsp.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/vq4.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/lsf_check.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/unpack_bits.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/gain_quant.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/bw_expand.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/audio_encoder_ilbc.cc.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/sort_sq.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/hp_output.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/enhancer.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/get_cd_vec.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/simple_lpc_analysis.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/init_encode.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/cb_mem_energy.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/smooth.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/gain_dequant.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/do_plc.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/augmented_cb_corr.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/pack_bits.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/interpolate.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/index_conv_enc.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/energy_inverse.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/poly_to_lsf.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/window32_w32.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/init_decode.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/decode_residual.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/lsf_to_poly.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/cb_update_best_index.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/cb_mem_energy_augmentation.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/smooth_out_data.c.o
CMakeFiles/WebrtcModulesAudio_CodingIlbc.dir/Source/webrtc/modules/audio_coding/codecs/ilbc/constants.c.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto.dir/gen/webrtc/modules/audio_processing/debug.pb.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/cross_correlation.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/dsp_helper.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/post_decode_vad.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/time_stretch.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/audio_decoder_impl.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/background_noise.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/delay_manager.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/statistics_calculator.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/neteq.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/decision_logic.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/dtmf_buffer.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/audio_multi_vector.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/packet.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/nack_tracker.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/merge.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/preemptive_expand.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/tick_timer.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/audio_classifier.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/delay_peak_detector.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/dtmf_tone_generator.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/buffer_level_filter.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/expand.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/sync_buffer.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/normal.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/random_vector.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/packet_buffer.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/timestamp_scaler.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/decision_logic_normal.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/accelerate.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/rtcp.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/comfort_noise.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/audio_vector.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/decision_logic_fax.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/neteq_impl.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/decoder_database.cc.o
CMakeFiles/WebrtcModulesAudio_CodingNeteq.dir/Source/webrtc/modules/audio_coding/neteq/red_payload_splitter.cc.o
CMakeFiles/WebrtcLoggingRtc_Event_Log_Impl.dir/Source/webrtc/logging/rtc_event_log/rtc_event_log_helper_thread.cc.o
CMakeFiles/WebrtcLoggingRtc_Event_Log_Impl.dir/Source/webrtc/logging/rtc_event_log/rtc_event_log.cc.o
CMakeFiles/WebrtcModulesAudio_CodingRed.dir/Source/webrtc/modules/audio_coding/codecs/red/audio_encoder_copy_red.cc.o
CMakeFiles/WebrtcToolsFrame_Analyzer.dir/Source/webrtc/tools/frame_analyzer/frame_analyzer.cc.o
CMakeFiles/WebrtcModulesAudio_DeviceAudio_Device.dir/Source/webrtc/modules/audio_device/mac/audio_device_mac.cc.o
CMakeFiles/WebrtcModulesAudio_DeviceAudio_Device.dir/Source/webrtc/modules/audio_device/mac/portaudio/pa_ringbuffer.c.o
CMakeFiles/WebrtcModulesAudio_DeviceAudio_Device.dir/Source/webrtc/modules/audio_device/audio_device_generic.cc.o
CMakeFiles/WebrtcModulesAudio_DeviceAudio_Device.dir/Source/webrtc/modules/audio_device/fine_audio_buffer.cc.o
CMakeFiles/WebrtcModulesAudio_DeviceAudio_Device.dir/Source/webrtc/modules/audio_device/dummy/audio_device_dummy.cc.o
CMakeFiles/WebrtcModulesAudio_DeviceAudio_Device.dir/Source/webrtc/modules/audio_device/mac/audio_mixer_manager_mac.cc.o
CMakeFiles/WebrtcModulesAudio_DeviceAudio_Device.dir/Source/webrtc/modules/audio_device/audio_device_buffer.cc.o
CMakeFiles/WebrtcModulesAudio_DeviceAudio_Device.dir/Source/webrtc/modules/audio_device/audio_device_impl.cc.o
CMakeFiles/WebrtcModulesAudio_DeviceAudio_Device.dir/Source/webrtc/modules/audio_device/dummy/file_audio_device_factory.cc.o
CMakeFiles/WebrtcModulesAudio_DeviceAudio_Device.dir/Source/webrtc/modules/audio_device/dummy/file_audio_device.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/stubs/structurally_valid.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/message_lite.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/arena.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/stubs/atomicops_internals_x86_gcc.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/stubs/once.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/repeated_field.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/extension_set.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/stubs/bytestream.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/wire_format_lite.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/stubs/status.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/arenastring.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/io/zero_copy_stream.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/io/coded_stream.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/stubs/atomicops_internals_x86_msvc.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/stubs/statusor.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/generated_message_util.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/stubs/stringprintf.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/stubs/time.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/stubs/stringpiece.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/stubs/strutil.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/stubs/common.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Lite.dir/Source/third_party/protobuf/src/google/protobuf/stubs/int128.cc.o
CMakeFiles/WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory.dir/Source/webrtc/modules/audio_coding/codecs/builtin_audio_decoder_factory.cc.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/t_crl.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/ctx.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bio/bio.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/t1_enc.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/evp/evp_ctx.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/ssl_rsa.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/evp/sign.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/tls_record.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/dtls_record.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/pcy_cache.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/i2d_pr.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cmac/cmac.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_set.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_pkey.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_ncons.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/sha/sha256.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ecdsa/ecdsa_asn1.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/rc4/rc4.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/by_file.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/asn1_par.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/rand/windows.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/obj/obj.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/shift.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/modes/ctr.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/newhope/ntt.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cipher/aead.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/rsaz_exp.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/sqrt.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_pubkey.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/s3_lib.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/convert.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/pcy_node.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bio/fd.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/pcy_lib.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/tasn_utl.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_object.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/gcd.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cipher/e_rc4.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/s3_pkt.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_pci.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/d1_srtp.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/tasn_typ.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_pcons.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_x509.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/random.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/curve25519/spake25519.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/md5/md5.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_purp.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bio/buffer.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/buf/buf.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509spki.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_spki.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_trs.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/handshake_client.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_ext.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_crld.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/pem/pem_xaux.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_ia5.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bio/socket.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/pkcs8/p8_pkey.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cipher/e_des.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cpu-intel.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_exten.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/conf/conf.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ec/ec_montgomery.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cipher/e_chacha20poly1305.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_val.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_mbstr.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_req.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/rand/urandom.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/rsa_pss.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_utl.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_bitst.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bio/printf.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/x_bignum.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cipher/derive_key.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_obj.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/mul.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/chacha/chacha.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/pem/pem_pk8.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/poly1305/poly1305_vec.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509name.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/dh/params.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/evp/p_rsa_asn1.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_vfy.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/tls_method.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/dh/dh_asn1.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bio/connect.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_sig.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/tasn_fre.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/modes/cbc.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/refcount_lock.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/div.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_def.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_pmaps.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ec/ec_key.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_print.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/montgomery.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_txt.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ec/p256-64.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/dh/dh.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ec/ec.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/f_enum.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/dtls_method.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/tls13_both.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/bn_asn1.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/evp/evp.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_pcia.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/hkdf/hkdf.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_int.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/pkcs8/pkcs8.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/by_dir.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/dsa/dsa.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ec/p224-64.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/sha/sha1.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/hmac/hmac.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/digest/digest.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_akey.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_utctm.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/thread.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/kronecker.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bytestring/cbs.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cpu-arm-linux.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/evp/p_rsa.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bio/hexdump.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_d2.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/rsa/rsa_asn1.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/rsa/rsa_impl.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/ssl_ecdh.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/ssl_aead_ctx.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509cset.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/pcy_data.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/d1_both.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/tls13_enc.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_int.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_alt.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/time_support.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cipher/e_tls.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_i2d_fp.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/curve25519/curve25519.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ec/wnaf.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/ssl_lib.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/pem/pem_info.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_bitstr.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_time.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/pem/pem_x509.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/evp/digestsign.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cpu-arm.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_lu.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_lib.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/a_strex.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cipher/e_null.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_att.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ecdsa/ecdsa.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_d2i_fp.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509rset.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/ssl_asn1.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/newhope/poly.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/pem/pem_lib.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/mem.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/evp/evp_asn1.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_enum.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/sha/sha512.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/digest/digests.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/aes/key_wrap.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bytestring/cbb.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/asn_pack.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/pcy_map.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cipher/e_aes.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/tasn_dec.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_genn.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/ssl_buffer.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_dup.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/tasn_new.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_gentm.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_akeya.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/asn1_gen.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/evp/p_dsa_asn1.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_attrib.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bytestring/ber.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/newhope/precomp.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_conf.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ecdh/ecdh.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/obj/obj_xref.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/pem/pem_all.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/t1_lib.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/ssl_cert.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/s3_both.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ec/simple.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/err/err.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/s3_enc.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ex_data.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/generic.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_prn.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_bool.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cipher/e_rc2.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/ssl_stat.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_cpols.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_skey.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/t_bitst.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cpu-aarch64-linux.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/pkcs8/p5_pbe.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/base64/base64.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/rand/deterministic.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/modes/gcm.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_req.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/prime.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_x509a.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bio/socket_helper.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_octet.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_r2x.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/evp/pbkdf.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/poly1305/poly1305.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/evp/p_ec_asn1.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/thread_win.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/crypto.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/algorithm.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_enum.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/newhope/reduce.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/asn1_lib.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/f_int.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/modes/cfb.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_pku.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/bn.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/d1_pkt.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_cmp.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/pkcs8/p5_pbev2.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/des/des.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_info.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_extku.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/handshake_server.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/tls13_client.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/tasn_enc.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/rsa/rsa.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/stack/stack.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/newhope/newhope.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/rand/rand.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/a_verify.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cpu-ppc64le.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/exponentiation.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_sxnet.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/err_data.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/x_long.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_strnid.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/aes/aes.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/pem/pem_oth.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/modes/ofb.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/a_digest.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bytestring/asn1_compat.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cipher/cipher.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509type.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_vpm.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/asm/x86_64-gcc.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/dsa/dsa_asn1.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_algor.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cipher/e_ssl3.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bio/file.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/montgomery_inv.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/pcy_tree.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/d1_lib.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509v3/v3_bcons.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/lhash/lhash.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_info.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/rsa/padding.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bio/bio_mem.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/newhope/error_correction.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_name.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/tls13_server.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ec/ec_asn1.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ec/p256-x86_64.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ec/util-64.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/t_x509.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/pkcs7.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/thread_none.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/ec/oct.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/ssl_cipher.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_crl.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/refcount_c11.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/dh/check.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/thread_pthread.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/md4/md4.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/t_x509a.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/cipher/tls_cbc.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/cmp.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x509_v3.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/ssl_file.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/rsa/blinding.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/curve25519/x25519-x86_64.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bio/pair.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/ssl_session.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/pem/pem_pkey.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/bn/add.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/aes/mode_wrappers.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/evp/p_ec.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_utf8.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/x_all.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/a_sign.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/engine/engine.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/a_type.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/asn1/f_string.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/evp/print.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/poly1305/poly1305_arm.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/crypto/x509/t_req.c.o
CMakeFiles/Third_PartyBoringsslBoringssl.dir/Source/third_party/boringssl/src/ssl/custom_extensions.c.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/vie_remb.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/receive_statistics_proxy.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/video_send_stream.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/send_statistics_proxy.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/send_delay_stats.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/video_encoder.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/video_decoder.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/rtp_stream_receiver.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/vie_encoder.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/transport_adapter.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/stream_synchronization.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/video_receive_stream.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/encoder_rtcp_feedback.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/overuse_frame_detector.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/call_stats.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/video_stream_decoder.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/stats_counter.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/report_block_stats.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/payload_router.cc.o
CMakeFiles/WebrtcVideoVideo.dir/Source/webrtc/video/rtp_streams_synchronizer.cc.o
CMakeFiles/WebrtcSdkRtc_Sdk_Common_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCFieldTrials.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Common_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCLogging.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Common_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCSSLAdapter.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Common_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCTracing.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Common_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/NSString+StdString.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Common_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCMetrics.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Common_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCFileLogger.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Common_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCMetricsSampleInfo.mm.o
CMakeFiles/WebrtcSdkRtc_Sdk_Common_Objc.dir/Source/webrtc/sdk/objc/Framework/Classes/RTCDispatcher.m.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing_Sse2.dir/Source/webrtc/modules/audio_processing/aec/aec_core_sse2.cc.o
CMakeFiles/WebrtcModulesAudio_ProcessingAudio_Processing_Sse2.dir/Source/webrtc/modules/audio_processing/utility/ooura_fft_sse2.cc.o
CMakeFiles/WebrtcToolsRgba_To_I420_Converter.dir/Source/webrtc/tools/converter/rgba_to_i420_converter.cc.o
CMakeFiles/WebrtcToolsRgba_To_I420_Converter.dir/Source/webrtc/tools/converter/converter.cc.o
CMakeFiles/WebrtcModulesCongestion_ControllerCongestion_Controller.dir/Source/webrtc/modules/congestion_controller/probe_bitrate_estimator.cc.o
CMakeFiles/WebrtcModulesCongestion_ControllerCongestion_Controller.dir/Source/webrtc/modules/congestion_controller/congestion_controller.cc.o
CMakeFiles/WebrtcModulesCongestion_ControllerCongestion_Controller.dir/Source/webrtc/modules/congestion_controller/probe_controller.cc.o
CMakeFiles/WebrtcModulesCongestion_ControllerCongestion_Controller.dir/Source/webrtc/modules/congestion_controller/delay_based_bwe.cc.o
CMakeFiles/WebrtcModulesCongestion_ControllerCongestion_Controller.dir/Source/webrtc/modules/congestion_controller/transport_feedback_adapter.cc.o
CMakeFiles/WebrtcStatsRtc_Stats.dir/Source/webrtc/stats/rtcstats.cc.o
CMakeFiles/WebrtcStatsRtc_Stats.dir/Source/webrtc/stats/rtcstats_objects.cc.o
CMakeFiles/WebrtcStatsRtc_Stats.dir/Source/webrtc/stats/rtcstatsreport.cc.o
CMakeFiles/WebrtcModulesVideo_CaptureVideo_Capture_Module.dir/Source/webrtc/modules/video_capture/video_capture_impl.cc.o
CMakeFiles/WebrtcModulesVideo_CaptureVideo_Capture_Module.dir/Source/webrtc/modules/video_capture/device_info_impl.cc.o
CMakeFiles/WebrtcModulesVideo_CaptureVideo_Capture_Module.dir/Source/webrtc/modules/video_capture/video_capture_factory.cc.o
CMakeFiles/WebrtcSystem_WrappersMetrics_Default.dir/Source/webrtc/system_wrappers/source/metrics_default.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/source_context.pb.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/structurally_valid.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/message_lite.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/compiler/importer.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/arena.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/proto_writer.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/time_util.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/type_resolver_util.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/message_differencer.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/map_field.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/unknown_field_set.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/field_mask.pb.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/any.pb.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/wire_format_lite.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/field_comparator.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/reflection_ops.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/mathlimits.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/extension_set_heavy.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/error_listener.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/wire_format.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/atomicops_internals_x86_gcc.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/once.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/io/tokenizer.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/repeated_field.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/struct.pb.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/json_escaping.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/io/printer.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/extension_set.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/bytestream.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/atomicops_internals_x86_msvc.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/duration.pb.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/default_value_objectwriter.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/field_mask_util.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/protostream_objectwriter.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/status.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/json_stream_parser.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/json_util.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/descriptor_database.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/wrappers.pb.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/arenastring.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/descriptor.pb.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/utility.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/empty.pb.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/field_mask_utility.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/protostream_objectsource.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/io/zero_copy_stream.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/io/coded_stream.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/text_format.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/object_writer.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/message.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/any.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/api.pb.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/descriptor.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/service.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/statusor.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/dynamic_message.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/type_info_test_helper.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/generated_message_util.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/stringprintf.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/time.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/stringpiece.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/timestamp.pb.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/type.pb.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/substitute.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/datapiece.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/json_objectwriter.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/compiler/parser.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/generated_message_reflection.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/strutil.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/util/internal/type_info.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/io/strtod.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/common.cc.o
CMakeFiles/Third_PartyProtobufProtobuf_Full.dir/Source/third_party/protobuf/src/google/protobuf/stubs/int128.cc.o
CMakeFiles/WebrtcModulesAudio_CodingCng.dir/Source/webrtc/modules/audio_coding/codecs/cng/webrtc_cng.cc.o
CMakeFiles/WebrtcModulesAudio_CodingCng.dir/Source/webrtc/modules/audio_coding/codecs/cng/audio_encoder_cng.cc.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/process_gains_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/laplace.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/stereo_quant_pred.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/lin2log.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/pitch.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/cwrs.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/debug.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/tables_pulses_per_block.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/VAD.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/sort.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/VQ_WMat_EC.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/tables_gain.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/sort_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/src/opus_encoder.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/src/repacketizer.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/k2a_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/inner_prod_aligned.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/modes.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/find_LPC_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/src/mlp.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/stereo_MS_to_LR.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/CNG.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/celt_lpc.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/apply_sine_window_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/init_encoder.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/check_control_input.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/pitch_analysis_core_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/NLSF_unpack.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/encode_frame_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/table_LSF_cos.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/mathops.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/tables_other.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/tables_pitch_lag.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/LPC_analysis_filter_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/init_decoder.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/control_codec.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/process_NLSFs.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/src/opus_multistream_encoder.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/corrMatrix_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/control_SNR.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/quant_bands.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/mdct.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/tables_NLSF_CB_NB_MB.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/stereo_LR_to_MS.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/resampler_private_AR2.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/resampler.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/NLSF_VQ.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/bwexpander_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/shell_coder.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/celt_encoder.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/levinsondurbin_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/warped_autocorrelation_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/vq.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/bwexpander_32.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/resampler_private_up2_HQ.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/entenc.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/resampler_private_down_FIR.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/resampler_down2.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/find_pitch_lags_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/prefilter_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/rate.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/stereo_find_predictor.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/src/analysis.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/LPC_inv_pred_gain.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/decoder_set_fs.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/A2NLSF.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/resampler_down2_3.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/stereo_decode_pred.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/decode_indices.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/celt_decoder.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/resampler_private_IIR_FIR.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/decode_frame.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/tables_NLSF_CB_WB.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/quant_LTP_gains.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/NSQ.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/dec_API.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/ana_filt_bank_1.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/biquad_alt.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/find_pred_coefs_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/NLSF_stabilize.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/autocorrelation_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/NLSF_decode.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/regularize_correlations_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/src/opus.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/NSQ_del_dec.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/find_LTP_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/LPC_inv_pred_gain_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/schur_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/bwexpander.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/sum_sqr_shift.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/interpolate.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/LP_variable_cutoff.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/LTP_analysis_filter_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/encode_pulses.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/src/opus_decoder.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/LTP_scale_ctrl_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/residual_energy_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/scale_vector_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/src/opus_multistream_decoder.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/inner_product_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/decode_pitch.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/noise_shape_analysis_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/bands.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/src/mlp_data.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/decode_pulses.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/burg_modified_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/log2lin.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/NLSF_del_dec_quant.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/solve_LS_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/HP_variable_cutoff.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/energy_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/pitch_est_tables.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/encode_indices.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/entcode.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/stereo_encode_pred.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/NLSF_VQ_weights_laroia.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/kiss_fft.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/sigm_Q15.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/LPC_analysis_filter.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/PLC.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/NLSF_encode.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/resampler_rom.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/NLSF2A.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/code_signs.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/wrappers_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/gain_quant.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/celt.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/float/scale_copy_vector_FLP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/enc_API.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/decode_parameters.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/decode_core.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/tables_LTP.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/celt/entdec.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/src/opus_multistream.c.o
CMakeFiles/Third_PartyOpusOpus.dir/Source/third_party/opus/src/silk/control_audio_bandwidth.c.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio_Sse2.dir/Source/webrtc/common_audio/resampler/sinc_resampler_sse.cc.o
CMakeFiles/WebrtcCommon_AudioCommon_Audio_Sse2.dir/Source/webrtc/common_audio/fir_filter_sse.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding_Utility.dir/Source/webrtc/modules/video_coding/utility/frame_dropper.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding_Utility.dir/Source/webrtc/modules/video_coding/utility/ivf_file_writer.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding_Utility.dir/Source/webrtc/modules/video_coding/utility/vp8_header_parser.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding_Utility.dir/Source/webrtc/modules/video_coding/utility/moving_average.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding_Utility.dir/Source/webrtc/modules/video_coding/utility/qp_parser.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding_Utility.dir/Source/webrtc/modules/video_coding/utility/simulcast_rate_allocator.cc.o
CMakeFiles/WebrtcModulesVideo_CodingVideo_Coding_Utility.dir/Source/webrtc/modules/video_coding/utility/quality_scaler.cc.o
CMakeFiles/WebrtcSdkWebrtc_H264_Video_Toolbox.dir/Source/webrtc/sdk/objc/Framework/Classes/h264_video_toolbox_nalu.cc.o
CMakeFiles/WebrtcSdkWebrtc_H264_Video_Toolbox.dir/Source/webrtc/sdk/objc/Framework/Classes/h264_video_toolbox_decoder.cc.o
CMakeFiles/WebrtcSdkWebrtc_H264_Video_Toolbox.dir/Source/webrtc/sdk/objc/Framework/Classes/h264_video_toolbox_encoder.mm.o
CMakeFiles/Third_PartyJsoncppJsoncpp.dir/Source/third_party/jsoncpp/overrides/src/lib_json/json_reader.cpp.o
CMakeFiles/WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl.dir/Source/webrtc/modules/video_capture/objc/device_info_objc.mm.o
CMakeFiles/WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl.dir/Source/webrtc/modules/video_capture/objc/video_capture.mm.o
CMakeFiles/WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl.dir/Source/webrtc/modules/video_capture/objc/rtc_video_capture_objc.mm.o
CMakeFiles/WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl.dir/Source/webrtc/modules/video_capture/objc/device_info.mm.o
CMakeFiles/Third_PartyOpusOpus_Compare.dir/Source/third_party/opus/src/src/opus_compare.c.o
CMakeFiles/WebrtcModulesUtilityUtility.dir/Source/webrtc/modules/utility/source/coder.cc.o
CMakeFiles/WebrtcModulesUtilityUtility.dir/Source/webrtc/modules/utility/source/audio_frame_operations.cc.o
CMakeFiles/WebrtcModulesUtilityUtility.dir/Source/webrtc/modules/utility/source/process_thread_impl.cc.o
CMakeFiles/WebrtcModulesUtilityUtility.dir/Source/webrtc/modules/utility/source/file_player.cc.o
CMakeFiles/WebrtcModulesUtilityUtility.dir/Source/webrtc/modules/utility/source/file_recorder.cc.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/spectrum_ar_model_tables.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/decode_bwe.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/entropy_coding.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/crc.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/arith_routines.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/pitch_filter.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_gain_swb_tables.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/pitch_estimator.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_shape_swb12_tables.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_shape_swb16_tables.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/transform.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/decode.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/audio_decoder_isac.cc.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/audio_encoder_isac.cc.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/pitch_gain_tables.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/encode_lpc_swb.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/bandwidth_estimator.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/encode.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/lattice.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_analysis.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/filterbanks.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/filter_functions.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/intialize.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/arith_routines_logist.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/arith_routines_hist.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_tables.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/pitch_lag_tables.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/isac.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/fft.c.o
CMakeFiles/WebrtcModulesAudio_CodingIsac.dir/Source/webrtc/modules/audio_coding/codecs/isac/main/source/filterbank_tables.c.o")
add_custom_command(OUTPUT ${LIBWEBRTC_OUTPUT_DIR}/../libwebrtc.a
    COMMAND libtool -static -o ${LIBWEBRTC_OUTPUT_DIR}/../libwebrtc.a -filelist ${LIBWEBRTC_OUTPUT_DIR}/list_libwebrtc_objects
    VERBATIM)
add_custom_target(LIBWEBRTC DEPENDS WebrtcModulesAudio_CodingAudio_Decoder_Interface WebrtcCommon_AudioCommon_Audio_Sse2 WebrtcModulesAudio_ProcessingAudioproc_Debug_Proto WebrtcVoice_EngineLevel_Indicator WebrtcLibjingleXmppRtc_Xmpp WebrtcBaseRtc_Base WebrtcSdkRtc_Sdk_Common_Objc Third_PartyGflagsGflags WebrtcModulesAudio_ProcessingAudioproc_Protobuf_Utils WebrtcModulesAudio_MixerAudio_Frame_Manipulator Third_PartyProtobufProtobuf_Lite Third_PartyExpatExpat WebrtcModulesAudio_CodingAudio_Network_Adaptor WebrtcModulesBitrate_ControllerBitrate_Controller WebrtcCommon_VideoCommon_Video Third_PartyLibyuvCpuid WebrtcCallCall WebrtcModulesUtilityUtility WebrtcLibjingleXmlliteRtc_Xmllite WebrtcToolsVideo_Quality_Analysis WebrtcModulesAudio_CodingAna_Debug_Dump_Proto WebrtcSdkWebrtc_H264_Video_Toolbox WebrtcP2PLibstunprober WebrtcModulesVideo_CodingWebrtc_I420 WebrtcModulesVideo_ProcessingVideo_Processing WebrtcWebrtc WebrtcModulesAudio_CodingNeteq WebrtcModulesAudio_DeviceAudio_Device WebrtcSystem_WrappersSystem_Wrappers WebrtcModulesDesktop_CaptureDesktop_Capture_Differ_Sse2 Third_PartyLibsrtpLibsrtp WebrtcToolsCommand_Line_Parser WebrtcModulesAudio_CodingBuiltin_Audio_Decoder_Factory WebrtcToolsChart_Proto WebrtcModulesMedia_FileMedia_File WebrtcApiLibjingle_Peerconnection WebrtcModulesAudio_CodingAudio_Coding WebrtcLoggingRtc_Event_Log_Parser Third_PartyBoringsslBoringssl_Asm Third_PartyBoringsslBoringssl WebrtcModulesAudio_CodingIsac WebrtcModulesVideo_ProcessingVideo_Processing_Sse2 WebrtcModulesAudio_CodingPcm16B Third_PartyLibyuvConvert WebrtcWebrtc_Common WebrtcToolsForce_Mic_Volume_Max WebrtcPcRtc_Pc WebrtcModulesAudio_CodingCng WebrtcModulesAudio_CodingAudio_Format_Conversion Third_PartyOpusOpus_Compare WebrtcModulesVideo_CodingWebrtc_H264 WebrtcModulesAudio_CodingWebrtc_Opus WebrtcModulesAudio_ProcessingAudioproc_Unittest_Proto WebrtcModulesPacingPacing WebrtcApiCall_Api WebrtcAudioAudio WebrtcModulesAudio_CodingIlbc WebrtcModulesAudio_MixerAudio_Mixer_Impl WebrtcVideoVideo Third_PartyUsrsctpUsrsctp WebrtcModulesAudio_CodingIsac_Common WebrtcModulesVideo_CaptureVideo_Capture_Internal_Impl WebrtcModulesAudio_CodingRent_A_Codec Third_PartyLibyuvPsnr WebrtcCommon_AudioCommon_Audio WebrtcVoice_EngineVoice_Engine WebrtcModulesAudio_CodingNeteq_Unittest_Proto WebrtcModulesAudio_CodingRed Third_PartyProtobufProtoc_Lib WebrtcModulesAudio_CodingIsac_Fix WebrtcModulesDesktop_CaptureDesktop_Capture WebrtcModulesAudio_CodingAna_Config_Proto WebrtcModulesVideo_CaptureVideo_Capture_Module WebrtcToolsRgba_To_I420_Converter Third_PartyLibyuvLibyuv WebrtcBaseRtc_Base_Approved WebrtcModulesDesktop_CapturePrimitives WebrtcSystem_WrappersField_Trial_Default Third_PartyJsoncppJsoncpp WebrtcModulesCongestion_ControllerCongestion_Controller WebrtcLoggingRtc_Event_Log_Impl WebrtcLoggingRtc_Event_Log_Proto Third_PartyOpusOpus Third_PartyOpusOpus_Demo WebrtcSystem_WrappersMetrics_Default WebrtcToolsPsnr_Ssim_Analyzer WebrtcModulesAudio_Conference_MixerAudio_Conference_Mixer WebrtcToolsFrame_Editor Third_PartyProtobufProtoc WebrtcSdkRtc_Sdk_Peerconnection_Objc Third_PartyProtobufProtobuf_Full WebrtcToolsFrame_Editing_Lib WebrtcModulesVideo_CodingVideo_Coding WebrtcToolsFrame_Analyzer WebrtcP2PRtc_P2P WebrtcModulesAudio_ProcessingAudio_Processing WebrtcBaseRtc_Task_Queue WebrtcModulesVideo_CaptureVideo_Capture WebrtcModulesRtp_RtcpRtp_Rtcp WebrtcStatsRtc_Stats Third_PartyLibyuvCompare WebrtcModulesRemote_Bitrate_EstimatorRemote_Bitrate_Estimator WebrtcModulesAudio_ProcessingAudio_Processing_Sse2 WebrtcModulesAudio_CodingAudio_Format WebrtcStatsRtc_Stats_Test_Utils WebrtcMediaRtc_Media WebrtcModulesVideo_CodingVideo_Coding_Utility WebrtcToolsEvent_Log_Visualizer_Utils WebrtcModulesAudio_CodingAudio_Encoder_Interface ${LIBWEBRTC_OUTPUT_DIR}/../libwebrtc.a)
# End of target LIBWEBRTC

set_target_properties(WebrtcBaseGtest_Prod PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(WebrtcLoggingRtc_Event_Log_Api PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(Third_PartyBoringsslBoringssl_Asm PROPERTIES LINKER_LANGUAGE CXX)