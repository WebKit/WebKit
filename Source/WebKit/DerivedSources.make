# Copyright (C) 2010-2023 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

VPATH = \
    $(WebKit2) \
    $(WebKit2)/GPUProcess \
    $(WebKit2)/GPUProcess/graphics \
    $(WebKit2)/GPUProcess/mac \
    $(WebKit2)/GPUProcess/media \
    $(WebKit2)/GPUProcess/media/ios \
    $(WebKit2)/GPUProcess/webrtc \
    $(WebKit2)/ModelProcess \
    $(WebKit2)/NetworkProcess \
    $(WebKit2)/NetworkProcess/Cookies \
    $(WebKit2)/NetworkProcess/cache \
    $(WebKit2)/NetworkProcess/CustomProtocols \
    $(WebKit2)/NetworkProcess/mac \
    $(WebKit2)/NetworkProcess/webrtc \
    $(WebKit2)/NetworkProcess/IndexedDB \
    $(WebKit2)/NetworkProcess/ServiceWorker \
    $(WebKit2)/NetworkProcess/WebStorage \
    $(WebKit2)/NetworkProcess/storage \
    $(WebKit2)/Platform \
    $(WebKit2)/Resources/SandboxProfiles/ios \
    $(WebKit2)/Shared \
    $(WebKit2)/Shared/API/Cocoa \
    $(WebKit2)/Shared/ApplePay \
    $(WebKit2)/Shared/Authentication \
    $(WebKit2)/Shared/mac \
    $(WebKit2)/Shared/Notifications \
    $(WebKit2)/WebProcess/ApplePay \
    $(WebKit2)/WebProcess/ApplicationCache \
    $(WebKit2)/WebProcess/Automation \
    $(WebKit2)/WebProcess/Cache \
    $(WebKit2)/WebProcess/Databases/IndexedDB \
    $(WebKit2)/WebProcess/Extensions/Interfaces \
    $(WebKit2)/WebProcess/FullScreen \
    $(WebKit2)/WebProcess/Geolocation \
    $(WebKit2)/WebProcess/GPU \
    $(WebKit2)/WebProcess/GPU/graphics \
    $(WebKit2)/WebProcess/GPU/media \
    $(WebKit2)/WebProcess/GPU/media/ios \
    $(WebKit2)/WebProcess/GPU/webrtc \
    $(WebKit2)/WebProcess/IconDatabase \
    $(WebKit2)/WebProcess/Inspector \
    $(WebKit2)/WebProcess/MediaCache \
    $(WebKit2)/WebProcess/MediaSession \
    $(WebKit2)/WebProcess/MediaStream \
    $(WebKit2)/WebProcess/Model \
    $(WebKit2)/WebProcess/Network \
    $(WebKit2)/WebProcess/Network/webrtc \
    $(WebKit2)/WebProcess/Notifications \
    $(WebKit2)/WebProcess/OriginData \
    $(WebKit2)/WebProcess/Plugins \
    $(WebKit2)/WebProcess/ResourceCache \
	$(WebKit2)/WebProcess/Speech \
    $(WebKit2)/WebProcess/Storage \
    $(WebKit2)/WebProcess/UserContent \
    $(WebKit2)/WebProcess/WebAuthentication \
    $(WebKit2)/WebProcess/WebCoreSupport \
    $(WebKit2)/WebProcess/WebPage \
    $(WebKit2)/WebProcess/WebPage/Cocoa \
    $(WebKit2)/WebProcess/WebPage/RemoteLayerTree \
    $(WebKit2)/WebProcess/WebStorage \
    $(WebKit2)/WebProcess/cocoa \
    $(WebKit2)/WebProcess/ios \
    $(WebKit2)/WebProcess \
    $(WebKit2)/UIProcess \
    $(WebKit2)/UIProcess/Automation \
    $(WebKit2)/UIProcess/Cocoa \
    $(WebKit2)/UIProcess/Databases \
    $(WebKit2)/UIProcess/Downloads \
    $(WebKit2)/UIProcess/GPU \
    $(WebKit2)/UIProcess/Inspector \
    $(WebKit2)/UIProcess/Inspector/Agents \
    $(WebKit2)/UIProcess/Media \
    $(WebKit2)/UIProcess/Media/cocoa \
    $(WebKit2)/UIProcess/MediaStream \
    $(WebKit2)/UIProcess/Model \
    $(WebKit2)/UIProcess/Network \
    $(WebKit2)/UIProcess/Network/CustomProtocols \
    $(WebKit2)/UIProcess/Notifications \
    $(WebKit2)/UIProcess/Plugins \
    $(WebKit2)/UIProcess/RemoteLayerTree \
    $(WebKit2)/UIProcess/Storage \
    $(WebKit2)/UIProcess/UserContent \
    $(WebKit2)/UIProcess/WebAuthentication \
    $(WebKit2)/UIProcess/mac \
    $(WebKit2)/UIProcess/ios \
    $(WebKit2)/webpushd \
    $(WebKit2)/webpushd/mac \
    $(WEBKITADDITIONS_HEADER_SEARCH_PATHS) \
#

# Workaround for rdar://84212106.
find_tool = $(realpath $(shell xcrun --sdk $(SDK_NAME) -f $(1)))

PYTHON := $(call find_tool,python3)
PERL := perl
RUBY := ruby

ifeq ($(OS),Windows_NT)
    DELETE = cmd //C del
else
    DELETE = rm -f
endif

MESSAGE_RECEIVERS = \
	NetworkProcess/NetworkBroadcastChannelRegistry \
	NetworkProcess/NetworkConnectionToWebProcess \
	NetworkProcess/NetworkContentRuleListManager \
	NetworkProcess/CustomProtocols/LegacyCustomProtocolManager \
	NetworkProcess/NetworkSocketChannel \
	NetworkProcess/ServiceWorker/WebSWServerConnection \
	NetworkProcess/ServiceWorker/ServiceWorkerDownloadTask \
	NetworkProcess/ServiceWorker/ServiceWorkerFetchTask \
	NetworkProcess/ServiceWorker/WebSWServerToContextConnection \
	NetworkProcess/SharedWorker/WebSharedWorkerServerConnection \
	NetworkProcess/SharedWorker/WebSharedWorkerServerToContextConnection \
	NetworkProcess/NetworkProcess \
	NetworkProcess/NetworkResourceLoader \
	NetworkProcess/webrtc/NetworkMDNSRegister \
	NetworkProcess/webrtc/NetworkRTCProvider \
	NetworkProcess/webrtc/NetworkRTCMonitor \
	NetworkProcess/webrtc/RTCDataChannelRemoteManagerProxy \
	NetworkProcess/webtransport/NetworkTransportSession \
	NetworkProcess/Cookies/WebCookieManager \
	NetworkProcess/storage/NetworkStorageManager \
	Shared/AuxiliaryProcess \
	Shared/API/Cocoa/RemoteObjectRegistry \
	Shared/ApplePay/WebPaymentCoordinatorProxy \
	Shared/Authentication/AuthenticationManager \
	Shared/Notifications/NotificationManagerMessageHandler \
	Shared/IPCConnectionTester \
	Shared/IPCStreamTester \
	Shared/IPCStreamTesterProxy \
	Shared/IPCTester \
	Shared/IPCTesterReceiver \
	UIProcess/WebFullScreenManagerProxy \
	UIProcess/RemoteLayerTree/RemoteLayerTreeDrawingAreaProxy \
	UIProcess/GPU/GPUProcessProxy \
	UIProcess/WebAuthentication/WebAuthenticatorCoordinatorProxy \
	UIProcess/WebPasteboardProxy \
	UIProcess/UserContent/WebUserContentControllerProxy \
	UIProcess/Inspector/WebInspectorUIProxy \
	UIProcess/Inspector/RemoteWebInspectorUIProxy \
	UIProcess/Inspector/WebInspectorUIExtensionControllerProxy \
	UIProcess/DrawingAreaProxy \
	UIProcess/Network/NetworkProcessProxy \
	UIProcess/Network/CustomProtocols/LegacyCustomProtocolManagerProxy \
	UIProcess/WebPageProxy \
	UIProcess/VisitedLinkStore \
	UIProcess/ios/WebDeviceOrientationUpdateProviderProxy \
	UIProcess/ios/SmartMagnificationController \
	UIProcess/mac/SecItemShimProxy \
	UIProcess/WebGeolocationManagerProxy \
	UIProcess/WebLockRegistryProxy \
	UIProcess/WebPermissionControllerProxy \
	UIProcess/Cocoa/PlaybackSessionManagerProxy \
	UIProcess/Cocoa/UserMediaCaptureManagerProxy \
	UIProcess/Cocoa/VideoPresentationManagerProxy \
	UIProcess/ViewGestureController \
	UIProcess/WebProcessProxy \
	UIProcess/Automation/WebAutomationSession \
	UIProcess/WebProcessPool \
	UIProcess/WebScreenOrientationManagerProxy \
	UIProcess/Downloads/DownloadProxy \
	UIProcess/Extensions/WebExtensionContext \
	UIProcess/Extensions/WebExtensionController \
	UIProcess/Media/AudioSessionRoutingArbitratorProxy \
	UIProcess/Media/RemoteMediaSessionCoordinatorProxy \
	UIProcess/Model/ModelProcessProxy \
	UIProcess/SpeechRecognitionRemoteRealtimeMediaSourceManager \
	UIProcess/SpeechRecognitionServer \
	UIProcess/XR/PlatformXRSystem \
	WebProcess/Databases/IndexedDB/WebIDBConnectionToServer \
	WebProcess/Extensions/WebExtensionContextProxy \
	WebProcess/Extensions/WebExtensionControllerProxy \
	WebProcess/GPU/GPUProcessConnection \
	WebProcess/GPU/graphics/RemoteImageBufferProxy \
	WebProcess/GPU/graphics/RemoteImageBufferSetProxy \
	WebProcess/GPU/graphics/RemoteRenderingBackendProxy \
	WebProcess/GPU/graphics/RemoteGraphicsContextGLProxy \
	WebProcess/GPU/graphics/WebGPU/RemoteGPUProxy \
	WebProcess/GPU/webrtc/LibWebRTCCodecs \
	WebProcess/GPU/webrtc/SampleBufferDisplayLayer \
	WebProcess/GPU/media/MediaPlayerPrivateRemote \
	WebProcess/GPU/media/MediaSourcePrivateRemoteMessageReceiver \
	WebProcess/GPU/media/RemoteAudioHardwareListener \
	WebProcess/GPU/media/RemoteAudioSession \
	WebProcess/GPU/media/RemoteAudioSourceProviderManager \
	WebProcess/GPU/media/RemoteCDMInstance \
	WebProcess/GPU/media/RemoteCDMInstanceSession \
	WebProcess/GPU/media/RemoteImageDecoderAVFManager \
	WebProcess/GPU/media/RemoteLegacyCDMSession \
	WebProcess/GPU/media/RemoteRemoteCommandListener \
	WebProcess/GPU/media/SourceBufferPrivateRemoteMessageReceiver \
	WebProcess/GPU/media/ios/RemoteMediaSessionHelper \
	WebProcess/GPU/webrtc/RemoteVideoFrameObjectHeapProxyProcessor \
	WebProcess/WebStorage/StorageAreaMap \
	WebProcess/UserContent/WebUserContentController \
	WebProcess/Inspector/WebInspectorInterruptDispatcher \
	WebProcess/Inspector/WebInspectorUI \
	WebProcess/Inspector/WebInspectorUIExtensionController \
	WebProcess/Inspector/WebInspector \
	WebProcess/Inspector/RemoteWebInspectorUI \
	WebProcess/MediaSession/RemoteMediaSessionCoordinator \
	WebProcess/Model/ModelProcessConnection \
	WebProcess/Model/ModelProcessModelPlayer \
	WebProcess/Network/WebSocketChannel \
	WebProcess/Network/NetworkProcessConnection \
	WebProcess/Network/WebResourceLoader \
	WebProcess/Network/WebTransportSession \
	WebProcess/Network/webrtc/LibWebRTCNetwork \
	WebProcess/Network/webrtc/RTCDataChannelRemoteManager \
	WebProcess/Network/webrtc/WebRTCMonitor \
	WebProcess/Network/webrtc/WebRTCResolver \
	WebProcess/WebCoreSupport/RemoteWebLockRegistry \
	WebProcess/WebCoreSupport/WebBroadcastChannelRegistry \
	WebProcess/WebCoreSupport/WebDeviceOrientationUpdateProvider \
	WebProcess/WebCoreSupport/WebFileSystemStorageConnection \
	WebProcess/WebCoreSupport/WebPermissionController \
	WebProcess/WebCoreSupport/WebScreenOrientationManager \
	WebProcess/WebCoreSupport/WebSpeechRecognitionConnection \
	WebProcess/Speech/SpeechRecognitionRealtimeMediaSourceManager \
	WebProcess/Storage/WebSharedWorkerContextManagerConnection \
	WebProcess/Storage/WebSharedWorkerObjectConnection \
	WebProcess/Storage/WebSWContextManagerConnection \
	WebProcess/Storage/WebSWClientConnection \
	WebProcess/WebProcess \
	WebProcess/cocoa/PlaybackSessionManager \
	WebProcess/cocoa/RemoteCaptureSampleManager \
	WebProcess/cocoa/UserMediaCaptureManager \
	WebProcess/cocoa/VideoPresentationManager \
	WebProcess/Geolocation/WebGeolocationManager \
	WebProcess/Automation/WebAutomationSessionProxy \
	WebProcess/FullScreen/WebFullScreenManager \
	WebProcess/ApplePay/WebPaymentCoordinator \
	WebProcess/Notifications/WebNotificationManager \
	WebProcess/WebPage/EventDispatcher \
	WebProcess/WebPage/RemoteLayerTree/RemoteScrollingCoordinator \
	WebProcess/WebPage/ViewGestureGeometryCollector \
	WebProcess/WebPage/DrawingArea \
	WebProcess/WebPage/WebPage \
	WebProcess/WebPage/WebPageTesting \
	WebProcess/WebPage/VisitedLinkTableController \
	WebProcess/WebPage/Cocoa/TextCheckingControllerProxy \
	WebProcess/WebPage/ViewUpdateDispatcher \
	WebProcess/XR/PlatformXRSystemProxy \
	GPUProcess/GPUConnectionToWebProcess \
	GPUProcess/RemoteSharedResourceCache \
	GPUProcess/ShapeDetection/RemoteBarcodeDetector \
	GPUProcess/ShapeDetection/RemoteFaceDetector \
	GPUProcess/ShapeDetection/RemoteTextDetector \
	GPUProcess/graphics/RemoteDisplayListRecorder \
	GPUProcess/graphics/RemoteImageBuffer \
	GPUProcess/graphics/RemoteImageBufferSet \
	GPUProcess/graphics/RemoteRenderingBackend \
	GPUProcess/graphics/RemoteGraphicsContextGL \
	GPUProcess/graphics/WebGPU/RemoteAdapter \
	GPUProcess/graphics/WebGPU/RemoteBindGroup \
	GPUProcess/graphics/WebGPU/RemoteBindGroupLayout \
	GPUProcess/graphics/WebGPU/RemoteBuffer \
	GPUProcess/graphics/WebGPU/RemoteCommandBuffer \
	GPUProcess/graphics/WebGPU/RemoteCommandEncoder \
	GPUProcess/graphics/WebGPU/RemoteCompositorIntegration \
	GPUProcess/graphics/WebGPU/RemoteComputePassEncoder \
	GPUProcess/graphics/WebGPU/RemoteComputePipeline \
	GPUProcess/graphics/WebGPU/RemoteDevice \
	GPUProcess/graphics/WebGPU/RemoteExternalTexture \
	GPUProcess/graphics/WebGPU/RemoteGPU \
	GPUProcess/graphics/WebGPU/RemotePipelineLayout \
	GPUProcess/graphics/WebGPU/RemotePresentationContext \
	GPUProcess/graphics/WebGPU/RemoteQuerySet \
	GPUProcess/graphics/WebGPU/RemoteQueue \
	GPUProcess/graphics/WebGPU/RemoteRenderBundle \
	GPUProcess/graphics/WebGPU/RemoteRenderBundleEncoder \
	GPUProcess/graphics/WebGPU/RemoteRenderPassEncoder \
	GPUProcess/graphics/WebGPU/RemoteRenderPipeline \
	GPUProcess/graphics/WebGPU/RemoteSampler \
	GPUProcess/graphics/WebGPU/RemoteShaderModule \
	GPUProcess/graphics/WebGPU/RemoteTexture \
	GPUProcess/graphics/WebGPU/RemoteTextureView \
	GPUProcess/graphics/WebGPU/RemoteXRBinding \
	GPUProcess/graphics/WebGPU/RemoteXRProjectionLayer \
	GPUProcess/graphics/WebGPU/RemoteXRSubImage \
	GPUProcess/graphics/WebGPU/RemoteXRView \
	GPUProcess/webrtc/LibWebRTCCodecsProxy \
	GPUProcess/webrtc/RemoteSampleBufferDisplayLayerManager \
	GPUProcess/webrtc/RemoteMediaRecorderManager \
	GPUProcess/webrtc/RemoteSampleBufferDisplayLayer \
	GPUProcess/webrtc/RemoteMediaRecorder \
	GPUProcess/webrtc/RemoteAudioMediaStreamTrackRendererInternalUnitManager \
	GPUProcess/GPUProcess \
	GPUProcess/media/RemoteImageDecoderAVFProxy \
	GPUProcess/media/RemoteLegacyCDMSessionProxy \
	GPUProcess/media/RemoteLegacyCDMFactoryProxy \
	GPUProcess/media/RemoteAudioSessionProxy \
	GPUProcess/media/RemoteCDMInstanceSessionProxy \
	GPUProcess/media/RemoteCDMProxy \
	GPUProcess/media/ios/RemoteMediaSessionHelperProxy \
	GPUProcess/media/RemoteAudioDestinationManager \
	GPUProcess/media/RemoteCDMFactoryProxy \
	GPUProcess/media/RemoteCDMInstanceProxy \
	GPUProcess/media/RemoteLegacyCDMProxy \
	GPUProcess/media/RemoteMediaEngineConfigurationFactoryProxy \
	GPUProcess/media/RemoteMediaPlayerManagerProxy \
	GPUProcess/media/RemoteMediaPlayerProxy \
	GPUProcess/media/RemoteMediaResourceManager \
	GPUProcess/media/RemoteVideoFrameObjectHeap \
	GPUProcess/media/RemoteMediaSourceProxy \
	GPUProcess/media/RemoteRemoteCommandListenerProxy \
	GPUProcess/media/RemoteSourceBufferProxy \
	ModelProcess/ModelConnectionToWebProcess \
	ModelProcess/ModelProcess \
	ModelProcess/ModelProcessModelPlayerManagerProxy \
	ModelProcess/cocoa/ModelProcessModelPlayerProxy \
	webpushd/PushClientConnection \
#

GENERATE_MESSAGE_RECEIVER_SCRIPT = $(WebKit2)/Scripts/generate-message-receiver.py
GENERATE_MESSAGE_RECEIVER_SCRIPTS = \
    $(GENERATE_MESSAGE_RECEIVER_SCRIPT) \
    $(WebKit2)/Scripts/webkit/__init__.py \
    $(WebKit2)/Scripts/webkit/messages.py \
    $(WebKit2)/Scripts/webkit/model.py \
    $(WebKit2)/Scripts/webkit/parser.py \
    $(WebKit2)/DerivedSources.make \
#

ifneq ($(SDKROOT),)
	SDK_FLAGS=-isysroot $(SDKROOT)
endif

ifeq ($(ENABLE_ADDRESS_SANITIZER),YES)
	SANITIZE_FLAGS = -fsanitize=address
endif

WK_CURRENT_ARCH=$(word 1, $(ARCHS))
TARGET_TRIPLE_FLAGS=-target $(WK_CURRENT_ARCH)-$(LLVM_TARGET_TRIPLE_VENDOR)-$(LLVM_TARGET_TRIPLE_OS_VERSION)$(LLVM_TARGET_TRIPLE_SUFFIX)

FRAMEWORK_FLAGS := $(addprefix -F, $(BUILT_PRODUCTS_DIR) $(FRAMEWORK_SEARCH_PATHS) $(SYSTEM_FRAMEWORK_SEARCH_PATHS))
HEADER_FLAGS := $(addprefix -I, $(BUILT_PRODUCTS_DIR) $(HEADER_SEARCH_PATHS) $(SYSTEM_HEADER_SEARCH_PATHS))
EXTERNAL_FLAGS := -DRELEASE_WITHOUT_OPTIMIZATIONS $(addprefix -D, $(GCC_PREPROCESSOR_DEFINITIONS))

platform_h_compiler_command = $(CC) -std=c++2b -x c++ $(1) $(SANITIZE_FLAGS) $(SDK_FLAGS) $(TARGET_TRIPLE_FLAGS) $(FRAMEWORK_FLAGS) $(HEADER_FLAGS) $(EXTERNAL_FLAGS) -include "wtf/Platform.h" /dev/null

FEATURE_AND_PLATFORM_FLAGS := $(shell $(call platform_h_compiler_command,-E -P -dM) | $(PERL) -ne "print if s/\#define ((?:HAVE_|USE_|ENABLE_|WTF_PLATFORM_)\w+) (1|0)/\1=\2/")
FEATURE_AND_PLATFORM_DEFINES := $(patsubst %=1, %, $(filter %=1, $(FEATURE_AND_PLATFORM_FLAGS)))
FEATURE_AND_PLATFORM_UNDEFINES := $(patsubst %=0, %, $(filter %=0, $(FEATURE_AND_PLATFORM_FLAGS)))

PLATFORM_HEADER_DIR := $(realpath $(BUILT_PRODUCTS_DIR)$(WK_LIBRARY_HEADERS_FOLDER_PATH))
PLATFORM_HEADER_DEPENDENCIES := $(filter $(PLATFORM_HEADER_DIR)/%,$(realpath $(shell $(call platform_h_compiler_command,-M) | $(PERL) -e "local \$$/; my (\$$target, \$$deps) = split(/:/, <>); print split(/\\\\/, \$$deps);")))
FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES = $(WebKit2)/DerivedSources.make $(PLATFORM_HEADER_DEPENDENCIES)

MESSAGE_RECEIVER_FILES := $(addsuffix MessageReceiver.cpp,$(notdir $(MESSAGE_RECEIVERS)))
MESSAGES_FILES := $(addsuffix Messages.h,$(notdir $(MESSAGE_RECEIVERS)))

GENERATED_MESSAGES_FILES := $(MESSAGE_RECEIVER_FILES) $(MESSAGES_FILES) MessageNames.h MessageNames.cpp MessageArgumentDescriptions.cpp
GENERATED_MESSAGES_FILES_AS_PATTERNS := $(subst .,%,$(GENERATED_MESSAGES_FILES))

MESSAGES_IN_FILES := $(addsuffix .messages.in,$(MESSAGE_RECEIVERS))

SANDBOX_IMPORT_DIR=$(SDKROOT)/usr/local/share/sandbox/profiles/embedded/imports

.PHONY : all

all : $(GENERATED_MESSAGES_FILES)

$(GENERATED_MESSAGES_FILES_AS_PATTERNS) : $(MESSAGES_IN_FILES) $(GENERATE_MESSAGE_RECEIVER_SCRIPTS)
	$(PYTHON) $(GENERATE_MESSAGE_RECEIVER_SCRIPT) $(WebKit2) $(MESSAGE_RECEIVERS)

TEXT_PREPROCESSOR_FLAGS=-E -P -w

ifeq ($(USE_SYSTEM_CONTENT_PATH),YES)
	SANDBOX_DEFINES = -DUSE_SYSTEM_CONTENT_PATH=1 -DSYSTEM_CONTENT_PATH=$(SYSTEM_CONTENT_PATH)
endif

SANDBOX_PROFILES_WITHOUT_WEBPUSHD = \
	com.apple.WebProcess.sb \
	com.apple.WebKit.NetworkProcess.sb \
	com.apple.WebKit.GPUProcess.sb

ifeq ($(WK_RELOCATABLE_WEBPUSHD),YES)
WEBPUSHD_SANDBOX_PROFILE = \
	com.apple.WebKit.webpushd.relocatable.mac.sb
else
WEBPUSHD_SANDBOX_PROFILE = \
	com.apple.WebKit.webpushd.mac.sb
endif

SANDBOX_PROFILES_IOS = \
	com.apple.WebKit.adattributiond.sb \
	com.apple.WebKit.webpushd.sb \
	com.apple.WebKit.GPU.sb \
	com.apple.WebKit.Model.sb \
	com.apple.WebKit.Networking.sb \
	com.apple.WebKit.WebContent.sb

sandbox-profiles-ios : $(SANDBOX_PROFILES_IOS)

all : $(SANDBOX_PROFILES_WITHOUT_WEBPUSHD) $(WEBPUSHD_SANDBOX_PROFILE) $(SANDBOX_PROFILES_IOS)

NOTIFICATION_ALLOW_LISTS = \
	Resources/cocoa/NotificationAllowList/EmbeddedForwardedNotifications.def \
	Resources/cocoa/NotificationAllowList/ForwardedNotifications.def \
	Resources/cocoa/NotificationAllowList/MacForwardedNotifications.def \
	Resources/cocoa/NotificationAllowList/NonForwardedNotifications.def

%.sb : %.sb.in $(NOTIFICATION_ALLOW_LISTS)
	@echo Pre-processing $* sandbox profile...
	grep -o '^[^;]*' $< | $(CC) $(SANITIZE_FLAGS) $(SDK_FLAGS) $(TARGET_TRIPLE_FLAGS) $(SANDBOX_DEFINES) $(TEXT_PREPROCESSOR_FLAGS) $(FRAMEWORK_FLAGS) $(HEADER_FLAGS) $(EXTERNAL_FLAGS) -include "wtf/Platform.h" - > $@.tmp
	$(WebKit2)/Scripts/compile-sandbox.sh $@.tmp $* $(SDK_NAME) $(SANDBOX_IMPORT_DIR)
	mv $@.tmp $@

AUTOMATION_PROTOCOL_GENERATOR_SCRIPTS = \
	$(JavaScriptCore_SCRIPTS_DIR)/cpp_generator_templates.py \
	$(JavaScriptCore_SCRIPTS_DIR)/cpp_generator.py \
	$(JavaScriptCore_SCRIPTS_DIR)/generate_cpp_backend_dispatcher_header.py \
	$(JavaScriptCore_SCRIPTS_DIR)/generate_cpp_backend_dispatcher_implementation.py \
	$(JavaScriptCore_SCRIPTS_DIR)/generate_cpp_frontend_dispatcher_header.py \
	$(JavaScriptCore_SCRIPTS_DIR)/generate_cpp_frontend_dispatcher_implementation.py \
	$(JavaScriptCore_SCRIPTS_DIR)/generate_cpp_protocol_types_header.py \
	$(JavaScriptCore_SCRIPTS_DIR)/generate_cpp_protocol_types_implementation.py \
	$(JavaScriptCore_SCRIPTS_DIR)/generator_templates.py \
	$(JavaScriptCore_SCRIPTS_DIR)/generator.py \
	$(JavaScriptCore_SCRIPTS_DIR)/models.py \
	$(JavaScriptCore_SCRIPTS_DIR)/generate-inspector-protocol-bindings.py \
#

AUTOMATION_PROTOCOL_INPUT_FILES = \
    $(WebKit2)/UIProcess/Automation/Automation.json \
#

AUTOMATION_PROTOCOL_OUTPUT_FILES = \
    AutomationBackendDispatchers.h \
    AutomationBackendDispatchers.cpp \
    AutomationFrontendDispatchers.h \
    AutomationFrontendDispatchers.cpp \
    AutomationProtocolObjects.h \
    AutomationProtocolObjects.cpp \
#
AUTOMATION_PROTOCOL_OUTPUT_PATTERNS = $(subst .,%,$(AUTOMATION_PROTOCOL_OUTPUT_FILES))

# JSON-RPC Frontend Dispatchers, Backend Dispatchers, Type Builders
$(AUTOMATION_PROTOCOL_OUTPUT_PATTERNS) : $(AUTOMATION_PROTOCOL_INPUT_FILES) $(AUTOMATION_PROTOCOL_GENERATOR_SCRIPTS)
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/generate-inspector-protocol-bindings.py --framework WebKit --backend --outputDir . $(AUTOMATION_PROTOCOL_INPUT_FILES)

all : $(AUTOMATION_PROTOCOL_OUTPUT_FILES)

%ScriptSource.h : %.js $(JavaScriptCore_SCRIPTS_DIR)/jsmin.py $(JavaScriptCore_SCRIPTS_DIR)/xxd.pl
	echo "//# sourceURL=__InjectedScript_$(notdir $<)" > $(basename $(notdir $<)).min.js
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/jsmin.py < $< >> $(basename $(notdir $<)).min.js
	$(PERL) $(JavaScriptCore_SCRIPTS_DIR)/xxd.pl $(basename $(notdir $<))ScriptSource $(basename $(notdir $<)).min.js $@
	$(DELETE) $(basename $(notdir $<)).min.js

all : WebAutomationSessionProxyScriptSource.h

# WebPreferences generation

WEB_PREFERENCES = \
    $(WTF_BUILD_SCRIPTS_DIR)/Preferences/UnifiedWebPreferences.yaml \
    $(ADDITIONAL_WEB_PREFERENCES_INPUT_FILES) \
#

WEB_PREFERENCES_TEMPLATES = \
    $(WebKit2)/Scripts/PreferencesTemplates/SharedPreferencesForWebProcess.h.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/SharedPreferencesForWebProcess.cpp.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/SharedPreferencesForWebProcess.serialization.in.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPageUpdatePreferences.cpp.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPreferencesDefinitions.h.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPreferencesFeatures.cpp.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPreferencesGetterSetters.cpp.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPreferencesKeys.cpp.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPreferencesKeys.h.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPreferencesStoreDefaultsMap.cpp.erb \
#
WEB_PREFERENCES_FILES = $(basename $(notdir $(WEB_PREFERENCES_TEMPLATES)))
WEB_PREFERENCES_PATTERNS = $(subst .cpp,%cpp, $(subst .h,%h, $(subst .in,%in, $(WEB_PREFERENCES_FILES))))

all : $(WEB_PREFERENCES_FILES)

$(WEB_PREFERENCES_PATTERNS) : $(WTF_BUILD_SCRIPTS_DIR)/GeneratePreferences.rb $(WEB_PREFERENCES_TEMPLATES) $(WEB_PREFERENCES)
	$(RUBY) $< --frontend WebKit $(addprefix --template , $(WEB_PREFERENCES_TEMPLATES)) $(WEB_PREFERENCES)

SERIALIZATION_DESCRIPTION_FILES = \
	GPUProcess/GPUProcessCreationParameters.serialization.in \
	GPUProcess/GPUProcessPreferences.serialization.in \
	GPUProcess/GPUProcessSessionParameters.serialization.in \
	GPUProcess/graphics/PathSegment.serialization.in \
	GPUProcess/graphics/RemoteGraphicsContextGLInitializationState.serialization.in \
	GPUProcess/graphics/WebGPU/RemoteGPURequestAdapterResponse.serialization.in \
	GPUProcess/media/AudioTrackPrivateRemoteConfiguration.serialization.in \
	GPUProcess/media/InitializationSegmentInfo.serialization.in \
	GPUProcess/media/MediaDescriptionInfo.serialization.in \
	GPUProcess/media/RemoteMediaPlayerProxyConfiguration.serialization.in \
	GPUProcess/media/TextTrackPrivateRemoteConfiguration.serialization.in \
	GPUProcess/media/TrackPrivateRemoteConfiguration.serialization.in \
	GPUProcess/media/VideoTrackPrivateRemoteConfiguration.serialization.in \
	ModelProcess/ModelProcessCreationParameters.serialization.in \
	NetworkProcess/NetworkProcessCreationParameters.serialization.in \
	NetworkProcess/NetworkResourceLoadParameters.serialization.in \
	NetworkProcess/NetworkSessionCreationParameters.serialization.in \
	NetworkProcess/Classifier/ITPThirdPartyData.serialization.in \
	NetworkProcess/Classifier/ITPThirdPartyDataForSpecificFirstParty.serialization.in \
	NetworkProcess/Classifier/StorageAccessStatus.serialization.in \
	NetworkProcess/PrivateClickMeasurement/PrivateClickMeasurementManagerInterface.serialization.in \
	NetworkProcess/storage/FileSystemStorageError.serialization.in \
	Platform/IPC/ConnectionHandle.serialization.in \
	Platform/IPC/FormDataReference.serialization.in \
	Platform/IPC/IPCEvent.serialization.in \
	Platform/IPC/IPCSemaphore.serialization.in \
	Platform/IPC/MessageFlags.serialization.in \
	Platform/IPC/ObjectIdentifierReference.serialization.in \
	Platform/IPC/SharedBufferReference.serialization.in \
	Platform/IPC/SharedFileHandle.serialization.in \
	Platform/IPC/StreamServerConnection.serialization.in \
	Platform/cocoa/MediaPlaybackTargetContextSerialized.serialization.in \
	Shared/AuxiliaryProcessCreationParameters.serialization.in \
	Shared/API/APIArray.serialization.in \
	Shared/API/APIData.serialization.in \
	Shared/API/APIDictionary.serialization.in \
	Shared/API/APIError.serialization.in \
	Shared/API/APIFrameHandle.serialization.in \
	Shared/API/APIGeometry.serialization.in \
	Shared/API/APINumber.serialization.in \
	Shared/API/APIObject.serialization.in \
	Shared/API/APIPageHandle.serialization.in \
	Shared/API/APISerializedScriptValue.serialization.in \
	Shared/API/APIString.serialization.in \
	Shared/API/APIURL.serialization.in \
	Shared/API/APIURLRequest.serialization.in \
	Shared/API/APIURLResponse.serialization.in \
	Shared/API/APIUserContentURLPattern.serialization.in \
	Shared/AccessibilityPreferences.serialization.in \
	Shared/AlternativeTextClient.serialization.in \
	Shared/AppPrivacyReportTestingData.serialization.in \
	Shared/Authentication/AuthenticationChallengeDisposition.serialization.in \
	Shared/BackgroundFetchChange.serialization.in \
	Shared/BackgroundFetchState.serialization.in \
	Shared/CacheModel.serialization.in \
	Shared/Cocoa/CacheStoragePolicy.serialization.in \
	Shared/Cocoa/CoreIPCAVOutputContext.serialization.in \
	Shared/Cocoa/CoreIPCArray.serialization.in \
	Shared/Cocoa/CoreIPCAuditToken.serialization.in \
	Shared/Cocoa/CoreIPCCFType.serialization.in \
	Shared/Cocoa/CoreIPCCFURL.serialization.in \
	Shared/Cocoa/CoreIPCColor.serialization.in \
	Shared/Cocoa/CoreIPCContacts.serialization.in \
	Shared/Cocoa/CoreIPCDDActionContext.serialization.in \
	Shared/Cocoa/CoreIPCDDScannerResult.serialization.in \
	Shared/Cocoa/CoreIPCData.serialization.in \
	Shared/Cocoa/CoreIPCDate.serialization.in \
	Shared/Cocoa/CoreIPCDateComponents.serialization.in \
	Shared/Cocoa/CoreIPCDictionary.serialization.in \
	Shared/Cocoa/CoreIPCError.serialization.in \
	Shared/Cocoa/CoreIPCCVPixelBufferRef.serialization.in \
	Shared/Cocoa/CoreIPCFont.serialization.in \
	Shared/Cocoa/CoreIPCLocale.serialization.in \
	Shared/Cocoa/CoreIPCNSCFObject.serialization.in \
	Shared/Cocoa/CoreIPCNSShadow.serialization.in \
	Shared/Cocoa/CoreIPCNSURLCredential.serialization.in \
	Shared/Cocoa/CoreIPCNSURLProtectionSpace.serialization.in \
	Shared/Cocoa/CoreIPCNSURLRequest.serialization.in \
	Shared/Cocoa/CoreIPCNSValue.serialization.in \
	Shared/Cocoa/CoreIPCNull.serialization.in \
	Shared/Cocoa/CoreIPCPassKit.serialization.in \
	Shared/Cocoa/CoreIPCPersonNameComponents.serialization.in \
	Shared/Cocoa/CoreIPCPlist.serialization.in \
	Shared/Cocoa/CoreIPCPresentationIntent.serialization.in \
	Shared/Cocoa/CoreIPCSecureCoding.serialization.in \
	Shared/Cocoa/CoreIPCString.serialization.in \
	Shared/Cocoa/CoreIPCURL.serialization.in \
	Shared/Cocoa/CoreIPCCFCharacterSet.serialization.in \
	Shared/Cocoa/DataDetectionResult.serialization.in \
	Shared/Cocoa/InsertTextOptions.serialization.in \
	Shared/Cocoa/RemoteObjectInvocation.serialization.in \
	Shared/Cocoa/RevealItem.serialization.in \
	Shared/Cocoa/SharedCARingBuffer.serialization.in \
	Shared/Cocoa/WebCoreArgumentCodersCocoa.serialization.in \
	Shared/CallbackID.serialization.in \
	Shared/ContextMenuContextData.serialization.in \
	Shared/CoordinateSystem.serialization.in \
	Shared/DebuggableInfoData.serialization.in \
	Shared/DisplayListArgumentCoders.serialization.in \
	Shared/DocumentEditingContext.serialization.in \
	Shared/DragControllerAction.serialization.in \
	Shared/DrawingAreaInfo.serialization.in \
	Shared/EditingRange.serialization.in \
	Shared/EditorState.serialization.in \
	Shared/Extensions/WebExtensionAlarmParameters.serialization.in \
	Shared/Extensions/WebExtensionCommandParameters.serialization.in \
	Shared/Extensions/WebExtensionContentWorldType.serialization.in \
	Shared/Extensions/WebExtensionContext.serialization.in \
	Shared/Extensions/WebExtensionContextParameters.serialization.in \
	Shared/Extensions/WebExtensionControllerParameters.serialization.in \
	Shared/Extensions/WebExtensionCookieParameters.serialization.in \
	Shared/Extensions/WebExtensionDynamicScripts.serialization.in \
	Shared/Extensions/WebExtensionEventListenerType.serialization.in \
	Shared/Extensions/WebExtensionFrameParameters.serialization.in \
	Shared/Extensions/WebExtensionMatchedRuleParameters.serialization.in \
	Shared/Extensions/WebExtensionMenuItem.serialization.in \
	Shared/Extensions/WebExtensionMessageSenderParameters.serialization.in \
	Shared/Extensions/WebExtensionSidebarParameters.serialization.in \
	Shared/Extensions/WebExtensionStorage.serialization.in \
	Shared/Extensions/WebExtensionTab.serialization.in \
	Shared/Extensions/WebExtensionWindow.serialization.in \
	Shared/FileSystemSyncAccessHandleInfo.serialization.in \
	Shared/FocusedElementInformation.serialization.in \
	Shared/FrameInfoData.serialization.in \
	Shared/FrameTreeCreationParameters.serialization.in \
	Shared/FrameTreeNodeData.serialization.in \
	Shared/FullScreenMediaDetails.serialization.in \
	Shared/Gamepad/GamepadData.serialization.in \
	Shared/GPUProcessConnectionParameters.serialization.in \
	Shared/GoToBackForwardItemParameters.serialization.in \
	Shared/ImageOptions.serialization.in \
	Shared/InspectorExtensionTypes.serialization.in \
	Shared/PlatformFontInfo.serialization.in \
	Shared/ios/DynamicViewportSizeUpdate.serialization.in \
	Shared/ios/GestureTypes.serialization.in \
	Shared/ios/HardwareKeyboardState.serialization.in \
	Shared/ios/InteractionInformationAtPosition.serialization.in \
	Shared/ios/InteractionInformationRequest.serialization.in \
	Shared/ios/WebAutocorrectionContext.serialization.in \
	Shared/ios/WebAutocorrectionData.serialization.in \
	Shared/JavaScriptCore.serialization.in \
	Shared/LayerTreeContext.serialization.in \
	Shared/LoadParameters.serialization.in \
	Shared/MediaPlaybackState.serialization.in \
	Shared/Model.serialization.in \
	Shared/ModelProcessConnectionParameters.serialization.in \
	Shared/MonotonicObjectIdentifier.serialization.in \
	Shared/NavigationActionData.serialization.in \
	Shared/NetworkProcessConnectionParameters.serialization.in \
	Shared/Pasteboard.serialization.in \
	Shared/PlatformPopupMenuData.serialization.in \
	Shared/PolicyDecision.serialization.in \
	Shared/PrintInfo.serialization.in \
	Shared/ProcessQualified.serialization.in \
	Shared/ProvisionalFrameCreationParameters.serialization.in \
	Shared/PushMessageForTesting.serialization.in \
	Shared/RTCNetwork.serialization.in \
	Shared/RTCPacketOptions.serialization.in \
	Shared/RemoteWorkerInitializationData.serialization.in \
	Shared/RemoteWorkerType.serialization.in \
	Shared/ResourceLoadInfo.serialization.in \
	Shared/ResourceLoadStatisticsParameters.serialization.in \
	Shared/SameDocumentNavigationType.serialization.in \
	Shared/SandboxExtension.serialization.in \
	Shared/ScrollingAccelerationCurve.serialization.in \
	Shared/SessionState.serialization.in \
	Shared/SyntheticEditingCommandType.serialization.in \
	Shared/TextFlags.serialization.in \
	Shared/TextAnimationTypes.serialization.in \
	Shared/TextRecognitionResult.serialization.in \
	Shared/TextRecognitionUpdateResult.serialization.in \
	Shared/URLSchemeTaskParameters.serialization.in \
	Shared/UndoOrRedo.serialization.in \
	Shared/UserContentControllerParameters.serialization.in \
	Shared/UserData.serialization.in \
	Shared/UserInterfaceIdiom.serialization.in \
	Shared/VideoCodecType.serialization.in \
	Shared/WebCompiledContentRuleListData.serialization.in \
	Shared/ViewWindowCoordinates.serialization.in \
	Shared/VisibleContentRectUpdateInfo.serialization.in \
	Shared/WTFArgumentCoders.serialization.in \
	Shared/WebBackForwardListCounts.serialization.in \
	Shared/WebContextMenuItemData.serialization.in \
	Shared/WebCoreArgumentCoders.serialization.in \
	Shared/WebCoreFont.serialization.in \
	Shared/WebEvent.serialization.in \
	Shared/WebFindOptions.serialization.in \
	Shared/WebFoundTextRange.serialization.in \
	Shared/WebHitTestResultData.serialization.in \
	Shared/WebImage.serialization.in \
	Shared/WebNavigationDataStore.serialization.in \
	Shared/WebPageCreationParameters.serialization.in \
	Shared/WebPageNetworkParameters.serialization.in \
	Shared/WebPageGroupData.serialization.in \
	Shared/WebPopupItem.serialization.in \
	Shared/WebPreferencesStore.serialization.in \
	Shared/WebProcessCreationParameters.serialization.in \
	Shared/WebProcessDataStoreParameters.serialization.in \
	Shared/WebPushDaemonConnectionConfiguration.serialization.in \
	Shared/WebPushMessage.serialization.in \
	Shared/WebsiteAutoplayPolicy.serialization.in \
	Shared/WebsiteAutoplayQuirk.serialization.in \
	Shared/WebsitePoliciesData.serialization.in \
	Shared/WebsitePopUpPolicy.serialization.in \
	Shared/ApplePay/ApplePayPaymentSetupFeatures.serialization.in \
	Shared/ApplePay/PaymentSetupConfiguration.serialization.in \
	Shared/Databases/IndexedDB/WebIDBResult.serialization.in \
	Shared/RemoteLayerTree/RemoteLayerTree.serialization.in \
	Shared/RemoteLayerTree/BufferAndBackendInfo.serialization.in \
	Shared/RemoteLayerTree/RemoteScrollingCoordinatorTransaction.serialization.in \
	Shared/RemoteLayerTree/RemoteScrollingUIState.serialization.in \
	Shared/cf/CFTypes.serialization.in \
	Shared/cf/CoreIPCBoolean.serialization.in \
	Shared/cf/CoreIPCCFArray.serialization.in \
	Shared/cf/CoreIPCCFDictionary.serialization.in \
	Shared/cf/CoreIPCCGColorSpace.serialization.in \
	Shared/cf/CoreIPCNumber.serialization.in \
	Shared/cf/CoreIPCSecAccessControl.serialization.in \
	Shared/cf/CoreIPCSecCertificate.serialization.in \
	Shared/cf/CoreIPCSecKeychainItem.serialization.in \
	Shared/cf/CoreIPCSecTrust.serialization.in \
	Shared/mac/PDFContextMenuItem.serialization.in \
	Shared/mac/SecItemRequestData.serialization.in \
	Shared/mac/SecItemResponseData.serialization.in \
	Shared/mac/WebHitTestResultPlatformData.serialization.in \
	Shared/WebsiteDataStoreParameters.serialization.in \
	Shared/WebsiteData/UnifiedOriginStorageLevel.serialization.in \
	Shared/WebsiteData/WebsiteData.serialization.in \
	Shared/WebsiteData/WebsiteDataFetchOption.serialization.in \
	Shared/WebsiteData/WebsiteDataType.serialization.in \
	Shared/WebGPU/WebGPUBindGroupDescriptor.serialization.in \
	Shared/WebGPU/WebGPUBindGroupLayoutDescriptor.serialization.in \
	Shared/WebGPU/WebGPUBindGroupLayoutEntry.serialization.in \
	Shared/WebGPU/WebGPUBufferDescriptor.serialization.in \
	Shared/WebGPU/WebGPUCommandBufferDescriptor.serialization.in \
	Shared/WebGPU/WebGPUCommandEncoderDescriptor.serialization.in \
	Shared/WebGPU/WebGPUCompilationMessage.serialization.in \
	Shared/WebGPU/WebGPUComputePassDescriptor.serialization.in \
	Shared/WebGPU/WebGPUComputePipelineDescriptor.serialization.in \
	Shared/WebGPU/WebGPUDepthStencilState.serialization.in \
	Shared/WebGPU/WebGPUDeviceDescriptor.serialization.in \
	Shared/WebGPU/WebGPUExtent3D.serialization.in \
	Shared/WebGPU/WebGPUExternalTextureBindingLayout.serialization.in \
	Shared/WebGPU/WebGPUExternalTextureDescriptor.serialization.in \
	Shared/WebGPU/WebGPUFeatureName.serialization.in \
	Shared/WebGPU/WebGPUFragmentState.serialization.in \
	Shared/WebGPU/WebGPUImageCopyBuffer.serialization.in \
	Shared/WebGPU/WebGPUImageCopyExternalImage.serialization.in \
	Shared/WebGPU/WebGPUImageCopyTexture.serialization.in \
	Shared/WebGPU/WebGPUImageCopyTextureTagged.serialization.in \
	Shared/WebGPU/WebGPUImageDataLayout.serialization.in \
	Shared/WebGPU/WebGPUInternalError.serialization.in \
	Shared/WebGPU/WebGPUMultisampleState.serialization.in \
	Shared/WebGPU/WebGPUOrigin2D.serialization.in \
	Shared/WebGPU/WebGPUOutOfMemoryError.serialization.in \
	Shared/WebGPU/WebGPUPipelineDescriptorBase.serialization.in \
	Shared/WebGPU/WebGPUPipelineLayoutDescriptor.serialization.in \
	Shared/WebGPU/WebGPUPresentationContextDescriptor.serialization.in \
	Shared/WebGPU/WebGPUQuerySetDescriptor.serialization.in \
	Shared/WebGPU/WebGPURenderBundleDescriptor.serialization.in \
	Shared/WebGPU/WebGPURenderBundleEncoderDescriptor.serialization.in \
	Shared/WebGPU/WebGPURenderPassDescriptor.serialization.in \
	Shared/WebGPU/WebGPURenderPassLayout.serialization.in \
	Shared/WebGPU/WebGPURenderPipelineDescriptor.serialization.in \
	Shared/WebGPU/WebGPURequestAdapterOptions.serialization.in \
	Shared/WebGPU/WebGPUSamplerBindingLayout.serialization.in \
	Shared/WebGPU/WebGPUSamplerDescriptor.serialization.in \
	Shared/WebGPU/WebGPUShaderModuleCompilationHint.serialization.in \
	Shared/WebGPU/WebGPUShaderModuleDescriptor.serialization.in \
	Shared/WebGPU/WebGPUTextureDescriptor.serialization.in \
	Shared/WebGPU/WebGPUTextureViewDescriptor.serialization.in \
	Shared/WebGPU/WebGPUVertexState.serialization.in \
	Shared/WebGPU/WebGPUVertexBufferLayout.serialization.in \
	Shared/WebGPU/WebGPUVertexAttribute.serialization.in \
	Shared/WebGPU/WebGPUValidationError.serialization.in \
	Shared/WebGPU/WebGPUTextureBindingLayout.serialization.in \
	Shared/WebGPU/WebGPUSupportedLimits.serialization.in \
	Shared/WebGPU/WebGPUSupportedFeatures.serialization.in \
	Shared/WebGPU/WebGPUStorageTextureBindingLayout.serialization.in \
	Shared/WebGPU/WebGPUStencilFaceState.serialization.in \
	Shared/WebGPU/WebGPURenderPassTimestampWrites.serialization.in \
	Shared/WebGPU/WebGPURenderPassDepthStencilAttachment.serialization.in \
	Shared/WebGPU/WebGPURenderPassColorAttachment.serialization.in \
	Shared/WebGPU/WebGPUProgrammableStage.serialization.in \
	Shared/WebGPU/WebGPUPrimitiveState.serialization.in \
	Shared/WebGPU/WebGPUOrigin3D.serialization.in \
	Shared/WebGPU/WebGPUObjectDescriptorBase.serialization.in \
	Shared/WebGPU/WebGPUComputePassTimestampWrites.serialization.in \
	Shared/WebGPU/WebGPUColorTargetState.serialization.in \
	Shared/WebGPU/WebGPUColor.serialization.in \
	Shared/WebGPU/WebGPUCanvasConfiguration.serialization.in \
	Shared/WebGPU/WebGPUBufferBindingLayout.serialization.in \
	Shared/WebGPU/WebGPUBufferBinding.serialization.in \
	Shared/WebGPU/WebGPUBlendState.serialization.in \
	Shared/WebGPU/WebGPUBlendComponent.serialization.in \
	Shared/WebGPU/WebGPUBindGroupEntry.serialization.in \
	Shared/XR/PlatformXR.serialization.in \
	Shared/XR/XRSystem.serialization.in \
	Shared/WebUserContentControllerDataTypes.serialization.in \
	UIProcess/Extensions/WebExtension.serialization.in \
	WebProcess/GPU/GPUProcessConnectionInfo.serialization.in \
	WebProcess/GPU/graphics/BufferIdentifierSet.serialization.in \
	WebProcess/GPU/graphics/PrepareBackingStoreBuffersData.serialization.in \
	WebProcess/GPU/media/MediaOverridesForTesting.serialization.in \
	WebProcess/GPU/media/MediaTimeUpdateData.serialization.in \
	WebProcess/GPU/media/RemoteCDMConfiguration.serialization.in \
	WebProcess/GPU/media/RemoteCDMInstanceConfiguration.serialization.in \
	WebProcess/GPU/media/RemoteAudioSessionConfiguration.serialization.in \
	WebProcess/GPU/media/RemoteMediaPlayerConfiguration.serialization.in \
	WebProcess/GPU/media/RemoteMediaPlayerState.serialization.in \
	WebProcess/GPU/media/RemoteVideoFrameProxyProperties.serialization.in \
	WebProcess/GPU/webrtc/SharedVideoFrame.serialization.in \
	WebProcess/MediaStream/MediaDeviceSandboxExtensions.serialization.in \
	WebProcess/Model/ModelProcessConnectionInfo.serialization.in \
	WebProcess/Network/NetworkProcessConnectionInfo.serialization.in \
	WebProcess/UserContent/InjectUserScriptImmediately.serialization.in \
	WebProcess/WebCoreSupport/WebSpeechSynthesisVoice.serialization.in \
	WebProcess/WebPage/RemoteLayerTree/PlatformCAAnimationRemoteProperties.serialization.in \
	SharedPreferencesForWebProcess.serialization.in \
#

WEBCORE_SERIALIZATION_DESCRIPTION_FILES = \
	HTTPHeaderNames.serialization.in \
	ActivityState.serialization.in \
	DragActions.serialization.in \
	InbandTextTrackPrivate.serialization.in \
	IndexedDB.serialization.in \
	LayoutMilestones.serialization.in \
	MediaProducer.serialization.in \
	MDNSRegisterError.serialization.in \
	PlatformEvent.serialization.in \
	PlatformMediaSession.serialization.in \
	PlatformScreen.serialization.in \
	PlatformWheelEvent.serialization.in \
	PlaybackSessionModel.serialization.in \
	ProtectionSpaceBase.serialization.in \
	ScrollTypes.serialization.in \
	WebGPU.serialization.in \
#

WEBCORE_SERIALIZATION_DESCRIPTION_FILES_FULLPATH := $(foreach I,$(WEBCORE_SERIALIZATION_DESCRIPTION_FILES),$(WebCorePrivateHeaders)/$I)

all : GeneratedSerializers.h GeneratedSerializers.mm GeneratedWebKitSecureCoding.h GeneratedWebKitSecureCoding.mm SerializedTypeInfo.mm WebKitPlatformGeneratedSerializers.mm

GENERATED_SERIALIZERS_OUTPUT_FILES = \
    GeneratedSerializers.h \
    GeneratedSerializers.mm \
    GeneratedWebKitSecureCoding.h \
    GeneratedWebKitSecureCoding.mm \
    SerializedTypeInfo.mm \
    WebKitPlatformGeneratedSerializers.mm \
#

GENERATED_SERIALIZERS_OUTPUT_PATTERNS = $(subst .,%,$(GENERATED_SERIALIZERS_OUTPUT_FILES))

$(GENERATED_SERIALIZERS_OUTPUT_PATTERNS) : $(WebKit2)/Scripts/generate-serializers.py $(SERIALIZATION_DESCRIPTION_FILES) $(WebKit2)/DerivedSources.make $(WEBCORE_SERIALIZATION_DESCRIPTION_FILES_FULLPATH)
	$(PYTHON) $(WebKit2)/Scripts/generate-serializers.py mm $(filter %.in,$^)

EXTENSIONS_DIR = $(WebKit2)/WebProcess/Extensions
EXTENSIONS_SCRIPTS_DIR = $(EXTENSIONS_DIR)/Bindings/Scripts
EXTENSIONS_INTERFACES_DIR = $(EXTENSIONS_DIR)/Interfaces
IDL_ATTRIBUTES_FILE = $(EXTENSIONS_SCRIPTS_DIR)/IDLAttributes.json
IDL_FILE_NAMES_LIST = WebExtensionIDLFileNamesList.txt

BINDINGS_SCRIPTS = \
    $(WebCorePrivateHeaders)/generate-bindings.pl \
    $(WebCorePrivateHeaders)/IDLParser.pm \
    $(WebCorePrivateHeaders)/CodeGenerator.pm \
    $(EXTENSIONS_SCRIPTS_DIR)/CodeGeneratorExtensions.pm \
#

EXTENSION_INTERFACES = \
    WebExtensionAPIAction \
    WebExtensionAPIAlarms \
    WebExtensionAPICommands \
    WebExtensionAPICookies \
    WebExtensionAPIDeclarativeNetRequest \
    WebExtensionAPIDevTools \
    WebExtensionAPIDevToolsExtensionPanel \
    WebExtensionAPIDevToolsInspectedWindow \
    WebExtensionAPIDevToolsNetwork \
    WebExtensionAPIDevToolsPanels \
    WebExtensionAPIEvent \
    WebExtensionAPIExtension \
    WebExtensionAPILocalization \
    WebExtensionAPIMenus \
    WebExtensionAPINamespace \
    WebExtensionAPINotifications \
    WebExtensionAPIPermissions \
    WebExtensionAPIPort \
    WebExtensionAPIRuntime \
    WebExtensionAPIScripting \
    WebExtensionAPISidePanel \
    WebExtensionAPISidebarAction \
    WebExtensionAPIStorage \
    WebExtensionAPIStorageArea \
    WebExtensionAPITabs \
    WebExtensionAPITest \
    WebExtensionAPIWebNavigation \
    WebExtensionAPIWebNavigationEvent \
    WebExtensionAPIWebPageNamespace \
    WebExtensionAPIWebPageRuntime \
    WebExtensionAPIWebRequest \
    WebExtensionAPIWebRequestEvent \
    WebExtensionAPIWindows \
    WebExtensionAPIWindowsEvent \
#

$(IDL_FILE_NAMES_LIST) : $(EXTENSION_INTERFACES:%=%.idl)
	echo $^ | tr " " "\n" > $@

JS%.h JS%.mm : %.idl $(BINDINGS_SCRIPTS) $(IDL_ATTRIBUTES_FILE) $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES) $(IDL_FILE_NAMES_LIST)
	@echo Generating bindings for $*...
	$(PERL) -I $(WebCorePrivateHeaders) -I $(EXTENSIONS_SCRIPTS_DIR) $(WebCorePrivateHeaders)/generate-bindings.pl --defines "$(FEATURE_AND_PLATFORM_DEFINES)" --outputDir . --generator Extensions --idlAttributesFile $(IDL_ATTRIBUTES_FILE) --idlFileNamesList $(IDL_FILE_NAMES_LIST) $<

all : $(EXTENSION_INTERFACES:%=JS%.h) $(EXTENSION_INTERFACES:%=JS%.mm)

module.private.modulemap : $(WK_MODULEMAP_PRIVATE_FILE)
	unifdef $(addprefix -D, $(FEATURE_AND_PLATFORM_DEFINES)) $(addprefix -U, $(FEATURE_AND_PLATFORM_UNDEFINES)) -o $@ $< || [ $$? -eq 1 ]

all : module.private.modulemap

ifeq ($(USE_INTERNAL_SDK),YES)
WEBKIT_ADDITIONS_SWIFT_FILES = \
#

$(WEBKIT_ADDITIONS_SWIFT_FILES): %.swift : %.swift.in
	cp -f $< $@

all : $(WEBKIT_ADDITIONS_SWIFT_FILES)
endif
