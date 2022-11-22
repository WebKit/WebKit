# Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
    $(WebKit2)/Resources/SandboxProfiles/ios \
    $(WebKit2)/Shared/Plugins \
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
	NetworkProcess/cache/CacheStorageEngineConnection \
	NetworkProcess/CustomProtocols/LegacyCustomProtocolManager \
	NetworkProcess/NetworkSocketChannel \
	NetworkProcess/ServiceWorker/WebSWServerConnection \
	NetworkProcess/ServiceWorker/ServiceWorkerDownloadTask \
	NetworkProcess/ServiceWorker/ServiceWorkerFetchTask \
	NetworkProcess/ServiceWorker/WebSWServerToContextConnection \
	NetworkProcess/SharedWorker/WebSharedWorkerServerConnection \
	NetworkProcess/SharedWorker/WebSharedWorkerServerToContextConnection \
	NetworkProcess/NetworkSocketStream \
	NetworkProcess/NetworkProcess \
	NetworkProcess/NetworkResourceLoader \
	NetworkProcess/webrtc/NetworkMDNSRegister \
	NetworkProcess/webrtc/NetworkRTCProvider \
	NetworkProcess/webrtc/NetworkRTCMonitor \
	NetworkProcess/webrtc/RTCDataChannelRemoteManagerProxy \
	NetworkProcess/Cookies/WebCookieManager \
	NetworkProcess/storage/NetworkStorageManager \
	Shared/AuxiliaryProcess \
	Shared/API/Cocoa/RemoteObjectRegistry \
	Shared/ApplePay/WebPaymentCoordinatorProxy \
	Shared/Authentication/AuthenticationManager \
	Shared/Notifications/NotificationManagerMessageHandler \
	Shared/WebConnection \
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
	UIProcess/WebFrameProxy \
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
	UIProcess/Cocoa/VideoFullscreenManagerProxy \
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
	UIProcess/SpeechRecognitionRemoteRealtimeMediaSourceManager \
	UIProcess/SpeechRecognitionServer \
	UIProcess/XR/PlatformXRSystem \
	WebProcess/Databases/IndexedDB/WebIDBConnectionToServer \
	WebProcess/Extensions/WebExtensionContextProxy \
	WebProcess/Extensions/WebExtensionControllerProxy \
	WebProcess/GPU/GPUProcessConnection \
	WebProcess/GPU/graphics/RemoteRenderingBackendProxy \
	WebProcess/GPU/graphics/RemoteGraphicsContextGLProxy \
	WebProcess/GPU/graphics/WebGPU/RemoteGPUProxy \
	WebProcess/GPU/webrtc/LibWebRTCCodecs \
	WebProcess/GPU/webrtc/SampleBufferDisplayLayer \
	WebProcess/GPU/media/MediaPlayerPrivateRemote \
	WebProcess/GPU/media/MediaSourcePrivateRemote \
	WebProcess/GPU/media/RemoteAudioHardwareListener \
	WebProcess/GPU/media/RemoteAudioSession \
	WebProcess/GPU/media/RemoteAudioSourceProviderManager \
	WebProcess/GPU/media/RemoteCDMInstance \
	WebProcess/GPU/media/RemoteCDMInstanceSession \
	WebProcess/GPU/media/RemoteImageDecoderAVFManager \
	WebProcess/GPU/media/RemoteLegacyCDMSession \
	WebProcess/GPU/media/RemoteRemoteCommandListener \
	WebProcess/GPU/media/SourceBufferPrivateRemote \
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
	WebProcess/Network/WebSocketChannel \
	WebProcess/Network/NetworkProcessConnection \
	WebProcess/Network/WebSocketStream \
	WebProcess/Network/WebResourceLoader \
	WebProcess/Network/webrtc/LibWebRTCNetwork \
	WebProcess/Network/webrtc/RTCDataChannelRemoteManager \
	WebProcess/Network/webrtc/WebRTCMonitor \
	WebProcess/Network/webrtc/WebMDNSRegister \
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
	WebProcess/cocoa/VideoFullscreenManager \
	WebProcess/Geolocation/WebGeolocationManager \
	WebProcess/Automation/WebAutomationSessionProxy \
	WebProcess/FullScreen/WebFullScreenManager \
	WebProcess/ApplePay/WebPaymentCoordinator \
	WebProcess/Notifications/WebNotificationManager \
	WebProcess/WebPage/EventDispatcher \
	WebProcess/WebPage/RemoteLayerTree/RemoteScrollingCoordinator \
	WebProcess/WebPage/ViewGestureGeometryCollector \
	WebProcess/WebPage/DrawingArea \
	WebProcess/WebPage/WebFrame \
	WebProcess/WebPage/WebPage \
	WebProcess/WebPage/VisitedLinkTableController \
	WebProcess/WebPage/Cocoa/TextCheckingControllerProxy \
	WebProcess/WebPage/ViewUpdateDispatcher \
	WebProcess/XR/PlatformXRSystemProxy \
	GPUProcess/GPUConnectionToWebProcess \
	GPUProcess/graphics/RemoteDisplayListRecorder \
	GPUProcess/graphics/RemoteRenderingBackend \
	GPUProcess/graphics/RemoteGraphicsContextGL \
	GPUProcess/graphics/WebGPU/RemoteAdapter \
	GPUProcess/graphics/WebGPU/RemoteBindGroup \
	GPUProcess/graphics/WebGPU/RemoteBindGroupLayout \
	GPUProcess/graphics/WebGPU/RemoteBuffer \
	GPUProcess/graphics/WebGPU/RemoteCommandBuffer \
	GPUProcess/graphics/WebGPU/RemoteCommandEncoder \
	GPUProcess/graphics/WebGPU/RemoteComputePassEncoder \
	GPUProcess/graphics/WebGPU/RemoteComputePipeline \
	GPUProcess/graphics/WebGPU/RemoteDevice \
	GPUProcess/graphics/WebGPU/RemoteExternalTexture \
	GPUProcess/graphics/WebGPU/RemoteGPU \
	GPUProcess/graphics/WebGPU/RemotePipelineLayout \
	GPUProcess/graphics/WebGPU/RemoteQuerySet \
	GPUProcess/graphics/WebGPU/RemoteQueue \
	GPUProcess/graphics/WebGPU/RemoteRenderBundle \
	GPUProcess/graphics/WebGPU/RemoteRenderBundleEncoder \
	GPUProcess/graphics/WebGPU/RemoteRenderPassEncoder \
	GPUProcess/graphics/WebGPU/RemoteRenderPipeline \
	GPUProcess/graphics/WebGPU/RemoteSampler \
	GPUProcess/graphics/WebGPU/RemoteShaderModule \
	GPUProcess/graphics/WebGPU/RemoteSurface \
	GPUProcess/graphics/WebGPU/RemoteSwapChain \
	GPUProcess/graphics/WebGPU/RemoteTexture \
	GPUProcess/graphics/WebGPU/RemoteTextureView \
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

FRAMEWORK_FLAGS := $(shell echo $(BUILT_PRODUCTS_DIR) $(FRAMEWORK_SEARCH_PATHS) $(SYSTEM_FRAMEWORK_SEARCH_PATHS) | perl -e 'print "-F " . join(" -F ", split(" ", <>));')
HEADER_FLAGS := $(shell echo $(BUILT_PRODUCTS_DIR) $(HEADER_SEARCH_PATHS) $(SYSTEM_HEADER_SEARCH_PATHS) | perl -e 'print "-I" . join(" -I", split(" ", <>));')

platform_h_compiler_command = $(CC) -std=c++2a -x c++ $(1) $(SDK_FLAGS) $(TARGET_TRIPLE_FLAGS) $(FRAMEWORK_FLAGS) $(HEADER_FLAGS) -include "wtf/Platform.h" /dev/null

FEATURE_AND_PLATFORM_DEFINES := $(shell $(call platform_h_compiler_command,-E -P -dM) | $(PERL) -ne "print if s/\#define ((HAVE_|USE_|ENABLE_|WTF_PLATFORM_)\w+) 1/\1/")

PLATFORM_HEADER_DIR := $(realpath $(BUILT_PRODUCTS_DIR)$(WK_LIBRARY_HEADERS_FOLDER_PATH))
PLATFORM_HEADER_DEPENDENCIES := $(filter $(PLATFORM_HEADER_DIR)/%,$(realpath $(shell $(call platform_h_compiler_command,-M) | $(PERL) -e "local \$$/; my (\$$target, \$$deps) = split(/:/, <>); print split(/\\\\/, \$$deps);")))
FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES = $(WebKit2)/DerivedSources.make $(PLATFORM_HEADER_DEPENDENCIES)

MESSAGE_RECEIVER_FILES := $(addsuffix MessageReceiver.cpp,$(notdir $(MESSAGE_RECEIVERS)))
MESSAGES_FILES := $(addsuffix Messages.h,$(notdir $(MESSAGE_RECEIVERS)))
MESSAGE_REPLIES_FILES := $(addsuffix MessagesReplies.h,$(notdir $(MESSAGE_RECEIVERS)))

GENERATED_MESSAGES_FILES := $(MESSAGE_RECEIVER_FILES) $(MESSAGES_FILES) $(MESSAGE_REPLIES_FILES) MessageNames.h MessageNames.cpp MessageArgumentDescriptions.cpp
GENERATED_MESSAGES_FILES_AS_PATTERNS := $(subst .,%,$(GENERATED_MESSAGES_FILES))

MESSAGES_IN_FILES := $(addsuffix .messages.in,$(MESSAGE_RECEIVERS))

.PHONY : all

all : $(GENERATED_MESSAGES_FILES)

$(GENERATED_MESSAGES_FILES_AS_PATTERNS) : $(MESSAGES_IN_FILES) $(GENERATE_MESSAGE_RECEIVER_SCRIPTS)
	$(PYTHON) $(GENERATE_MESSAGE_RECEIVER_SCRIPT) $(WebKit2) $(MESSAGE_RECEIVERS)

TEXT_PREPROCESSOR_FLAGS=-E -P -w

ifneq ($(SDKROOT),)
	SDK_FLAGS=-isysroot $(SDKROOT)
endif

ifeq ($(USE_LLVM_TARGET_TRIPLES_FOR_CLANG),YES)
	WK_CURRENT_ARCH=$(word 1, $(ARCHS))
	TARGET_TRIPLE_FLAGS=-target $(WK_CURRENT_ARCH)-$(LLVM_TARGET_TRIPLE_VENDOR)-$(LLVM_TARGET_TRIPLE_OS_VERSION)$(LLVM_TARGET_TRIPLE_SUFFIX)
endif

ifeq ($(USE_SYSTEM_CONTENT_PATH),YES)
	SANDBOX_DEFINES = -DUSE_SYSTEM_CONTENT_PATH=1 -DSYSTEM_CONTENT_PATH=$(SYSTEM_CONTENT_PATH)
endif

SANDBOX_PROFILES = \
	com.apple.WebProcess.sb \
	com.apple.WebKit.NetworkProcess.sb \
	com.apple.WebKit.GPUProcess.sb \
	com.apple.WebKit.webpushd.sb
	
SANDBOX_PROFILES_IOS = \
	com.apple.WebKit.adattributiond.sb \
	com.apple.WebKit.GPU.sb \
	com.apple.WebKit.Networking.sb \
	com.apple.WebKit.WebContent.sb

sandbox-profiles-ios : $(SANDBOX_PROFILES_IOS)

all : $(SANDBOX_PROFILES) $(SANDBOX_PROFILES_IOS)

%.sb : %.sb.in
	@echo Pre-processing $* sandbox profile...
	grep -o '^[^;]*' $< | $(CC) $(SDK_FLAGS) $(TARGET_TRIPLE_FLAGS) $(SANDBOX_DEFINES) $(TEXT_PREPROCESSOR_FLAGS) $(FRAMEWORK_FLAGS) $(HEADER_FLAGS) -include "wtf/Platform.h" - > $@

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

WEB_PREFERENCES_INPUT_FILES = \
    ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferences.yaml \
    $(ADDITIONAL_WEB_PREFERENCES_INPUT_FILES) \
#
WEB_PREFERENCES_COMBINED_INPUT_FILE = WebPreferencesCombined.yaml

WEB_PREFERENCES_CATEGORY_INPUT_FILES = \
    ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesDebug.yaml \
    ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesExperimental.yaml \
    ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesInternal.yaml \
#

WEB_PREFERENCES_TEMPLATES = \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPageUpdatePreferences.cpp.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPreferencesDefinitions.h.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPreferencesExperimentalFeatures.cpp.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPreferencesGetterSetters.cpp.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPreferencesInternalDebugFeatures.cpp.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPreferencesKeys.cpp.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPreferencesKeys.h.erb \
    $(WebKit2)/Scripts/PreferencesTemplates/WebPreferencesStoreDefaultsMap.cpp.erb \
#
WEB_PREFERENCES_FILES = $(basename $(notdir $(WEB_PREFERENCES_TEMPLATES)))
WEB_PREFERENCES_PATTERNS = $(subst .,%,$(WEB_PREFERENCES_FILES))

all : $(WEB_PREFERENCES_FILES) $(WEB_PREFERENCES_COMBINED_INPUT_FILE)

$(WEB_PREFERENCES_COMBINED_INPUT_FILE) : $(WEB_PREFERENCES_INPUT_FILES)
	cat $^ > $(WEB_PREFERENCES_COMBINED_INPUT_FILE)

$(WEB_PREFERENCES_PATTERNS) : $(WTF_BUILD_SCRIPTS_DIR)/GeneratePreferences.rb $(WEB_PREFERENCES_TEMPLATES) $(WEB_PREFERENCES_COMBINED_INPUT_FILE) $(WEB_PREFERENCES_CATEGORY_INPUT_FILES)
	$(RUBY) $< --frontend WebKit --base $(WEB_PREFERENCES_COMBINED_INPUT_FILE) --debug ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesDebug.yaml --experimental ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesExperimental.yaml	--internal ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesInternal.yaml $(addprefix --template , $(WEB_PREFERENCES_TEMPLATES))

SERIALIZATION_DESCRIPTION_FILES = \
	GPUProcess/GPUProcessSessionParameters.serialization.in \
	GPUProcess/graphics/RemoteRenderingBackendCreationParameters.serialization.in \
	GPUProcess/media/InitializationSegmentInfo.serialization.in \
	GPUProcess/media/MediaDescriptionInfo.serialization.in \
	GPUProcess/media/TextTrackPrivateRemoteConfiguration.serialization.in \
	NetworkProcess/NetworkProcessCreationParameters.serialization.in \
	Shared/API/APIGeometry.serialization.in \
	Shared/Cocoa/WebCoreArgumentCodersCocoa.serialization.in \
	Shared/EditorState.serialization.in \
	Shared/FocusedElementInformation.serialization.in \
	Shared/FrameInfoData.serialization.in \
	Shared/FrameTreeNodeData.serialization.in \
	Shared/ios/InteractionInformationAtPosition.serialization.in \
	Shared/LayerTreeContext.serialization.in \
	Shared/Model.serialization.in \
	Shared/Pasteboard.serialization.in \
	Shared/SessionState.serialization.in \
	Shared/ShareableBitmap.serialization.in \
	Shared/TextFlags.serialization.in \
	Shared/WTFArgumentCoders.serialization.in \
	Shared/WebCoreArgumentCoders.serialization.in \
	Shared/WebExtensionContextParameters.serialization.in \
	Shared/WebEvent.serialization.in \
	Shared/WebExtensionControllerParameters.serialization.in \
	Shared/WebPushDaemonConnectionConfiguration.serialization.in \
	Shared/WebPushMessage.serialization.in \
	Shared/mac/SecItemRequestData.serialization.in \
	Shared/mac/SecItemResponseData.serialization.in \
	Shared/WebsiteDataStoreParameters.serialization.in \
	Shared/WebsiteData/WebsiteDataFetchOption.serialization.in \
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
	Shared/WebGPU/WebGPUMultisampleState.serialization.in \
	Shared/WebGPU/WebGPUOrigin2D.serialization.in \
	Shared/WebGPU/WebGPUOutOfMemoryError.serialization.in \
	Shared/WebGPU/WebGPUPipelineDescriptorBase.serialization.in \
	Shared/WebGPU/WebGPUPipelineLayoutDescriptor.serialization.in \
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
	Shared/WebGPU/WebGPUSurfaceDescriptor.serialization.in \
	Shared/WebGPU/WebGPUSwapChainDescriptor.serialization.in \
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
	Shared/XR/XRSystem.serialization.in \
	WebProcess/GPU/media/RemoteMediaPlayerState.serialization.in \
#

all : GeneratedSerializers.h GeneratedSerializers.cpp GeneratedSerializers.mm SerializedTypeInfo.cpp

GeneratedSerializers.h GeneratedSerializers.cpp GeneratedSerializers.mm SerializedTypeInfo.cpp : $(WebKit2)/Scripts/generate-serializers.py $(SERIALIZATION_DESCRIPTION_FILES) $(WebKit2)/DerivedSources.make
	$(PYTHON) $(WebKit2)/Scripts/generate-serializers.py $(WebKit2)/ $(SERIALIZATION_DESCRIPTION_FILES)

EXTENSIONS_DIR = $(WebKit2)/WebProcess/Extensions
EXTENSIONS_SCRIPTS_DIR = $(EXTENSIONS_DIR)/Bindings/Scripts
EXTENSIONS_INTERFACES_DIR = $(EXTENSIONS_DIR)/Interfaces
IDL_ATTRIBUTES_FILE = $(EXTENSIONS_SCRIPTS_DIR)/IDLAttributes.json

BINDINGS_SCRIPTS = \
    $(WebCorePrivateHeaders)/generate-bindings.pl \
    $(WebCorePrivateHeaders)/IDLParser.pm \
    $(WebCorePrivateHeaders)/CodeGenerator.pm \
    $(EXTENSIONS_SCRIPTS_DIR)/CodeGeneratorExtensions.pm \
#

EXTENSION_INTERFACES = \
    WebExtensionAPIExtension \
    WebExtensionAPINamespace \
    WebExtensionAPIRuntime \
    WebExtensionAPITest \
#

JS%.h JS%.mm : %.idl $(BINDINGS_SCRIPTS) $(IDL_ATTRIBUTES_FILE) $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	@echo Generating bindings for $*...
	$(PERL) -I $(WebCorePrivateHeaders) -I $(EXTENSIONS_SCRIPTS_DIR) $(WebCorePrivateHeaders)/generate-bindings.pl --defines "$(FEATURE_AND_PLATFORM_DEFINES)" --include $(EXTENSIONS_INTERFACES_DIR) --outputDir . --generator Extensions --idlAttributesFile $(IDL_ATTRIBUTES_FILE) $<

all : $(EXTENSION_INTERFACES:%=JS%.h) $(EXTENSION_INTERFACES:%=JS%.mm)
