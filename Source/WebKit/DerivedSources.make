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
    $(WebKit2)/PluginProcess \
    $(WebKit2)/PluginProcess/mac \
    $(WebKit2)/Resources/SandboxProfiles/ios \
    $(WebKit2)/Shared/Plugins \
    $(WebKit2)/Shared \
    $(WebKit2)/Shared/API/Cocoa \
    $(WebKit2)/Shared/ApplePay \
    $(WebKit2)/Shared/Authentication \
    $(WebKit2)/Shared/mac \
    $(WebKit2)/WebAuthnProcess \
    $(WebKit2)/WebAuthnProcess/mac \
    $(WebKit2)/WebProcess/ApplePay \
    $(WebKit2)/WebProcess/ApplicationCache \
    $(WebKit2)/WebProcess/Automation \
    $(WebKit2)/WebProcess/Cache \
    $(WebKit2)/WebProcess/Databases/IndexedDB \
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
    $(WEBKITADDITIONS_HEADER_SEARCH_PATHS) \
#

PYTHON = python
PERL = perl
RUBY = ruby

ifeq ($(OS),Windows_NT)
    DELETE = cmd //C del
else
    DELETE = rm -f
endif

MESSAGE_RECEIVERS = \
	NetworkProcess/NetworkBroadcastChannelRegistry \
	NetworkProcess/NetworkConnectionToWebProcess \
	NetworkProcess/IndexedDB/WebIDBServer \
	NetworkProcess/NetworkContentRuleListManager \
	NetworkProcess/WebStorage/StorageManagerSet \
	NetworkProcess/cache/CacheStorageEngineConnection \
	NetworkProcess/CustomProtocols/LegacyCustomProtocolManager \
	NetworkProcess/NetworkSocketChannel \
	NetworkProcess/ServiceWorker/WebSWServerConnection \
	NetworkProcess/ServiceWorker/ServiceWorkerFetchTask \
	NetworkProcess/ServiceWorker/WebSWServerToContextConnection \
	NetworkProcess/NetworkSocketStream \
	NetworkProcess/NetworkProcess \
	NetworkProcess/NetworkResourceLoader \
	NetworkProcess/webrtc/NetworkMDNSRegister \
	NetworkProcess/webrtc/NetworkRTCProvider \
	NetworkProcess/webrtc/NetworkRTCMonitor \
	NetworkProcess/webrtc/RTCDataChannelRemoteManagerProxy \
	NetworkProcess/Cookies/WebCookieManager \
	Shared/Plugins/NPObjectMessageReceiver \
	Shared/AuxiliaryProcess \
	Shared/API/Cocoa/RemoteObjectRegistry \
	Shared/ApplePay/WebPaymentCoordinatorProxy \
	Shared/Authentication/AuthenticationManager \
	Shared/WebConnection \
	UIProcess/WebFullScreenManagerProxy \
	UIProcess/RemoteLayerTree/RemoteLayerTreeDrawingAreaProxy \
	UIProcess/GPU/GPUProcessProxy \
	UIProcess/WebAuthentication/WebAuthenticatorCoordinatorProxy \
	UIProcess/WebAuthentication/WebAuthnProcessProxy \
	UIProcess/WebPasteboardProxy \
	UIProcess/UserContent/WebUserContentControllerProxy \
	UIProcess/Inspector/WebInspectorUIProxy \
	UIProcess/Inspector/RemoteWebInspectorUIProxy \
	UIProcess/Inspector/WebInspectorUIExtensionControllerProxy \
	UIProcess/Plugins/PluginProcessProxy \
	UIProcess/DrawingAreaProxy \
	UIProcess/Network/NetworkProcessProxy \
	UIProcess/Network/CustomProtocols/LegacyCustomProtocolManagerProxy \
	UIProcess/WebPageProxy \
	UIProcess/VisitedLinkStore \
	UIProcess/ios/WebDeviceOrientationUpdateProviderProxy \
	UIProcess/ios/SmartMagnificationController \
	UIProcess/mac/SecItemShimProxy \
	UIProcess/WebGeolocationManagerProxy \
	UIProcess/Cocoa/PlaybackSessionManagerProxy \
	UIProcess/Cocoa/UserMediaCaptureManagerProxy \
	UIProcess/Cocoa/VideoFullscreenManagerProxy \
	UIProcess/WebCookieManagerProxy \
	UIProcess/ViewGestureController \
	UIProcess/WebProcessProxy \
	UIProcess/Automation/WebAutomationSession \
	UIProcess/WebProcessPool \
	UIProcess/Downloads/DownloadProxy \
	UIProcess/Media/AudioSessionRoutingArbitratorProxy \
	UIProcess/Media/RemoteMediaSessionCoordinatorProxy \
	UIProcess/SpeechRecognitionRemoteRealtimeMediaSourceManager \
	UIProcess/SpeechRecognitionServer \
	UIProcess/XR/PlatformXRSystem \
	WebProcess/Databases/IndexedDB/WebIDBConnectionToServer \
	WebProcess/GPU/GPUProcessConnection \
	WebProcess/GPU/graphics/RemoteRenderingBackendProxy \
	WebProcess/GPU/graphics/RemoteGraphicsContextGLProxy \
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
	WebProcess/WebStorage/StorageAreaMap \
	WebProcess/UserContent/WebUserContentController \
	WebProcess/Inspector/WebInspectorInterruptDispatcher \
	WebProcess/Inspector/WebInspectorUI \
	WebProcess/Inspector/WebInspectorUIExtensionController \
	WebProcess/Inspector/WebInspector \
	WebProcess/Inspector/RemoteWebInspectorUI \
	WebProcess/MediaSession/RemoteMediaSessionCoordinator \
	WebProcess/Plugins/PluginProcessConnectionManager \
	WebProcess/Plugins/PluginProxy \
	WebProcess/Plugins/PluginProcessConnection \
	WebProcess/Network/WebSocketChannel \
	WebProcess/Network/NetworkProcessConnection \
	WebProcess/Network/WebSocketStream \
	WebProcess/Network/WebResourceLoader \
	WebProcess/Network/webrtc/LibWebRTCNetwork \
	WebProcess/Network/webrtc/RTCDataChannelRemoteManager \
	WebProcess/Network/webrtc/WebRTCMonitor \
	WebProcess/Network/webrtc/WebMDNSRegister \
	WebProcess/Network/webrtc/WebRTCResolver \
	WebProcess/WebCoreSupport/WebBroadcastChannelRegistry \
	WebProcess/WebCoreSupport/WebDeviceOrientationUpdateProvider \
	WebProcess/WebCoreSupport/WebSpeechRecognitionConnection \
	WebProcess/Speech/SpeechRecognitionRealtimeMediaSourceManager \
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
	WebProcess/WebPage/WebPage \
	WebProcess/WebPage/VisitedLinkTableController \
	WebProcess/WebPage/Cocoa/TextCheckingControllerProxy \
	WebProcess/WebPage/ViewUpdateDispatcher \
	WebProcess/WebAuthentication/WebAuthnProcessConnection \
	WebProcess/XR/PlatformXRSystemProxy \
	PluginProcess/WebProcessConnection \
	PluginProcess/PluginControllerProxy \
	PluginProcess/PluginProcess \
	GPUProcess/GPUConnectionToWebProcess \
	GPUProcess/graphics/RemoteRenderingBackend \
	GPUProcess/graphics/RemoteGraphicsContextGL \
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
	GPUProcess/media/RemoteMediaSourceProxy \
	GPUProcess/media/RemoteRemoteCommandListenerProxy \
	GPUProcess/media/RemoteSourceBufferProxy \
	WebAuthnProcess/WebAuthnConnectionToWebProcess \
	WebAuthnProcess/WebAuthnProcess \
#

GENERATE_MESSAGE_RECEIVER_SCRIPT = $(WebKit2)/Scripts/generate-message-receiver.py
GENERATE_MESSAGE_RECEIVER_SCRIPTS = \
    $(GENERATE_MESSAGE_RECEIVER_SCRIPT) \
    $(WebKit2)/Scripts/webkit/__init__.py \
    $(WebKit2)/Scripts/webkit/messages.py \
    $(WebKit2)/Scripts/webkit/model.py \
    $(WebKit2)/Scripts/webkit/parser.py \
#

FRAMEWORK_FLAGS := $(shell echo $(BUILT_PRODUCTS_DIR) $(FRAMEWORK_SEARCH_PATHS) $(SYSTEM_FRAMEWORK_SEARCH_PATHS) | perl -e 'print "-F " . join(" -F ", split(" ", <>));')
HEADER_FLAGS := $(shell echo $(BUILT_PRODUCTS_DIR) $(HEADER_SEARCH_PATHS) $(SYSTEM_HEADER_SEARCH_PATHS) | perl -e 'print "-I" . join(" -I", split(" ", <>));')

MESSAGE_RECEIVER_FILES := $(addsuffix MessageReceiver.cpp,$(notdir $(MESSAGE_RECEIVERS)))
MESSAGES_FILES := $(addsuffix Messages.h,$(notdir $(MESSAGE_RECEIVERS)))
MESSAGE_REPLIES_FILES := $(addsuffix MessagesReplies.h,$(notdir $(MESSAGE_RECEIVERS)))

GENERATED_MESSAGES_FILES := $(MESSAGE_RECEIVER_FILES) $(MESSAGES_FILES) $(MESSAGE_REPLIES_FILES) MessageNames.h MessageNames.cpp MessageArgumentDescriptions.cpp
GENERATED_MESSAGES_FILES_AS_PATTERNS := $(subst .,%,$(GENERATED_MESSAGES_FILES))

MESSAGES_IN_FILES := $(addsuffix .messages.in,$(MESSAGE_RECEIVERS))

.PHONY : all

all : $(GENERATED_MESSAGES_FILES)

$(GENERATED_MESSAGES_FILES_AS_PATTERNS) : $(MESSAGES_IN_FILES) $(GENERATE_MESSAGE_RECEIVER_SCRIPTS)
	python $(GENERATE_MESSAGE_RECEIVER_SCRIPT) $(WebKit2) $(MESSAGE_RECEIVERS)

TEXT_PREPROCESSOR_FLAGS=-E -P -w

ifneq ($(SDKROOT),)
	SDK_FLAGS=-isysroot $(SDKROOT)
endif

ifeq ($(USE_LLVM_TARGET_TRIPLES_FOR_CLANG),YES)
	WK_CURRENT_ARCH=$(word 1, $(ARCHS))
	TARGET_TRIPLE_FLAGS=-target $(WK_CURRENT_ARCH)-$(LLVM_TARGET_TRIPLE_VENDOR)-$(LLVM_TARGET_TRIPLE_OS_VERSION)$(LLVM_TARGET_TRIPLE_SUFFIX)
endif

SANDBOX_PROFILES = \
	com.apple.WebProcess.sb \
	com.apple.WebKit.plugin-common.sb \
	com.apple.WebKit.NetworkProcess.sb \
	com.apple.WebKit.GPUProcess.sb \
	com.apple.WebKit.WebAuthnProcess.sb
	
SANDBOX_PROFILES_IOS = \
	com.apple.WebKit.WebContent.sb \

sandbox-profiles-ios : $(SANDBOX_PROFILES_IOS)

all : $(SANDBOX_PROFILES) $(SANDBOX_PROFILES_IOS)

%.sb : %.sb.in
	@echo Pre-processing $* sandbox profile...
	grep -o '^[^;]*' $< | $(CC) $(SDK_FLAGS) $(TARGET_TRIPLE_FLAGS) $(TEXT_PREPROCESSOR_FLAGS) $(FRAMEWORK_FLAGS) $(HEADER_FLAGS) -include "wtf/Platform.h" - > $@

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
