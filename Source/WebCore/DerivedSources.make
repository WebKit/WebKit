# Copyright (C) 2006-2022 Apple Inc. All rights reserved.
# Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
# Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Workaround for rdar://84212106.
find_tool = $(realpath $(shell xcrun --sdk $(SDK_NAME) -f $(1)))

PYTHON := $(call find_tool,python3)
PERL := perl
RUBY := ruby
DELETE := rm -f

ifneq ($(SDKROOT),)
    SDK_FLAGS = -isysroot $(SDKROOT)
endif

WK_CURRENT_ARCH = $(word 1, $(ARCHS))
TARGET_TRIPLE_FLAGS = -target $(WK_CURRENT_ARCH)-$(LLVM_TARGET_TRIPLE_VENDOR)-$(LLVM_TARGET_TRIPLE_OS_VERSION)$(LLVM_TARGET_TRIPLE_SUFFIX)

FRAMEWORK_FLAGS := $(addprefix -F, $(BUILT_PRODUCTS_DIR) $(FRAMEWORK_SEARCH_PATHS) $(SYSTEM_FRAMEWORK_SEARCH_PATHS))
HEADER_FLAGS := $(addprefix -I, $(BUILT_PRODUCTS_DIR) $(HEADER_SEARCH_PATHS) $(SYSTEM_HEADER_SEARCH_PATHS))
EXTERNAL_FLAGS := -DRELEASE_WITHOUT_OPTIMIZATIONS $(addprefix -D, $(GCC_PREPROCESSOR_DEFINITIONS))

platform_h_compiler_command = $(CC) -std=c++2a -x c++ $(1) $(SDK_FLAGS) $(TARGET_TRIPLE_FLAGS) $(FRAMEWORK_FLAGS) $(HEADER_FLAGS) $(EXTERNAL_FLAGS) -include "wtf/Platform.h" /dev/null

FEATURE_AND_PLATFORM_DEFINES := $(shell $(call platform_h_compiler_command,-E -P -dM) | $(PERL) -ne "print if s/\#define ((HAVE_|USE_|ENABLE_|WTF_PLATFORM_)\w+) 1/\1/")

PLATFORM_HEADER_DIR := $(realpath $(BUILT_PRODUCTS_DIR)$(WK_LIBRARY_HEADERS_FOLDER_PATH))
PLATFORM_HEADER_DEPENDENCIES := $(filter $(PLATFORM_HEADER_DIR)/%,$(realpath $(shell $(call platform_h_compiler_command,-M) | $(PERL) -e "local \$$/; my (\$$target, \$$deps) = split(/:/, <>); print split(/\\\\/, \$$deps);")))
FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES = $(WebCore)/DerivedSources.make $(PLATFORM_HEADER_DEPENDENCIES)

# --------

JS_BINDING_IDLS := \
    $(WebCore)/Modules/WebGPU/GPU.idl \
    $(WebCore)/Modules/WebGPU/GPUAdapter.idl \
    $(WebCore)/Modules/WebGPU/GPUAddressMode.idl \
    $(WebCore)/Modules/WebGPU/GPUAutoLayoutMode.idl \
    $(WebCore)/Modules/WebGPU/GPUBindGroup.idl \
    $(WebCore)/Modules/WebGPU/GPUBindGroupDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUBindGroupEntry.idl \
    $(WebCore)/Modules/WebGPU/GPUBindGroupLayout.idl \
    $(WebCore)/Modules/WebGPU/GPUBindGroupLayoutDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUBindGroupLayoutEntry.idl \
    $(WebCore)/Modules/WebGPU/GPUBlendComponent.idl \
    $(WebCore)/Modules/WebGPU/GPUBlendFactor.idl \
    $(WebCore)/Modules/WebGPU/GPUBlendOperation.idl \
    $(WebCore)/Modules/WebGPU/GPUBlendState.idl \
    $(WebCore)/Modules/WebGPU/GPUBuffer.idl \
    $(WebCore)/Modules/WebGPU/GPUBufferBinding.idl \
    $(WebCore)/Modules/WebGPU/GPUBufferBindingLayout.idl \
    $(WebCore)/Modules/WebGPU/GPUBufferBindingType.idl \
    $(WebCore)/Modules/WebGPU/GPUBufferDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUBufferUsage.idl \
    $(WebCore)/Modules/WebGPU/GPUCanvasCompositingAlphaMode.idl \
    $(WebCore)/Modules/WebGPU/GPUCanvasConfiguration.idl \
    $(WebCore)/Modules/WebGPU/GPUColorDict.idl \
    $(WebCore)/Modules/WebGPU/GPUColorTargetState.idl \
    $(WebCore)/Modules/WebGPU/GPUColorWrite.idl \
    $(WebCore)/Modules/WebGPU/GPUCommandBuffer.idl \
    $(WebCore)/Modules/WebGPU/GPUCommandBufferDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUCommandEncoder.idl \
    $(WebCore)/Modules/WebGPU/GPUCommandEncoderDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUCommandsMixin.idl \
    $(WebCore)/Modules/WebGPU/GPUCompareFunction.idl \
    $(WebCore)/Modules/WebGPU/GPUCompilationInfo.idl \
    $(WebCore)/Modules/WebGPU/GPUCompilationMessage.idl \
    $(WebCore)/Modules/WebGPU/GPUCompilationMessageType.idl \
    $(WebCore)/Modules/WebGPU/GPUComputePassDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUComputePassEncoder.idl \
    $(WebCore)/Modules/WebGPU/GPUComputePassTimestampLocation.idl \
    $(WebCore)/Modules/WebGPU/GPUComputePassTimestampWrite.idl \
    $(WebCore)/Modules/WebGPU/GPUComputePipeline.idl \
    $(WebCore)/Modules/WebGPU/GPUComputePipelineDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUCullMode.idl \
    $(WebCore)/Modules/WebGPU/GPUDebugCommandsMixin.idl \
    $(WebCore)/Modules/WebGPU/GPUDepthStencilState.idl \
    $(WebCore)/Modules/WebGPU/GPUDevice.idl \
    $(WebCore)/Modules/WebGPU/GPUDeviceDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUDeviceError.idl \
    $(WebCore)/Modules/WebGPU/GPUDeviceLost.idl \
    $(WebCore)/Modules/WebGPU/GPUDeviceLostInfo.idl \
    $(WebCore)/Modules/WebGPU/GPUDeviceLostReason.idl \
    $(WebCore)/Modules/WebGPU/GPUDeviceUncapturedError.idl \
    $(WebCore)/Modules/WebGPU/GPUErrorFilter.idl \
    $(WebCore)/Modules/WebGPU/GPUExtent3DDict.idl \
    $(WebCore)/Modules/WebGPU/GPUExternalTexture.idl \
    $(WebCore)/Modules/WebGPU/GPUExternalTextureBindingLayout.idl \
    $(WebCore)/Modules/WebGPU/GPUExternalTextureDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUFeatureName.idl \
    $(WebCore)/Modules/WebGPU/GPUFilterMode.idl \
    $(WebCore)/Modules/WebGPU/GPUFragmentState.idl \
    $(WebCore)/Modules/WebGPU/GPUFrontFace.idl \
    $(WebCore)/Modules/WebGPU/GPUImageCopyBuffer.idl \
    $(WebCore)/Modules/WebGPU/GPUImageCopyExternalImage.idl \
    $(WebCore)/Modules/WebGPU/GPUImageCopyTexture.idl \
    $(WebCore)/Modules/WebGPU/GPUImageCopyTextureTagged.idl \
    $(WebCore)/Modules/WebGPU/GPUImageDataLayout.idl \
    $(WebCore)/Modules/WebGPU/GPUIndexFormat.idl \
    $(WebCore)/Modules/WebGPU/GPULoadOp.idl \
    $(WebCore)/Modules/WebGPU/GPUMapMode.idl \
    $(WebCore)/Modules/WebGPU/GPUMultisampleState.idl \
    $(WebCore)/Modules/WebGPU/GPUObjectBase.idl \
    $(WebCore)/Modules/WebGPU/GPUObjectDescriptorBase.idl \
    $(WebCore)/Modules/WebGPU/GPUOrigin2DDict.idl \
    $(WebCore)/Modules/WebGPU/GPUOrigin3DDict.idl \
    $(WebCore)/Modules/WebGPU/GPUOutOfMemoryError.idl \
    $(WebCore)/Modules/WebGPU/GPUPipelineBase.idl \
    $(WebCore)/Modules/WebGPU/GPUPipelineDescriptorBase.idl \
    $(WebCore)/Modules/WebGPU/GPUPipelineLayout.idl \
    $(WebCore)/Modules/WebGPU/GPUPipelineLayoutDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUPowerPreference.idl \
    $(WebCore)/Modules/WebGPU/GPUPredefinedColorSpace.idl \
    $(WebCore)/Modules/WebGPU/GPUPrimitiveState.idl \
    $(WebCore)/Modules/WebGPU/GPUPrimitiveTopology.idl \
    $(WebCore)/Modules/WebGPU/GPUProgrammablePassEncoder.idl \
    $(WebCore)/Modules/WebGPU/GPUProgrammableStage.idl \
    $(WebCore)/Modules/WebGPU/GPUQuerySet.idl \
    $(WebCore)/Modules/WebGPU/GPUQuerySetDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUQueryType.idl \
    $(WebCore)/Modules/WebGPU/GPUQueue.idl \
    $(WebCore)/Modules/WebGPU/GPURenderBundle.idl \
    $(WebCore)/Modules/WebGPU/GPURenderBundleDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPURenderBundleEncoder.idl \
    $(WebCore)/Modules/WebGPU/GPURenderBundleEncoderDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPURenderEncoderBase.idl \
    $(WebCore)/Modules/WebGPU/GPURenderPassColorAttachment.idl \
    $(WebCore)/Modules/WebGPU/GPURenderPassDepthStencilAttachment.idl \
    $(WebCore)/Modules/WebGPU/GPURenderPassDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPURenderPassEncoder.idl \
    $(WebCore)/Modules/WebGPU/GPURenderPassLayout.idl \
    $(WebCore)/Modules/WebGPU/GPURenderPassTimestampLocation.idl \
    $(WebCore)/Modules/WebGPU/GPURenderPassTimestampWrite.idl \
    $(WebCore)/Modules/WebGPU/GPURenderPipeline.idl \
    $(WebCore)/Modules/WebGPU/GPURenderPipelineDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPURequestAdapterOptions.idl \
    $(WebCore)/Modules/WebGPU/GPUSampler.idl \
    $(WebCore)/Modules/WebGPU/GPUSamplerBindingLayout.idl \
    $(WebCore)/Modules/WebGPU/GPUSamplerBindingType.idl \
    $(WebCore)/Modules/WebGPU/GPUSamplerDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUShaderModule.idl \
    $(WebCore)/Modules/WebGPU/GPUShaderModuleCompilationHint.idl \
    $(WebCore)/Modules/WebGPU/GPUShaderModuleDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUShaderStage.idl \
    $(WebCore)/Modules/WebGPU/GPUStencilFaceState.idl \
    $(WebCore)/Modules/WebGPU/GPUStencilOperation.idl \
    $(WebCore)/Modules/WebGPU/GPUStorageTextureAccess.idl \
    $(WebCore)/Modules/WebGPU/GPUStorageTextureBindingLayout.idl \
    $(WebCore)/Modules/WebGPU/GPUStoreOp.idl \
    $(WebCore)/Modules/WebGPU/GPUSupportedFeatures.idl \
    $(WebCore)/Modules/WebGPU/GPUSupportedLimits.idl \
    $(WebCore)/Modules/WebGPU/GPUTexture.idl \
    $(WebCore)/Modules/WebGPU/GPUTextureAspect.idl \
    $(WebCore)/Modules/WebGPU/GPUTextureBindingLayout.idl \
    $(WebCore)/Modules/WebGPU/GPUTextureDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUTextureDimension.idl \
    $(WebCore)/Modules/WebGPU/GPUTextureFormat.idl \
    $(WebCore)/Modules/WebGPU/GPUTextureSampleType.idl \
    $(WebCore)/Modules/WebGPU/GPUTextureUsage.idl \
    $(WebCore)/Modules/WebGPU/GPUTextureView.idl \
    $(WebCore)/Modules/WebGPU/GPUTextureViewDescriptor.idl \
    $(WebCore)/Modules/WebGPU/GPUTextureViewDimension.idl \
    $(WebCore)/Modules/WebGPU/GPUUncapturedErrorEvent.idl \
    $(WebCore)/Modules/WebGPU/GPUUncapturedErrorEventInit.idl \
    $(WebCore)/Modules/WebGPU/GPUValidationError.idl \
    $(WebCore)/Modules/WebGPU/GPUVertexAttribute.idl \
    $(WebCore)/Modules/WebGPU/GPUVertexBufferLayout.idl \
    $(WebCore)/Modules/WebGPU/GPUVertexFormat.idl \
    $(WebCore)/Modules/WebGPU/GPUVertexState.idl \
    $(WebCore)/Modules/WebGPU/GPUVertexStepMode.idl \
    $(WebCore)/Modules/WebGPU/NavigatorGPU.idl \
    $(WebCore)/Modules/airplay/WebKitPlaybackTargetAvailabilityEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayAutomaticReloadPaymentRequest.idl \
    $(WebCore)/Modules/applepay/ApplePayCancelEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayContactField.idl \
    $(WebCore)/Modules/applepay/ApplePayCouponCodeChangedEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayCouponCodeDetails.idl \
    $(WebCore)/Modules/applepay/ApplePayCouponCodeUpdate.idl \
    $(WebCore)/Modules/applepay/ApplePayDateComponents.idl \
    $(WebCore)/Modules/applepay/ApplePayDateComponentsRange.idl \
    $(WebCore)/Modules/applepay/ApplePayDetailsUpdateBase.idl \
    $(WebCore)/Modules/applepay/ApplePayError.idl \
    $(WebCore)/Modules/applepay/ApplePayErrorCode.idl \
    $(WebCore)/Modules/applepay/ApplePayErrorContactField.idl \
    $(WebCore)/Modules/applepay/ApplePayFeature.idl \
    $(WebCore)/Modules/applepay/ApplePayInstallmentConfiguration.idl \
    $(WebCore)/Modules/applepay/ApplePayInstallmentItem.idl \
    $(WebCore)/Modules/applepay/ApplePayInstallmentItemType.idl \
    $(WebCore)/Modules/applepay/ApplePayInstallmentRetailChannel.idl \
    $(WebCore)/Modules/applepay/ApplePayLineItem.idl \
    $(WebCore)/Modules/applepay/ApplePayMerchantCapability.idl \
    $(WebCore)/Modules/applepay/ApplePayPayment.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentAuthorizationResult.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentAuthorizedEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentContact.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentMethod.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentMethodSelectedEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentMethodType.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentMethodUpdate.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentOrderDetails.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentPass.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentRequest.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentTiming.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentTokenContext.idl \
    $(WebCore)/Modules/applepay/ApplePayRecurringPaymentDateUnit.idl \
    $(WebCore)/Modules/applepay/ApplePayRecurringPaymentRequest.idl \
    $(WebCore)/Modules/applepay/ApplePayRequestBase.idl \
    $(WebCore)/Modules/applepay/ApplePaySession.idl \
    $(WebCore)/Modules/applepay/ApplePaySessionError.idl \
    $(WebCore)/Modules/applepay/ApplePaySetup.idl \
    $(WebCore)/Modules/applepay/ApplePaySetupConfiguration.idl \
    $(WebCore)/Modules/applepay/ApplePaySetupFeature.idl \
    $(WebCore)/Modules/applepay/ApplePaySetupFeatureState.idl \
    $(WebCore)/Modules/applepay/ApplePaySetupFeatureType.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingContactEditingMode.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingContactSelectedEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingContactUpdate.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingMethod.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingMethodSelectedEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingMethodUpdate.idl \
    $(WebCore)/Modules/applepay/ApplePayValidateMerchantEvent.idl \
    $(WebCore)/Modules/applepay/paymentrequest/ApplePayModifier.idl \
    $(WebCore)/Modules/applepay/paymentrequest/ApplePayPaymentCompleteDetails.idl \
    $(WebCore)/Modules/applepay/paymentrequest/ApplePayRequest.idl \
    $(WebCore)/Modules/applepay-ams-ui/ApplePayAMSUIRequest.idl \
    $(WebCore)/Modules/async-clipboard/Clipboard.idl \
    $(WebCore)/Modules/async-clipboard/ClipboardItem.idl \
    $(WebCore)/Modules/async-clipboard/Navigator+Clipboard.idl \
    $(WebCore)/Modules/audiosession/DOMAudioSession.idl \
    $(WebCore)/Modules/audiosession/Navigator+AudioSession.idl \
    $(WebCore)/Modules/badge/Navigator+Badge.idl \
    $(WebCore)/Modules/badge/NavigatorBadge.idl \
    $(WebCore)/Modules/beacon/Navigator+Beacon.idl \
    $(WebCore)/Modules/cache/CacheQueryOptions.idl \
    $(WebCore)/Modules/cache/DOMCache.idl \
    $(WebCore)/Modules/cache/DOMCacheStorage.idl \
    $(WebCore)/Modules/cache/MultiCacheQueryOptions.idl \
    $(WebCore)/Modules/cache/WindowOrWorkerGlobalScope+Caches.idl \
    $(WebCore)/Modules/compression/CompressionStream.idl \
    $(WebCore)/Modules/compression/CompressionStreamEncoder.idl \
    $(WebCore)/Modules/compression/DecompressionStream.idl \
    $(WebCore)/Modules/compression/DecompressionStreamDecoder.idl \
    $(WebCore)/Modules/contact-picker/ContactInfo.idl \
    $(WebCore)/Modules/contact-picker/ContactProperty.idl \
    $(WebCore)/Modules/contact-picker/ContactsManager.idl \
    $(WebCore)/Modules/contact-picker/ContactsSelectOptions.idl \
    $(WebCore)/Modules/contact-picker/Navigator+Contacts.idl \
    $(WebCore)/Modules/cookie-consent/Navigator+CookieConsent.idl \
    $(WebCore)/Modules/cookie-consent/RequestCookieConsentOptions.idl \
    $(WebCore)/Modules/credentialmanagement/BasicCredential.idl \
    $(WebCore)/Modules/credentialmanagement/CredentialCreationOptions.idl \
    $(WebCore)/Modules/credentialmanagement/CredentialRequestOptions.idl \
    $(WebCore)/Modules/credentialmanagement/CredentialsContainer.idl \
    $(WebCore)/Modules/credentialmanagement/Navigator+Credentials.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeyEncryptionScheme.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeyMessageEventInit.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeyMessageEvent.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeyMessageType.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeySession.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeySessionType.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeyStatusMap.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeySystemAccess.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeySystemConfiguration.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeySystemMediaCapability.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeys.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeysRequirement.idl \
    $(WebCore)/Modules/encryptedmedia/Navigator+EME.idl \
    $(WebCore)/Modules/encryptedmedia/legacy/WebKitMediaKeyMessageEvent.idl \
    $(WebCore)/Modules/encryptedmedia/legacy/WebKitMediaKeyNeededEvent.idl \
    $(WebCore)/Modules/encryptedmedia/legacy/WebKitMediaKeySession.idl \
    $(WebCore)/Modules/encryptedmedia/legacy/WebKitMediaKeys.idl \
    $(WebCore)/Modules/entriesapi/DOMFileSystem.idl \
    $(WebCore)/Modules/entriesapi/ErrorCallback.idl \
    $(WebCore)/Modules/entriesapi/FileCallback.idl \
    $(WebCore)/Modules/entriesapi/FileSystemDirectoryEntry.idl \
    $(WebCore)/Modules/entriesapi/FileSystemDirectoryReader.idl \
    $(WebCore)/Modules/entriesapi/FileSystemEntriesCallback.idl \
    $(WebCore)/Modules/entriesapi/FileSystemEntry.idl \
    $(WebCore)/Modules/entriesapi/FileSystemEntryCallback.idl \
    $(WebCore)/Modules/entriesapi/FileSystemFileEntry.idl \
    $(WebCore)/Modules/entriesapi/HTMLInputElement+EntriesAPI.idl \
    $(WebCore)/Modules/fetch/FetchBody.idl \
    $(WebCore)/Modules/fetch/FetchHeaders.idl \
    $(WebCore)/Modules/fetch/FetchReferrerPolicy.idl \
    $(WebCore)/Modules/fetch/FetchRequest.idl \
    $(WebCore)/Modules/fetch/FetchRequestCache.idl \
    $(WebCore)/Modules/fetch/FetchRequestCredentials.idl \
    $(WebCore)/Modules/fetch/FetchRequestDestination.idl \
    $(WebCore)/Modules/fetch/FetchRequestInit.idl \
    $(WebCore)/Modules/fetch/FetchRequestMode.idl \
    $(WebCore)/Modules/fetch/FetchRequestRedirect.idl \
    $(WebCore)/Modules/fetch/FetchResponse.idl \
    $(WebCore)/Modules/fetch/WindowOrWorkerGlobalScope+Fetch.idl \
    $(WebCore)/Modules/filesystemaccess/FileSystemDirectoryHandle.idl \
    $(WebCore)/Modules/filesystemaccess/FileSystemFileHandle.idl \
    $(WebCore)/Modules/filesystemaccess/FileSystemHandle.idl \
    $(WebCore)/Modules/filesystemaccess/FileSystemSyncAccessHandle.idl \
    $(WebCore)/Modules/filesystemaccess/StorageManager+FileSystemAccess.idl \
    $(WebCore)/Modules/gamepad/Gamepad.idl \
    $(WebCore)/Modules/gamepad/GamepadButton.idl \
    $(WebCore)/Modules/gamepad/GamepadEffectParameters.idl \
    $(WebCore)/Modules/gamepad/GamepadEvent.idl \
    $(WebCore)/Modules/gamepad/GamepadHapticActuator.idl \
    $(WebCore)/Modules/gamepad/GamepadHapticEffectType.idl \
    $(WebCore)/Modules/gamepad/Navigator+Gamepad.idl \
    $(WebCore)/Modules/gamepad/WindowEventHandlers+Gamepad.idl \
    $(WebCore)/Modules/geolocation/Geolocation.idl \
    $(WebCore)/Modules/geolocation/GeolocationCoordinates.idl \
    $(WebCore)/Modules/geolocation/GeolocationPosition.idl \
    $(WebCore)/Modules/geolocation/GeolocationPositionError.idl \
    $(WebCore)/Modules/geolocation/Navigator+Geolocation.idl \
    $(WebCore)/Modules/geolocation/PositionCallback.idl \
    $(WebCore)/Modules/geolocation/PositionErrorCallback.idl \
    $(WebCore)/Modules/geolocation/PositionOptions.idl \
    $(WebCore)/Modules/highlight/HighlightRegister.idl \
    $(WebCore)/Modules/highlight/Highlight.idl \
    $(WebCore)/Modules/indexeddb/IDBCursor.idl \
    $(WebCore)/Modules/indexeddb/IDBCursorDirection.idl \
    $(WebCore)/Modules/indexeddb/IDBCursorWithValue.idl \
    $(WebCore)/Modules/indexeddb/IDBDatabase.idl \
    $(WebCore)/Modules/indexeddb/IDBFactory.idl \
    $(WebCore)/Modules/indexeddb/IDBIndex.idl \
    $(WebCore)/Modules/indexeddb/IDBKeyRange.idl \
    $(WebCore)/Modules/indexeddb/IDBObjectStore.idl \
    $(WebCore)/Modules/indexeddb/IDBOpenDBRequest.idl \
    $(WebCore)/Modules/indexeddb/IDBRequest.idl \
    $(WebCore)/Modules/indexeddb/IDBTransaction.idl \
	$(WebCore)/Modules/indexeddb/IDBTransactionDurability.idl \
    $(WebCore)/Modules/indexeddb/IDBTransactionMode.idl \
    $(WebCore)/Modules/indexeddb/IDBVersionChangeEvent.idl \
    $(WebCore)/Modules/indexeddb/WindowOrWorkerGlobalScope+IndexedDatabase.idl \
    $(WebCore)/Modules/mediacapabilities/AudioConfiguration.idl \
    $(WebCore)/Modules/mediacapabilities/ColorGamut.idl \
    $(WebCore)/Modules/mediacapabilities/HdrMetadataType.idl \
    $(WebCore)/Modules/mediacapabilities/MediaCapabilities.idl \
    $(WebCore)/Modules/mediacapabilities/MediaCapabilitiesDecodingInfo.idl \
    $(WebCore)/Modules/mediacapabilities/MediaCapabilitiesEncodingInfo.idl \
    $(WebCore)/Modules/mediacapabilities/MediaCapabilitiesInfo.idl \
    $(WebCore)/Modules/mediacapabilities/MediaConfiguration.idl \
    $(WebCore)/Modules/mediacapabilities/MediaDecodingConfiguration.idl \
    $(WebCore)/Modules/mediacapabilities/MediaDecodingType.idl \
    $(WebCore)/Modules/mediacapabilities/MediaEncodingConfiguration.idl \
    $(WebCore)/Modules/mediacapabilities/MediaEncodingType.idl \
    $(WebCore)/Modules/mediacapabilities/Navigator+MediaCapabilities.idl \
    $(WebCore)/Modules/mediacapabilities/TransferFunction.idl \
    $(WebCore)/Modules/mediacapabilities/VideoConfiguration.idl \
    $(WebCore)/Modules/mediacapabilities/WorkerNavigator+MediaCapabilities.idl \
    $(WebCore)/Modules/mediacontrols/MediaControlsHost.idl \
    $(WebCore)/Modules/mediarecorder/BlobEvent.idl \
    $(WebCore)/Modules/mediarecorder/MediaRecorder.idl \
    $(WebCore)/Modules/mediarecorder/MediaRecorderErrorEvent.idl \
    $(WebCore)/Modules/mediasource/AudioTrack+MediaSource.idl \
    $(WebCore)/Modules/mediasource/DOMURL+MediaSource.idl \
    $(WebCore)/Modules/mediasource/MediaSource.idl \
    $(WebCore)/Modules/mediasource/SourceBuffer.idl \
    $(WebCore)/Modules/mediasource/SourceBufferList.idl \
    $(WebCore)/Modules/mediasource/TextTrack+MediaSource.idl \
    $(WebCore)/Modules/mediasource/VideoPlaybackQuality.idl \
    $(WebCore)/Modules/mediasource/VideoTrack+MediaSource.idl \
    $(WebCore)/Modules/mediasession/MediaImage.idl \
    $(WebCore)/Modules/mediasession/MediaMetadata.idl \
    $(WebCore)/Modules/mediasession/MediaMetadataInit.idl \
    $(WebCore)/Modules/mediasession/MediaMetadataPlaylistMixin.idl \
    $(WebCore)/Modules/mediasession/MediaPositionState.idl \
    $(WebCore)/Modules/mediasession/MediaSession.idl \
    $(WebCore)/Modules/mediasession/MediaSessionAction.idl \
    $(WebCore)/Modules/mediasession/MediaSessionActionDetails.idl \
    $(WebCore)/Modules/mediasession/MediaSessionActionHandler.idl \
    $(WebCore)/Modules/mediasession/MediaSessionCoordinator.idl \
    $(WebCore)/Modules/mediasession/MediaSessionCoordinatorMixin.idl \
    $(WebCore)/Modules/mediasession/MediaSessionCoordinatorState.idl \
    $(WebCore)/Modules/mediasession/MediaSessionPlaybackState.idl \
    $(WebCore)/Modules/mediasession/MediaSessionPlaylistMixin.idl \
    $(WebCore)/Modules/mediasession/MediaSessionReadyState.idl \
    $(WebCore)/Modules/mediasession/Navigator+MediaSession.idl \
    $(WebCore)/Modules/mediastream/CanvasCaptureMediaStreamTrack.idl \
    $(WebCore)/Modules/mediastream/DoubleRange.idl \
    $(WebCore)/Modules/mediastream/LongRange.idl \
    $(WebCore)/Modules/mediastream/MediaDeviceInfo.idl \
    $(WebCore)/Modules/mediastream/MediaDevices.idl \
    $(WebCore)/Modules/mediastream/MediaStream.idl \
    $(WebCore)/Modules/mediastream/MediaStreamTrack.idl \
    $(WebCore)/Modules/mediastream/MediaStreamTrackEvent.idl \
    $(WebCore)/Modules/mediastream/MediaTrackConstraints.idl \
    $(WebCore)/Modules/mediastream/MediaTrackSupportedConstraints.idl \
    $(WebCore)/Modules/mediastream/Navigator+MediaDevices.idl \
    $(WebCore)/Modules/mediastream/OverconstrainedError.idl \
    $(WebCore)/Modules/mediastream/OverconstrainedErrorEvent.idl \
    $(WebCore)/Modules/mediastream/RTCAnswerOptions.idl \
    $(WebCore)/Modules/mediastream/RTCCertificate.idl \
    $(WebCore)/Modules/mediastream/RTCConfiguration.idl \
    $(WebCore)/Modules/mediastream/RTCDTMFSender.idl \
    $(WebCore)/Modules/mediastream/RTCDTMFToneChangeEvent.idl \
    $(WebCore)/Modules/mediastream/RTCDataChannel.idl \
    $(WebCore)/Modules/mediastream/RTCDataChannelEvent.idl \
    $(WebCore)/Modules/mediastream/RTCDegradationPreference.idl \
    $(WebCore)/Modules/mediastream/RTCDtlsTransport.idl \
    $(WebCore)/Modules/mediastream/RTCDtlsTransportState.idl \
    $(WebCore)/Modules/mediastream/RTCDtxStatus.idl \
    $(WebCore)/Modules/mediastream/RTCEncodedAudioFrame.idl \
    $(WebCore)/Modules/mediastream/RTCEncodedVideoFrame.idl \
    $(WebCore)/Modules/mediastream/RTCError.idl \
    $(WebCore)/Modules/mediastream/RTCErrorDetailType.idl \
    $(WebCore)/Modules/mediastream/RTCErrorEvent.idl \
    $(WebCore)/Modules/mediastream/RTCIceCandidate.idl \
    $(WebCore)/Modules/mediastream/RTCIceCandidateInit.idl \
    $(WebCore)/Modules/mediastream/RTCIceCandidateType.idl \
    $(WebCore)/Modules/mediastream/RTCIceComponent.idl \
    $(WebCore)/Modules/mediastream/RTCIceConnectionState.idl \
    $(WebCore)/Modules/mediastream/RTCIceGatheringState.idl \
    $(WebCore)/Modules/mediastream/RTCIceProtocol.idl \
    $(WebCore)/Modules/mediastream/RTCIceServer.idl \
    $(WebCore)/Modules/mediastream/RTCIceTcpCandidateType.idl \
    $(WebCore)/Modules/mediastream/RTCIceTransport.idl \
    $(WebCore)/Modules/mediastream/RTCIceTransportState.idl \
    $(WebCore)/Modules/mediastream/RTCLocalSessionDescriptionInit.idl \
    $(WebCore)/Modules/mediastream/RTCOfferAnswerOptions.idl \
    $(WebCore)/Modules/mediastream/RTCOfferOptions.idl \
    $(WebCore)/Modules/mediastream/RTCPeerConnection.idl \
    $(WebCore)/Modules/mediastream/RTCPeerConnectionIceEvent.idl \
    $(WebCore)/Modules/mediastream/RTCPeerConnectionIceErrorEvent.idl \
    $(WebCore)/Modules/mediastream/RTCPeerConnectionState.idl \
    $(WebCore)/Modules/mediastream/RTCPriorityType.idl \
    $(WebCore)/Modules/mediastream/RTCRtcpParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpCapabilities.idl \
    $(WebCore)/Modules/mediastream/RTCRtpCodecCapability.idl \
    $(WebCore)/Modules/mediastream/RTCRtpCodecParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpCodingParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpContributingSource.idl \
    $(WebCore)/Modules/mediastream/RTCRtpEncodingParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpFecParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpHeaderExtensionParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpReceiver.idl \
    $(WebCore)/Modules/mediastream/RTCRtpReceiver+Transform.idl \
    $(WebCore)/Modules/mediastream/RTCRtpRtxParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpSendParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpSender.idl \
    $(WebCore)/Modules/mediastream/RTCRtpSender+Transform.idl \
    $(WebCore)/Modules/mediastream/RTCRtpSFrameTransform.idl \
    $(WebCore)/Modules/mediastream/RTCRtpSFrameTransformErrorEvent.idl \
    $(WebCore)/Modules/mediastream/RTCRtpScriptTransform.idl \
    $(WebCore)/Modules/mediastream/RTCRtpScriptTransformProvider.idl \
    $(WebCore)/Modules/mediastream/RTCRtpScriptTransformer.idl \
    $(WebCore)/Modules/mediastream/RTCRtpSynchronizationSource.idl \
    $(WebCore)/Modules/mediastream/RTCRtpTransceiver.idl \
    $(WebCore)/Modules/mediastream/RTCRtpTransceiverDirection.idl \
    $(WebCore)/Modules/mediastream/RTCSctpTransport.idl \
    $(WebCore)/Modules/mediastream/RTCSctpTransportState.idl \
    $(WebCore)/Modules/mediastream/RTCSdpType.idl \
    $(WebCore)/Modules/mediastream/RTCSessionDescription.idl \
    $(WebCore)/Modules/mediastream/RTCSessionDescriptionInit.idl \
    $(WebCore)/Modules/mediastream/RTCSignalingState.idl \
    $(WebCore)/Modules/mediastream/RTCStatsReport.idl \
    $(WebCore)/Modules/mediastream/RTCTrackEvent.idl \
    $(WebCore)/Modules/mediastream/RTCTransformEvent.idl \
    $(WebCore)/Modules/model-element/HTMLModelElement.idl \
    $(WebCore)/Modules/model-element/HTMLModelElementCamera.idl \
    $(WebCore)/Modules/notifications/Notification.idl \
    $(WebCore)/Modules/notifications/NotificationDirection.idl \
    $(WebCore)/Modules/notifications/NotificationEvent.idl \
    $(WebCore)/Modules/notifications/NotificationOptions.idl \
    $(WebCore)/Modules/notifications/NotificationPermission.idl \
    $(WebCore)/Modules/notifications/NotificationPermissionCallback.idl \
    $(WebCore)/Modules/paymentrequest/AddressErrors.idl \
    $(WebCore)/Modules/paymentrequest/MerchantValidationEvent.idl \
    $(WebCore)/Modules/paymentrequest/PayerErrorFields.idl \
    $(WebCore)/Modules/paymentrequest/PaymentAddress.idl \
    $(WebCore)/Modules/paymentrequest/PaymentComplete.idl \
    $(WebCore)/Modules/paymentrequest/PaymentCompleteDetails.idl \
    $(WebCore)/Modules/paymentrequest/PaymentCurrencyAmount.idl \
    $(WebCore)/Modules/paymentrequest/PaymentDetailsBase.idl \
    $(WebCore)/Modules/paymentrequest/PaymentDetailsInit.idl \
    $(WebCore)/Modules/paymentrequest/PaymentDetailsModifier.idl \
    $(WebCore)/Modules/paymentrequest/PaymentDetailsUpdate.idl \
    $(WebCore)/Modules/paymentrequest/PaymentItem.idl \
    $(WebCore)/Modules/paymentrequest/PaymentMethodChangeEvent.idl \
    $(WebCore)/Modules/paymentrequest/PaymentMethodData.idl \
    $(WebCore)/Modules/paymentrequest/PaymentOptions.idl \
    $(WebCore)/Modules/paymentrequest/PaymentRequest.idl \
    $(WebCore)/Modules/paymentrequest/PaymentRequestUpdateEvent.idl \
    $(WebCore)/Modules/paymentrequest/PaymentRequestUpdateEventInit.idl \
    $(WebCore)/Modules/paymentrequest/PaymentResponse.idl \
    $(WebCore)/Modules/paymentrequest/PaymentShippingOption.idl \
    $(WebCore)/Modules/paymentrequest/PaymentShippingType.idl \
    $(WebCore)/Modules/paymentrequest/PaymentValidationErrors.idl \
    $(WebCore)/Modules/permissions/Navigator+Permissions.idl \
    $(WebCore)/Modules/permissions/PermissionDescriptor.idl \
    $(WebCore)/Modules/permissions/PermissionName.idl \
    $(WebCore)/Modules/permissions/PermissionState.idl \
    $(WebCore)/Modules/permissions/PermissionStatus.idl \
    $(WebCore)/Modules/permissions/Permissions.idl \
    $(WebCore)/Modules/permissions/WorkerNavigator+Permissions.idl \
    $(WebCore)/Modules/pictureinpicture/DocumentOrShadowRoot+PictureInPicture.idl \
    $(WebCore)/Modules/pictureinpicture/Document+PictureInPicture.idl \
    $(WebCore)/Modules/pictureinpicture/HTMLVideoElement+PictureInPicture.idl \
    $(WebCore)/Modules/pictureinpicture/PictureInPictureEvent.idl \
    $(WebCore)/Modules/pictureinpicture/PictureInPictureWindow.idl \
    $(WebCore)/Modules/push-api/PushEncryptionKeyName.idl \
    $(WebCore)/Modules/push-api/PushEvent.idl \
    $(WebCore)/Modules/push-api/PushEventInit.idl \
    $(WebCore)/Modules/push-api/PushManager.idl \
    $(WebCore)/Modules/push-api/PushMessageData.idl \
    $(WebCore)/Modules/push-api/PushPermissionState.idl \
    $(WebCore)/Modules/push-api/PushSubscription.idl \
    $(WebCore)/Modules/push-api/PushSubscriptionChangeEvent.idl \
    $(WebCore)/Modules/push-api/PushSubscriptionChangeEventInit.idl \
    $(WebCore)/Modules/push-api/PushSubscriptionJSON.idl \
    $(WebCore)/Modules/push-api/PushSubscriptionOptions.idl \
    $(WebCore)/Modules/push-api/PushSubscriptionOptionsInit.idl \
    $(WebCore)/Modules/push-api/ServiceWorkerGlobalScope+PushAPI.idl \
    $(WebCore)/Modules/push-api/ServiceWorkerRegistration+PushAPI.idl \
    $(WebCore)/Modules/remoteplayback/HTMLMediaElement+RemotePlayback.idl \
    $(WebCore)/Modules/remoteplayback/RemotePlayback.idl \
    $(WebCore)/Modules/remoteplayback/RemotePlaybackAvailabilityCallback.idl \
    $(WebCore)/Modules/reporting/DeprecationReportBody.idl \
    $(WebCore)/Modules/reporting/Report.idl \
    $(WebCore)/Modules/reporting/ReportBody.idl \
    $(WebCore)/Modules/reporting/ReportingObserver.idl \
    $(WebCore)/Modules/reporting/ReportingObserverCallback.idl \
    $(WebCore)/Modules/reporting/TestReportBody.idl \
    $(WebCore)/Modules/screen-wake-lock/Navigator+ScreenWakeLock.idl \
    $(WebCore)/Modules/screen-wake-lock/WakeLock.idl \
    $(WebCore)/Modules/screen-wake-lock/WakeLockSentinel.idl \
    $(WebCore)/Modules/screen-wake-lock/WakeLockType.idl \
    $(WebCore)/Modules/speech/DOMWindow+SpeechSynthesis.idl \
    $(WebCore)/Modules/speech/SpeechSynthesis.idl \
    $(WebCore)/Modules/speech/SpeechSynthesisErrorCode.idl \
    $(WebCore)/Modules/speech/SpeechSynthesisErrorEvent.idl \
    $(WebCore)/Modules/speech/SpeechSynthesisErrorEventInit.idl \
    $(WebCore)/Modules/speech/SpeechSynthesisEvent.idl \
    $(WebCore)/Modules/speech/SpeechSynthesisEventInit.idl \
    $(WebCore)/Modules/speech/SpeechSynthesisUtterance.idl \
    $(WebCore)/Modules/speech/SpeechSynthesisVoice.idl \
    $(WebCore)/Modules/speech/SpeechRecognition.idl \
    $(WebCore)/Modules/speech/SpeechRecognitionAlternative.idl \
    $(WebCore)/Modules/speech/SpeechRecognitionErrorCode.idl \
    $(WebCore)/Modules/speech/SpeechRecognitionErrorEvent.idl \
    $(WebCore)/Modules/speech/SpeechRecognitionEvent.idl \
    $(WebCore)/Modules/speech/SpeechRecognitionResult.idl \
    $(WebCore)/Modules/speech/SpeechRecognitionResultList.idl \
    $(WebCore)/Modules/streams/ByteLengthQueuingStrategy.idl \
    $(WebCore)/Modules/streams/CountQueuingStrategy.idl \
    $(WebCore)/Modules/streams/GenericTransformStream.idl \
    $(WebCore)/Modules/streams/ReadableByteStreamController.idl \
    $(WebCore)/Modules/streams/ReadableStream.idl \
    $(WebCore)/Modules/streams/ReadableStreamBYOBReader.idl \
    $(WebCore)/Modules/streams/ReadableStreamBYOBRequest.idl \
    $(WebCore)/Modules/streams/ReadableStreamDefaultController.idl \
    $(WebCore)/Modules/streams/ReadableStreamDefaultReader.idl \
    $(WebCore)/Modules/streams/ReadableStreamSink.idl \
    $(WebCore)/Modules/streams/ReadableStreamSource.idl \
    $(WebCore)/Modules/streams/TransformStream.idl \
    $(WebCore)/Modules/streams/TransformStreamDefaultController.idl \
    $(WebCore)/Modules/streams/WritableStream.idl \
    $(WebCore)/Modules/streams/WritableStreamDefaultController.idl \
    $(WebCore)/Modules/streams/WritableStreamDefaultWriter.idl \
    $(WebCore)/Modules/streams/WritableStreamSink.idl \
    $(WebCore)/Modules/storage/StorageManager.idl \
    $(WebCore)/Modules/web-locks/NavigatorLocks.idl \
    $(WebCore)/Modules/web-locks/WebLock.idl \
    $(WebCore)/Modules/web-locks/WebLockGrantedCallback.idl \
    $(WebCore)/Modules/web-locks/WebLockManager.idl \
    $(WebCore)/Modules/web-locks/WebLockManagerSnapshot.idl \
    $(WebCore)/Modules/web-locks/WebLockMode.idl \
    $(WebCore)/Modules/webaudio/AnalyserNode.idl \
    $(WebCore)/Modules/webaudio/AnalyserOptions.idl \
    $(WebCore)/Modules/webaudio/AudioBuffer.idl \
    $(WebCore)/Modules/webaudio/AudioBufferCallback.idl \
    $(WebCore)/Modules/webaudio/AudioBufferOptions.idl \
    $(WebCore)/Modules/webaudio/AudioBufferSourceNode.idl \
    $(WebCore)/Modules/webaudio/AudioBufferSourceOptions.idl \
    $(WebCore)/Modules/webaudio/AudioContext.idl \
    $(WebCore)/Modules/webaudio/AudioContextLatencyCategory.idl \
    $(WebCore)/Modules/webaudio/AudioContextOptions.idl \
    $(WebCore)/Modules/webaudio/AudioContextState.idl \
    $(WebCore)/Modules/webaudio/AudioDestinationNode.idl \
    $(WebCore)/Modules/webaudio/AudioListener.idl \
    $(WebCore)/Modules/webaudio/AudioNode.idl \
    $(WebCore)/Modules/webaudio/AudioNodeOptions.idl \
    $(WebCore)/Modules/webaudio/AudioParam.idl \
    $(WebCore)/Modules/webaudio/AudioParamDescriptor.idl \
    $(WebCore)/Modules/webaudio/AudioParamMap.idl \
    $(WebCore)/Modules/webaudio/AudioProcessingEvent.idl \
    $(WebCore)/Modules/webaudio/AudioProcessingEventInit.idl \
    $(WebCore)/Modules/webaudio/AudioScheduledSourceNode.idl \
    $(WebCore)/Modules/webaudio/AudioTimestamp.idl \
    $(WebCore)/Modules/webaudio/AudioWorklet.idl \
    $(WebCore)/Modules/webaudio/AudioWorkletGlobalScope.idl \
    $(WebCore)/Modules/webaudio/AudioWorkletNode.idl \
    $(WebCore)/Modules/webaudio/AudioWorkletNodeOptions.idl \
    $(WebCore)/Modules/webaudio/AudioWorkletProcessor.idl \
    $(WebCore)/Modules/webaudio/AudioWorkletProcessorConstructor.idl \
    $(WebCore)/Modules/webaudio/AutomationRate.idl \
    $(WebCore)/Modules/webaudio/BaseAudioContext.idl \
    $(WebCore)/Modules/webaudio/BiquadFilterNode.idl \
    $(WebCore)/Modules/webaudio/BiquadFilterOptions.idl \
    $(WebCore)/Modules/webaudio/BiquadFilterType.idl \
    $(WebCore)/Modules/webaudio/ChannelCountMode.idl \
    $(WebCore)/Modules/webaudio/ChannelInterpretation.idl \
    $(WebCore)/Modules/webaudio/ChannelMergerNode.idl \
    $(WebCore)/Modules/webaudio/ChannelMergerOptions.idl \
    $(WebCore)/Modules/webaudio/ChannelSplitterNode.idl \
    $(WebCore)/Modules/webaudio/ChannelSplitterOptions.idl \
    $(WebCore)/Modules/webaudio/ConstantSourceNode.idl \
    $(WebCore)/Modules/webaudio/ConstantSourceOptions.idl \
    $(WebCore)/Modules/webaudio/ConvolverNode.idl \
    $(WebCore)/Modules/webaudio/ConvolverOptions.idl \
    $(WebCore)/Modules/webaudio/DelayNode.idl \
    $(WebCore)/Modules/webaudio/DelayOptions.idl \
    $(WebCore)/Modules/webaudio/DistanceModelType.idl \
    $(WebCore)/Modules/webaudio/DynamicsCompressorNode.idl \
    $(WebCore)/Modules/webaudio/DynamicsCompressorOptions.idl \
    $(WebCore)/Modules/webaudio/GainNode.idl \
    $(WebCore)/Modules/webaudio/GainOptions.idl \
    $(WebCore)/Modules/webaudio/IIRFilterNode.idl \
    $(WebCore)/Modules/webaudio/IIRFilterOptions.idl \
    $(WebCore)/Modules/webaudio/MediaElementAudioSourceNode.idl \
    $(WebCore)/Modules/webaudio/MediaElementAudioSourceOptions.idl \
    $(WebCore)/Modules/webaudio/MediaStreamAudioDestinationNode.idl \
    $(WebCore)/Modules/webaudio/MediaStreamAudioSourceNode.idl \
    $(WebCore)/Modules/webaudio/MediaStreamAudioSourceOptions.idl \
    $(WebCore)/Modules/webaudio/OfflineAudioCompletionEvent.idl \
    $(WebCore)/Modules/webaudio/OfflineAudioCompletionEventInit.idl \
    $(WebCore)/Modules/webaudio/OfflineAudioContext.idl \
    $(WebCore)/Modules/webaudio/OfflineAudioContextOptions.idl \
    $(WebCore)/Modules/webaudio/OscillatorNode.idl \
    $(WebCore)/Modules/webaudio/OscillatorOptions.idl \
    $(WebCore)/Modules/webaudio/OscillatorType.idl \
    $(WebCore)/Modules/webaudio/OverSampleType.idl \
    $(WebCore)/Modules/webaudio/PannerNode.idl \
    $(WebCore)/Modules/webaudio/PannerOptions.idl \
    $(WebCore)/Modules/webaudio/PanningModelType.idl \
    $(WebCore)/Modules/webaudio/PeriodicWave.idl \
    $(WebCore)/Modules/webaudio/PeriodicWaveConstraints.idl \
    $(WebCore)/Modules/webaudio/PeriodicWaveOptions.idl \
    $(WebCore)/Modules/webaudio/ScriptProcessorNode.idl \
    $(WebCore)/Modules/webaudio/StereoPannerNode.idl \
    $(WebCore)/Modules/webaudio/StereoPannerOptions.idl \
    $(WebCore)/Modules/webaudio/WaveShaperNode.idl \
    $(WebCore)/Modules/webaudio/WaveShaperOptions.idl \
    $(WebCore)/Modules/webauthn/AttestationConveyancePreference.idl \
    $(WebCore)/Modules/webauthn/AuthenticationExtensionsClientInputs.idl \
    $(WebCore)/Modules/webauthn/AuthenticationExtensionsClientOutputs.idl \
    $(WebCore)/Modules/webauthn/AuthenticatorAssertionResponse.idl \
    $(WebCore)/Modules/webauthn/AuthenticatorAttachment.idl \
    $(WebCore)/Modules/webauthn/AuthenticatorAttestationResponse.idl \
    $(WebCore)/Modules/webauthn/AuthenticatorResponse.idl \
    $(WebCore)/Modules/webauthn/AuthenticatorTransport.idl \
    $(WebCore)/Modules/webauthn/PublicKeyCredential.idl \
    $(WebCore)/Modules/webauthn/PublicKeyCredentialCreationOptions.idl \
    $(WebCore)/Modules/webauthn/PublicKeyCredentialDescriptor.idl \
    $(WebCore)/Modules/webauthn/PublicKeyCredentialRequestOptions.idl \
    $(WebCore)/Modules/webauthn/PublicKeyCredentialType.idl \
    $(WebCore)/Modules/webauthn/ResidentKeyRequirement.idl \
    $(WebCore)/Modules/webauthn/UserVerificationRequirement.idl \
    $(WebCore)/Modules/webcodecs/AvcEncoderConfig.idl \
    $(WebCore)/Modules/webcodecs/BitrateMode.idl \
    $(WebCore)/Modules/webcodecs/LatencyMode.idl \
    $(WebCore)/Modules/webcodecs/HardwareAcceleration.idl \
    $(WebCore)/Modules/webcodecs/PlaneLayout.idl \
    $(WebCore)/Modules/webcodecs/VideoColorPrimaries.idl \
    $(WebCore)/Modules/webcodecs/VideoColorSpace.idl \
    $(WebCore)/Modules/webcodecs/VideoColorSpaceInit.idl \
    $(WebCore)/Modules/webcodecs/VideoMatrixCoefficients.idl \
    $(WebCore)/Modules/webcodecs/VideoPixelFormat.idl \
    $(WebCore)/Modules/webcodecs/VideoTransferCharacteristics.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsAlphaOption.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsEncodedVideoChunk.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsEncodedVideoChunkMetadata.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsEncodedVideoChunkOutputCallback.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsEncodedVideoChunkType.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsErrorCallback.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsCodecState.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsVideoDecoder.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsVideoDecoderConfig.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsVideoDecoderSupport.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsVideoEncoder.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsVideoEncoderConfig.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsVideoEncoderEncodeOptions.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsVideoEncoderSupport.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsVideoFrame.idl \
    $(WebCore)/Modules/webcodecs/WebCodecsVideoFrameOutputCallback.idl \
    $(WebCore)/Modules/webdatabase/DOMWindow+WebDatabase.idl \
    $(WebCore)/Modules/webdatabase/Database.idl \
    $(WebCore)/Modules/webdatabase/DatabaseCallback.idl \
    $(WebCore)/Modules/webdatabase/SQLError.idl \
    $(WebCore)/Modules/webdatabase/SQLResultSet.idl \
    $(WebCore)/Modules/webdatabase/SQLResultSetRowList.idl \
    $(WebCore)/Modules/webdatabase/SQLStatementCallback.idl \
    $(WebCore)/Modules/webdatabase/SQLStatementErrorCallback.idl \
    $(WebCore)/Modules/webdatabase/SQLTransaction.idl \
    $(WebCore)/Modules/webdatabase/SQLTransactionCallback.idl \
    $(WebCore)/Modules/webdatabase/SQLTransactionErrorCallback.idl \
    $(WebCore)/Modules/webdriver/Navigator+WebDriver.idl \
    $(WebCore)/Modules/websockets/CloseEvent.idl \
    $(WebCore)/Modules/websockets/WebSocket.idl \
    $(WebCore)/Modules/webxr/Navigator+WebXR.idl \
    $(WebCore)/Modules/webxr/WebXRBoundedReferenceSpace.idl \
    $(WebCore)/Modules/webxr/WebXRFrame+HandInput.idl \
    $(WebCore)/Modules/webxr/WebXRFrame.idl \
    $(WebCore)/Modules/webxr/WebXRHand.idl \
    $(WebCore)/Modules/webxr/WebXRInputSource+Gamepad.idl \
    $(WebCore)/Modules/webxr/WebXRInputSource.idl \
    $(WebCore)/Modules/webxr/WebXRInputSourceArray.idl \
    $(WebCore)/Modules/webxr/WebXRInputSource+HandInput.idl \
    $(WebCore)/Modules/webxr/WebXRJointPose.idl \
    $(WebCore)/Modules/webxr/WebXRJointSpace.idl \
    $(WebCore)/Modules/webxr/WebXRLayer.idl \
    $(WebCore)/Modules/webxr/WebXRPose.idl \
    $(WebCore)/Modules/webxr/WebXRReferenceSpace.idl \
    $(WebCore)/Modules/webxr/WebXRRenderState.idl \
    $(WebCore)/Modules/webxr/WebXRRigidTransform.idl \
    $(WebCore)/Modules/webxr/WebXRSession+AR.idl \
    $(WebCore)/Modules/webxr/WebXRSession.idl \
    $(WebCore)/Modules/webxr/WebXRSpace.idl \
    $(WebCore)/Modules/webxr/WebXRSystem.idl \
    $(WebCore)/Modules/webxr/WebXRViewerPose.idl \
    $(WebCore)/Modules/webxr/WebXRView.idl \
    $(WebCore)/Modules/webxr/WebXRViewport.idl \
    $(WebCore)/Modules/webxr/WebXRWebGLLayer.idl \
    $(WebCore)/Modules/webxr/XREnvironmentBlendMode.idl \
    $(WebCore)/Modules/webxr/XREye.idl \
    $(WebCore)/Modules/webxr/XRFrameRequestCallback.idl \
    $(WebCore)/Modules/webxr/XRHandJoint.idl \
    $(WebCore)/Modules/webxr/XRHandedness.idl \
    $(WebCore)/Modules/webxr/XRInputSourceEvent.idl \
    $(WebCore)/Modules/webxr/XRInputSourcesChangeEvent.idl \
    $(WebCore)/Modules/webxr/XRInteractionMode.idl \
    $(WebCore)/Modules/webxr/XRReferenceSpaceEvent.idl \
    $(WebCore)/Modules/webxr/XRReferenceSpaceType.idl \
    $(WebCore)/Modules/webxr/XRRenderStateInit.idl \
    $(WebCore)/Modules/webxr/XRSessionEvent.idl \
    $(WebCore)/Modules/webxr/XRSessionInit.idl \
    $(WebCore)/Modules/webxr/XRSessionMode.idl \
    $(WebCore)/Modules/webxr/XRTargetRayMode.idl \
    $(WebCore)/Modules/webxr/XRVisibilityState.idl \
    $(WebCore)/Modules/webxr/XRWebGLLayerInit.idl \
    $(WebCore)/accessibility/AccessibilityRole.idl \
    $(WebCore)/accessibility/AriaAttributes.idl \
    $(WebCore)/animation/Animatable.idl \
    $(WebCore)/animation/AnimationEffect.idl \
    $(WebCore)/animation/AnimationFrameProvider.idl \
    $(WebCore)/animation/AnimationFrameRatePreset.idl \
    $(WebCore)/animation/AnimationPlaybackEvent.idl \
    $(WebCore)/animation/AnimationPlaybackEventInit.idl \
    $(WebCore)/animation/AnimationTimeline.idl \
    $(WebCore)/animation/CSSAnimation.idl \
    $(WebCore)/animation/CSSAnimationEvent.idl \
    $(WebCore)/animation/CSSTransition.idl \
    $(WebCore)/animation/CSSTransitionEvent.idl \
    $(WebCore)/animation/CompositeOperation.idl \
    $(WebCore)/animation/CompositeOperationOrAuto.idl \
    $(WebCore)/animation/ComputedEffectTiming.idl \
    $(WebCore)/animation/CustomAnimationOptions.idl \
    $(WebCore)/animation/CustomEffect.idl \
    $(WebCore)/animation/CustomEffectCallback.idl \
    $(WebCore)/animation/Document+WebAnimations.idl \
    $(WebCore)/animation/DocumentOrShadowRoot+WebAnimations.idl \
    $(WebCore)/animation/DocumentTimeline.idl \
    $(WebCore)/animation/DocumentTimelineOptions.idl \
    $(WebCore)/animation/EffectTiming.idl \
    $(WebCore)/animation/FillMode.idl \
    $(WebCore)/animation/GetAnimationsOptions.idl \
    $(WebCore)/animation/GlobalEventHandlers+CSSAnimations.idl \
    $(WebCore)/animation/GlobalEventHandlers+CSSTransitions.idl \
    $(WebCore)/animation/IterationCompositeOperation.idl \
    $(WebCore)/animation/KeyframeAnimationOptions.idl \
    $(WebCore)/animation/KeyframeEffect.idl \
    $(WebCore)/animation/KeyframeEffectOptions.idl \
    $(WebCore)/animation/OptionalEffectTiming.idl \
    $(WebCore)/animation/PlaybackDirection.idl \
    $(WebCore)/animation/WebAnimation.idl \
    $(WebCore)/crypto/CryptoAlgorithmParameters.idl \
    $(WebCore)/crypto/CryptoKey.idl \
    $(WebCore)/crypto/CryptoKeyPair.idl \
    $(WebCore)/crypto/CryptoKeyUsage.idl \
    $(WebCore)/crypto/JsonWebKey.idl \
    $(WebCore)/crypto/RsaOtherPrimesInfo.idl \
    $(WebCore)/crypto/SubtleCrypto.idl \
    $(WebCore)/crypto/keys/CryptoAesKeyAlgorithm.idl \
    $(WebCore)/crypto/keys/CryptoEcKeyAlgorithm.idl \
    $(WebCore)/crypto/keys/CryptoHmacKeyAlgorithm.idl \
    $(WebCore)/crypto/keys/CryptoKeyAlgorithm.idl \
    $(WebCore)/crypto/keys/CryptoRsaHashedKeyAlgorithm.idl \
    $(WebCore)/crypto/keys/CryptoRsaKeyAlgorithm.idl \
    $(WebCore)/crypto/parameters/AesCbcCfbParams.idl \
    $(WebCore)/crypto/parameters/AesCtrParams.idl \
    $(WebCore)/crypto/parameters/AesGcmParams.idl \
    $(WebCore)/crypto/parameters/AesKeyParams.idl \
    $(WebCore)/crypto/parameters/EcKeyParams.idl \
    $(WebCore)/crypto/parameters/EcdhKeyDeriveParams.idl \
    $(WebCore)/crypto/parameters/EcdsaParams.idl \
    $(WebCore)/crypto/parameters/HkdfParams.idl \
    $(WebCore)/crypto/parameters/HmacKeyParams.idl \
    $(WebCore)/crypto/parameters/Pbkdf2Params.idl \
    $(WebCore)/crypto/parameters/RsaHashedImportParams.idl \
    $(WebCore)/crypto/parameters/RsaHashedKeyGenParams.idl \
    $(WebCore)/crypto/parameters/RsaKeyGenParams.idl \
    $(WebCore)/crypto/parameters/RsaOaepParams.idl \
    $(WebCore)/crypto/parameters/RsaPssParams.idl \
    $(WebCore)/css/CSSConditionRule.idl \
    $(WebCore)/css/CSSContainerRule.idl \
    $(WebCore)/css/CSSCounterStyleRule.idl \
    $(WebCore)/css/CSSFontFaceRule.idl \
    $(WebCore)/css/CSSFontFeatureValuesRule.idl \
    $(WebCore)/css/CSSFontPaletteValuesRule.idl \
    $(WebCore)/css/CSSGroupingRule.idl \
    $(WebCore)/css/CSSImportRule.idl \
    $(WebCore)/css/CSSImportRule+Layer.idl \
    $(WebCore)/css/CSSLayerBlockRule.idl \
    $(WebCore)/css/CSSLayerStatementRule.idl \
    $(WebCore)/css/CSSKeyframeRule.idl \
    $(WebCore)/css/CSSKeyframesRule.idl \
    $(WebCore)/css/CSSMediaRule.idl \
    $(WebCore)/css/CSSNamespaceRule.idl \
    $(WebCore)/css/CSSPageRule.idl \
    $(WebCore)/css/CSSPaintCallback.idl \
    $(WebCore)/css/CSSPaintSize.idl \
    $(WebCore)/css/CSSPropertyRule.idl \
    $(WebCore)/css/CSSRule.idl \
    $(WebCore)/css/CSSRuleList.idl \
    $(WebCore)/css/CSSStyleDeclaration.idl \
    $(WebCore)/css/CSSStyleRule.idl \
    $(WebCore)/css/CSSStyleSheet.idl \
    $(WebCore)/css/CSSSupportsRule.idl \
    $(WebCore)/css/CSSUnknownRule.idl \
    $(WebCore)/css/DocumentOrShadowRoot+CSSOM.idl \
    $(WebCore)/css/DOMCSSCustomPropertyDescriptor.idl \
    $(WebCore)/css/DOMCSSNamespace.idl \
    $(WebCore)/css/DOMCSSNamespace+CSSNumericFactory.idl \
    $(WebCore)/css/DOMCSSNamespace+CSSPainting.idl \
    $(WebCore)/css/DOMCSSNamespace+CSSPropertiesandValues.idl \
    $(WebCore)/css/DOMMatrix.idl \
    $(WebCore)/css/DOMMatrix2DInit.idl \
    $(WebCore)/css/DOMMatrixInit.idl \
    $(WebCore)/css/DOMMatrixReadOnly.idl \
    $(WebCore)/css/DeprecatedCSSOMCounter.idl \
    $(WebCore)/css/DeprecatedCSSOMPrimitiveValue.idl \
    $(WebCore)/css/DeprecatedCSSOMRGBColor.idl \
    $(WebCore)/css/DeprecatedCSSOMRect.idl \
    $(WebCore)/css/DeprecatedCSSOMValue.idl \
    $(WebCore)/css/DeprecatedCSSOMValueList.idl \
    $(WebCore)/css/ElementCSSInlineStyle.idl \
    $(WebCore)/css/FontFace.idl \
    $(WebCore)/css/FontFaceSet.idl \
    $(WebCore)/css/FontFaceSource.idl \
    $(WebCore)/css/LinkStyle.idl \
    $(WebCore)/css/MediaList.idl \
    $(WebCore)/css/MediaQueryList.idl \
    $(WebCore)/css/MediaQueryListEvent.idl \
    $(WebCore)/css/StyleMedia.idl \
    $(WebCore)/css/StyleSheet.idl \
    $(WebCore)/css/StyleSheetList.idl \
    $(WebCore)/css/typedom/StylePropertyMap.idl \
    $(WebCore)/css/typedom/StylePropertyMapReadOnly.idl \
	$(WebCore)/css/typedom/CSSKeywordValue.idl \
    $(WebCore)/css/typedom/CSSStyleImageValue.idl \
    $(WebCore)/css/typedom/CSSNumericValue.idl \
    $(WebCore)/css/typedom/CSSStyleValue.idl \
    $(WebCore)/css/typedom/CSSUnitValue.idl \
    $(WebCore)/css/typedom/CSSUnparsedValue.idl \
    $(WebCore)/css/typedom/CSSOMVariableReferenceValue.idl \
    $(WebCore)/css/typedom/color/CSSColor.idl \
    $(WebCore)/css/typedom/color/CSSColorValue.idl \
    $(WebCore)/css/typedom/color/CSSHSL.idl \
    $(WebCore)/css/typedom/color/CSSHWB.idl \
    $(WebCore)/css/typedom/color/CSSLCH.idl \
    $(WebCore)/css/typedom/color/CSSLab.idl \
    $(WebCore)/css/typedom/color/CSSOKLCH.idl \
    $(WebCore)/css/typedom/color/CSSOKLab.idl \
    $(WebCore)/css/typedom/color/CSSRGB.idl \
    $(WebCore)/css/typedom/numeric/CSSMathClamp.idl \
    $(WebCore)/css/typedom/numeric/CSSMathInvert.idl \
    $(WebCore)/css/typedom/numeric/CSSMathMax.idl \
    $(WebCore)/css/typedom/numeric/CSSMathMin.idl \
	$(WebCore)/css/typedom/numeric/CSSMathNegate.idl \
    $(WebCore)/css/typedom/numeric/CSSMathOperator.idl \
    $(WebCore)/css/typedom/numeric/CSSMathProduct.idl \
    $(WebCore)/css/typedom/numeric/CSSMathSum.idl \
    $(WebCore)/css/typedom/numeric/CSSMathValue.idl \
    $(WebCore)/css/typedom/numeric/CSSNumericArray.idl \
    $(WebCore)/css/typedom/numeric/CSSNumericBaseType.idl \
    $(WebCore)/css/typedom/numeric/CSSNumericType.idl \
	$(WebCore)/css/typedom/transform/CSSMatrixComponent.idl \
	$(WebCore)/css/typedom/transform/CSSMatrixComponentOptions.idl \
	$(WebCore)/css/typedom/transform/CSSPerspective.idl \
	$(WebCore)/css/typedom/transform/CSSRotate.idl \
	$(WebCore)/css/typedom/transform/CSSScale.idl \
	$(WebCore)/css/typedom/transform/CSSSkew.idl \
	$(WebCore)/css/typedom/transform/CSSSkewX.idl \
	$(WebCore)/css/typedom/transform/CSSSkewY.idl \
	$(WebCore)/css/typedom/transform/CSSTransformComponent.idl \
	$(WebCore)/css/typedom/transform/CSSTransformValue.idl \
	$(WebCore)/css/typedom/transform/CSSTranslate.idl \
    $(WebCore)/dom/AbortAlgorithm.idl \
    $(WebCore)/dom/AbortController.idl \
    $(WebCore)/dom/AbortSignal.idl \
    $(WebCore)/dom/AbstractRange.idl \
    $(WebCore)/dom/AddEventListenerOptions.idl \
    $(WebCore)/dom/Attr.idl \
    $(WebCore)/dom/BeforeUnloadEvent.idl \
    $(WebCore)/dom/BroadcastChannel.idl \
    $(WebCore)/dom/CDATASection.idl \
    $(WebCore)/dom/CharacterData.idl \
    $(WebCore)/dom/ChildNode.idl \
    $(WebCore)/dom/ClipboardEvent.idl \
    $(WebCore)/dom/Comment.idl \
    $(WebCore)/dom/CompositionEvent.idl \
    $(WebCore)/dom/CustomElementRegistry.idl \
    $(WebCore)/dom/CustomEvent.idl \
    $(WebCore)/dom/DOMException.idl \
    $(WebCore)/dom/DOMImplementation.idl \
    $(WebCore)/dom/DOMPoint.idl \
    $(WebCore)/dom/DOMPointInit.idl \
    $(WebCore)/dom/DOMPointReadOnly.idl \
    $(WebCore)/dom/DOMQuad.idl \
    $(WebCore)/dom/DOMQuadInit.idl \
    $(WebCore)/dom/DOMRect.idl \
    $(WebCore)/dom/DOMRectInit.idl \
    $(WebCore)/dom/DOMRectList.idl \
    $(WebCore)/dom/DOMRectReadOnly.idl \
    $(WebCore)/dom/DOMStringList.idl \
    $(WebCore)/dom/DOMStringMap.idl \
    $(WebCore)/dom/DataTransfer.idl \
    $(WebCore)/dom/DataTransferItem.idl \
    $(WebCore)/dom/DataTransferItemList.idl \
    $(WebCore)/dom/DeviceMotionEvent.idl \
    $(WebCore)/dom/DeviceOrientationEvent.idl \
    $(WebCore)/dom/DeviceOrientationOrMotionPermissionState.idl \
    $(WebCore)/dom/Document.idl \
    $(WebCore)/dom/Document+CSSOMView.idl \
    $(WebCore)/dom/Document+Fullscreen.idl \
    $(WebCore)/dom/Document+HTML.idl \
    $(WebCore)/dom/Document+HTMLObsolete.idl \
    $(WebCore)/dom/Document+PageVisibility.idl \
    $(WebCore)/dom/Document+PointerLock.idl \
    $(WebCore)/dom/Document+Selection.idl \
    $(WebCore)/dom/Document+StorageAccess.idl \
    $(WebCore)/dom/Document+UndoMananger.idl \
    $(WebCore)/dom/DocumentAndElementEventHandlers.idl \
    $(WebCore)/dom/DocumentFragment.idl \
    $(WebCore)/dom/DocumentOrShadowRoot.idl \
    $(WebCore)/dom/DocumentOrShadowRoot+Fullscreen.idl \
    $(WebCore)/dom/DocumentOrShadowRoot+PointerLock.idl \
    $(WebCore)/dom/DocumentType.idl \
    $(WebCore)/dom/DragEvent.idl \
    $(WebCore)/dom/Element+CSSOMView.idl \
    $(WebCore)/dom/Element+ComputedStyleMap.idl \
    $(WebCore)/dom/Element+DOMParsing.idl \
    $(WebCore)/dom/Element+Fullscreen.idl \
    $(WebCore)/dom/Element+PointerEvents.idl \
    $(WebCore)/dom/Element+PointerLock.idl \
    $(WebCore)/dom/Element.idl \
    $(WebCore)/dom/ElementContentEditable.idl \
    $(WebCore)/dom/ElementInternals.idl \
    $(WebCore)/dom/ErrorEvent.idl \
    $(WebCore)/dom/Event.idl \
    $(WebCore)/dom/EventInit.idl \
    $(WebCore)/dom/EventListener.idl \
    $(WebCore)/dom/EventListenerOptions.idl \
    $(WebCore)/dom/EventModifierInit.idl \
    $(WebCore)/dom/EventTarget.idl \
    $(WebCore)/dom/FocusEvent.idl \
    $(WebCore)/dom/FocusOptions.idl \
    $(WebCore)/dom/FormDataEvent.idl \
    $(WebCore)/dom/FullscreenOptions.idl \
    $(WebCore)/dom/GlobalEventHandlers+PointerEvents.idl \
    $(WebCore)/dom/GlobalEventHandlers+Selection.idl \
    $(WebCore)/dom/GlobalEventHandlers.idl \
    $(WebCore)/dom/HashChangeEvent.idl \
    $(WebCore)/dom/IdleDeadline.idl \
    $(WebCore)/dom/IdleRequestCallback.idl \
    $(WebCore)/dom/IdleRequestOptions.idl \
    $(WebCore)/dom/InnerHTML.idl \
    $(WebCore)/dom/InputEvent.idl \
    $(WebCore)/dom/KeyboardEvent.idl \
    $(WebCore)/dom/MessageChannel.idl \
    $(WebCore)/dom/MessageEvent.idl \
    $(WebCore)/dom/MessagePort.idl \
    $(WebCore)/dom/MouseEvent.idl \
    $(WebCore)/dom/MouseEventInit.idl \
    $(WebCore)/dom/MutationCallback.idl \
    $(WebCore)/dom/MutationEvent.idl \
    $(WebCore)/dom/MutationObserver.idl \
    $(WebCore)/dom/MutationRecord.idl \
    $(WebCore)/dom/NamedNodeMap.idl \
    $(WebCore)/dom/NavigatorMaxTouchPoints.idl \
    $(WebCore)/dom/Node.idl \
    $(WebCore)/dom/NodeFilter.idl \
    $(WebCore)/dom/NodeIterator.idl \
    $(WebCore)/dom/NodeList.idl \
    $(WebCore)/dom/NonDocumentTypeChildNode.idl \
    $(WebCore)/dom/NonElementParentNode.idl \
    $(WebCore)/dom/OverflowEvent.idl \
    $(WebCore)/dom/PageTransitionEvent.idl \
    $(WebCore)/dom/ParentNode.idl \
    $(WebCore)/dom/PointerEvent.idl \
    $(WebCore)/dom/PopStateEvent.idl \
    $(WebCore)/dom/ProcessingInstruction.idl \
    $(WebCore)/dom/ProgressEvent.idl \
    $(WebCore)/dom/PromiseRejectionEvent.idl \
    $(WebCore)/dom/Range+CSSOMView.idl \
    $(WebCore)/dom/Range+DOMParsing.idl \
    $(WebCore)/dom/Range.idl \
    $(WebCore)/dom/RequestAnimationFrameCallback.idl \
    $(WebCore)/dom/SecurityPolicyViolationEvent.idl \
    $(WebCore)/dom/SecurityPolicyViolationEventDisposition.idl \
    $(WebCore)/dom/ShadowRoot.idl \
    $(WebCore)/dom/ShadowRootInit.idl \
    $(WebCore)/dom/ShadowRootMode.idl \
    $(WebCore)/dom/SlotAssignmentMode.idl \
    $(WebCore)/dom/Slotable.idl \
    $(WebCore)/dom/StaticRange.idl \
    $(WebCore)/dom/StringCallback.idl \
    $(WebCore)/dom/Text.idl \
    $(WebCore)/dom/TextDecoder.idl \
    $(WebCore)/dom/TextDecoderStream.idl \
    $(WebCore)/dom/TextDecoderStreamDecoder.idl \
    $(WebCore)/dom/TextEncoder.idl \
    $(WebCore)/dom/TextEncoderStream.idl \
    $(WebCore)/dom/TextEncoderStreamEncoder.idl \
    $(WebCore)/dom/TextEvent.idl \
    $(WebCore)/dom/TreeWalker.idl \
    $(WebCore)/dom/UIEvent.idl \
    $(WebCore)/dom/UIEventInit.idl \
    $(WebCore)/dom/ValidityStateFlags.idl \
    $(WebCore)/dom/VisibilityState.idl \
    $(WebCore)/dom/WheelEvent.idl \
    $(WebCore)/dom/XMLDocument.idl \
    $(WebCore)/fileapi/Blob.idl \
    $(WebCore)/fileapi/BlobCallback.idl \
    $(WebCore)/fileapi/BlobPropertyBag.idl \
    $(WebCore)/fileapi/EndingType.idl \
    $(WebCore)/fileapi/File.idl \
    $(WebCore)/fileapi/FileList.idl \
    $(WebCore)/fileapi/FileReader.idl \
    $(WebCore)/fileapi/FileReaderSync.idl \
    $(WebCore)/html/DOMFormData.idl \
    $(WebCore)/html/DOMTokenList.idl \
    $(WebCore)/html/DOMURL.idl \
    $(WebCore)/html/HTMLAllCollection.idl \
    $(WebCore)/html/HTMLAnchorElement.idl \
    $(WebCore)/html/HTMLAreaElement.idl \
    $(WebCore)/html/HTMLAttachmentElement.idl \
    $(WebCore)/html/HTMLAudioElement.idl \
    $(WebCore)/html/HTMLBRElement.idl \
    $(WebCore)/html/HTMLBaseElement.idl \
    $(WebCore)/html/HTMLBodyElement+Compat.idl \
    $(WebCore)/html/HTMLBodyElement.idl \
    $(WebCore)/html/HTMLButtonElement.idl \
    $(WebCore)/html/HTMLCanvasElement.idl \
    $(WebCore)/html/HTMLCollection.idl \
    $(WebCore)/html/HTMLDListElement.idl \
    $(WebCore)/html/HTMLDataElement.idl \
    $(WebCore)/html/HTMLDataListElement.idl \
    $(WebCore)/html/HTMLDetailsElement.idl \
    $(WebCore)/html/HTMLDialogElement.idl \
    $(WebCore)/html/HTMLDirectoryElement.idl \
    $(WebCore)/html/HTMLDivElement.idl \
    $(WebCore)/html/HTMLDocument.idl \
    $(WebCore)/html/HTMLElement+CSSOMView.idl \
    $(WebCore)/html/HTMLElement.idl \
    $(WebCore)/html/HTMLEmbedElement.idl \
    $(WebCore)/html/HTMLFieldSetElement.idl \
    $(WebCore)/html/HTMLFontElement.idl \
    $(WebCore)/html/HTMLFormControlsCollection.idl \
    $(WebCore)/html/HTMLFormElement.idl \
    $(WebCore)/html/HTMLFrameElement.idl \
    $(WebCore)/html/HTMLFrameSetElement.idl \
    $(WebCore)/html/HTMLHRElement.idl \
    $(WebCore)/html/HTMLHeadElement.idl \
    $(WebCore)/html/HTMLHeadingElement.idl \
    $(WebCore)/html/HTMLHtmlElement.idl \
    $(WebCore)/html/HTMLHyperlinkElementUtils.idl \
    $(WebCore)/html/HTMLIFrameElement.idl \
    $(WebCore)/html/HTMLImageElement+CSSOMView.idl \
    $(WebCore)/html/HTMLImageElement.idl \
    $(WebCore)/html/HTMLInputElement.idl \
    $(WebCore)/html/HTMLLIElement.idl \
    $(WebCore)/html/HTMLLabelElement.idl \
    $(WebCore)/html/HTMLLegendElement.idl \
    $(WebCore)/html/HTMLLinkElement.idl \
    $(WebCore)/html/HTMLMapElement.idl \
    $(WebCore)/html/HTMLMarqueeElement.idl \
    $(WebCore)/html/HTMLMediaElement+AudioOutput.idl \
    $(WebCore)/html/HTMLMediaElement.idl \
    $(WebCore)/html/HTMLMenuElement.idl \
    $(WebCore)/html/HTMLMenuItemElement.idl \
    $(WebCore)/html/HTMLMetaElement.idl \
    $(WebCore)/html/HTMLMeterElement.idl \
    $(WebCore)/html/HTMLModElement.idl \
    $(WebCore)/html/HTMLOListElement.idl \
    $(WebCore)/html/HTMLObjectElement.idl \
    $(WebCore)/html/HTMLOptGroupElement.idl \
    $(WebCore)/html/HTMLOptionElement.idl \
    $(WebCore)/html/HTMLOptionsCollection.idl \
    $(WebCore)/html/HTMLOrForeignElement.idl \
    $(WebCore)/html/HTMLOutputElement.idl \
    $(WebCore)/html/HTMLParagraphElement.idl \
    $(WebCore)/html/HTMLParamElement.idl \
    $(WebCore)/html/HTMLPictureElement.idl \
    $(WebCore)/html/HTMLPreElement.idl \
    $(WebCore)/html/HTMLProgressElement.idl \
    $(WebCore)/html/HTMLQuoteElement.idl \
    $(WebCore)/html/HTMLScriptElement.idl \
    $(WebCore)/html/HTMLSelectElement.idl \
    $(WebCore)/html/HTMLSlotElement.idl \
    $(WebCore)/html/HTMLSourceElement.idl \
    $(WebCore)/html/HTMLSpanElement.idl \
    $(WebCore)/html/HTMLStyleElement.idl \
    $(WebCore)/html/HTMLTableCaptionElement.idl \
    $(WebCore)/html/HTMLTableCellElement.idl \
    $(WebCore)/html/HTMLTableColElement.idl \
    $(WebCore)/html/HTMLTableElement.idl \
    $(WebCore)/html/HTMLTableRowElement.idl \
    $(WebCore)/html/HTMLTableSectionElement.idl \
    $(WebCore)/html/HTMLTemplateElement.idl \
    $(WebCore)/html/HTMLTextAreaElement.idl \
    $(WebCore)/html/HTMLTimeElement.idl \
    $(WebCore)/html/HTMLTitleElement.idl \
    $(WebCore)/html/HTMLTrackElement.idl \
    $(WebCore)/html/HTMLUListElement.idl \
    $(WebCore)/html/HTMLUnknownElement.idl \
    $(WebCore)/html/HTMLVideoElement.idl \
    $(WebCore)/html/HTMLVideoElement+RequestVideoFrameCallback.idl \
    $(WebCore)/html/ImageBitmap.idl \
    $(WebCore)/html/ImageBitmapOptions.idl \
    $(WebCore)/html/ImageData.idl \
    $(WebCore)/html/ImageDataSettings.idl \
    $(WebCore)/html/MediaController.idl \
    $(WebCore)/html/MediaEncryptedEvent.idl \
    $(WebCore)/html/MediaError.idl \
    $(WebCore)/html/OffscreenCanvas.idl \
    $(WebCore)/html/RadioNodeList.idl \
    $(WebCore)/html/SubmitEvent.idl \
    $(WebCore)/html/TextMetrics.idl \
    $(WebCore)/html/TimeRanges.idl \
    $(WebCore)/html/URLSearchParams.idl \
    $(WebCore)/html/UserActivation.idl \
    $(WebCore)/html/ValidityState.idl \
    $(WebCore)/html/VideoFrameMetadata.idl \
    $(WebCore)/html/VideoFrameRequestCallback.idl \
    $(WebCore)/html/VoidCallback.idl \
    $(WebCore)/html/WebKitMediaKeyError.idl \
    $(WebCore)/html/canvas/ANGLEInstancedArrays.idl \
    $(WebCore)/html/canvas/CanvasCompositing.idl \
    $(WebCore)/html/canvas/CanvasDirection.idl \
    $(WebCore)/html/canvas/CanvasDrawImage.idl \
    $(WebCore)/html/canvas/CanvasDrawPath.idl \
    $(WebCore)/html/canvas/CanvasFillRule.idl \
    $(WebCore)/html/canvas/CanvasFillStrokeStyles.idl \
    $(WebCore)/html/canvas/CanvasFilters.idl \
    $(WebCore)/html/canvas/CanvasGradient.idl \
    $(WebCore)/html/canvas/CanvasImageData.idl \
    $(WebCore)/html/canvas/CanvasImageSmoothing.idl \
    $(WebCore)/html/canvas/CanvasLineCap.idl \
    $(WebCore)/html/canvas/CanvasLineJoin.idl \
    $(WebCore)/html/canvas/CanvasPath.idl \
    $(WebCore)/html/canvas/CanvasPathDrawingStyles.idl \
    $(WebCore)/html/canvas/CanvasPattern.idl \
    $(WebCore)/html/canvas/CanvasRect.idl \
    $(WebCore)/html/canvas/CanvasRenderingContext2D.idl \
    $(WebCore)/html/canvas/CanvasRenderingContext2DSettings.idl \
    $(WebCore)/html/canvas/CanvasShadowStyles.idl \
    $(WebCore)/html/canvas/CanvasState.idl \
    $(WebCore)/html/canvas/CanvasText.idl \
    $(WebCore)/html/canvas/CanvasTextAlign.idl \
    $(WebCore)/html/canvas/CanvasTextBaseline.idl \
    $(WebCore)/html/canvas/CanvasTextDrawingStyles.idl \
    $(WebCore)/html/canvas/CanvasTransform.idl \
    $(WebCore)/html/canvas/CanvasUserInterface.idl \
    $(WebCore)/html/canvas/EXTBlendMinMax.idl \
    $(WebCore)/html/canvas/EXTColorBufferFloat.idl \
    $(WebCore)/html/canvas/EXTColorBufferHalfFloat.idl \
    $(WebCore)/html/canvas/EXTFloatBlend.idl \
    $(WebCore)/html/canvas/EXTFragDepth.idl \
    $(WebCore)/html/canvas/EXTShaderTextureLOD.idl \
    $(WebCore)/html/canvas/EXTTextureCompressionBPTC.idl \
    $(WebCore)/html/canvas/EXTTextureCompressionRGTC.idl \
    $(WebCore)/html/canvas/EXTTextureFilterAnisotropic.idl \
    $(WebCore)/html/canvas/EXTTextureNorm16.idl \
    $(WebCore)/html/canvas/EXTsRGB.idl \
    $(WebCore)/html/canvas/GPUCanvasContext.idl \
    $(WebCore)/html/canvas/ImageBitmapRenderingContext.idl \
    $(WebCore)/html/canvas/ImageBitmapRenderingContextSettings.idl \
    $(WebCore)/html/canvas/ImageSmoothingQuality.idl \
    $(WebCore)/html/canvas/KHRParallelShaderCompile.idl \
    $(WebCore)/html/canvas/OESDrawBuffersIndexed.idl \
    $(WebCore)/html/canvas/OESElementIndexUint.idl \
    $(WebCore)/html/canvas/OESFBORenderMipmap.idl \
    $(WebCore)/html/canvas/OESStandardDerivatives.idl \
    $(WebCore)/html/canvas/OESTextureFloat.idl \
    $(WebCore)/html/canvas/OESTextureFloatLinear.idl \
    $(WebCore)/html/canvas/OESTextureHalfFloat.idl \
    $(WebCore)/html/canvas/OESTextureHalfFloatLinear.idl \
    $(WebCore)/html/canvas/OESVertexArrayObject.idl \
    $(WebCore)/html/canvas/OffscreenCanvasRenderingContext2D.idl \
    $(WebCore)/html/canvas/PaintRenderingContext2D.idl \
    $(WebCore)/html/canvas/Path2D.idl \
    $(WebCore)/html/canvas/PredefinedColorSpace.idl \
    $(WebCore)/html/canvas/WebGL2RenderingContext.idl \
    $(WebCore)/html/canvas/WebGLActiveInfo.idl \
    $(WebCore)/html/canvas/WebGLBuffer.idl \
    $(WebCore)/html/canvas/WebGLColorBufferFloat.idl \
    $(WebCore)/html/canvas/WebGLCompressedTextureASTC.idl \
    $(WebCore)/html/canvas/WebGLCompressedTextureETC.idl \
    $(WebCore)/html/canvas/WebGLCompressedTextureETC1.idl \
    $(WebCore)/html/canvas/WebGLCompressedTexturePVRTC.idl \
    $(WebCore)/html/canvas/WebGLCompressedTextureS3TC.idl \
    $(WebCore)/html/canvas/WebGLCompressedTextureS3TCsRGB.idl \
    $(WebCore)/html/canvas/WebGLContextAttributes.idl \
    $(WebCore)/html/canvas/WebGLContextEvent.idl \
    $(WebCore)/html/canvas/WebGLDebugRendererInfo.idl \
    $(WebCore)/html/canvas/WebGLDebugShaders.idl \
    $(WebCore)/html/canvas/WebGLDepthTexture.idl \
    $(WebCore)/html/canvas/WebGLDrawBuffers.idl \
    $(WebCore)/html/canvas/WebGLDrawInstancedBaseVertexBaseInstance.idl \
    $(WebCore)/html/canvas/WebGLFramebuffer.idl \
    $(WebCore)/html/canvas/WebGLLoseContext.idl \
    $(WebCore)/html/canvas/WebGLMultiDraw.idl \
    $(WebCore)/html/canvas/WebGLMultiDrawInstancedBaseVertexBaseInstance.idl \
    $(WebCore)/html/canvas/WebGLProgram.idl \
    $(WebCore)/html/canvas/WebGLProvokingVertex.idl \
    $(WebCore)/html/canvas/WebGLQuery.idl \
    $(WebCore)/html/canvas/WebGLRenderbuffer.idl \
    $(WebCore)/html/canvas/WebGLRenderingContext.idl \
    $(WebCore)/html/canvas/WebGLRenderingContextBase.idl \
    $(WebCore)/html/canvas/WebGLSampler.idl \
    $(WebCore)/html/canvas/WebGLShader.idl \
    $(WebCore)/html/canvas/WebGLShaderPrecisionFormat.idl \
    $(WebCore)/html/canvas/WebGLSync.idl \
    $(WebCore)/html/canvas/WebGLTexture.idl \
    $(WebCore)/html/canvas/WebGLTransformFeedback.idl \
    $(WebCore)/html/canvas/WebGLUniformLocation.idl \
    $(WebCore)/html/canvas/WebGLVertexArrayObject.idl \
    $(WebCore)/html/canvas/WebGLVertexArrayObjectOES.idl \
    $(WebCore)/html/track/AudioTrack.idl \
    $(WebCore)/html/track/AudioTrackConfiguration.idl \
    $(WebCore)/html/track/AudioTrackList.idl \
    $(WebCore)/html/track/DataCue.idl \
    $(WebCore)/html/track/TextTrack.idl \
    $(WebCore)/html/track/TextTrackCue.idl \
    $(WebCore)/html/track/TextTrackCueGeneric.idl \
    $(WebCore)/html/track/TextTrackCueList.idl \
    $(WebCore)/html/track/TextTrackList.idl \
    $(WebCore)/html/track/TrackEvent.idl \
    $(WebCore)/html/track/VTTCue.idl \
    $(WebCore)/html/track/VTTRegion.idl \
    $(WebCore)/html/track/VTTRegionList.idl \
    $(WebCore)/html/track/VideoTrack.idl \
    $(WebCore)/html/track/VideoTrackConfiguration.idl \
    $(WebCore)/html/track/VideoTrackList.idl \
    $(WebCore)/mathml/MathMLElement.idl \
    $(WebCore)/mathml/MathMLMathElement.idl \
    $(WebCore)/inspector/CommandLineAPIHost.idl \
    $(WebCore)/inspector/InspectorAuditAccessibilityObject.idl \
    $(WebCore)/inspector/InspectorAuditDOMObject.idl \
    $(WebCore)/inspector/InspectorAuditResourcesObject.idl \
    $(WebCore)/inspector/InspectorFrontendHost.idl \
    $(WebCore)/loader/COEPInheritenceViolationReportBody.idl \
    $(WebCore)/loader/CORPViolationReportBody.idl \
    $(WebCore)/loader/appcache/DOMApplicationCache.idl \
    $(WebCore)/page/BarProp.idl \
    $(WebCore)/page/Crypto.idl \
    $(WebCore)/page/DOMSelection.idl \
    $(WebCore)/page/DOMWindow+CSSOM.idl \
    $(WebCore)/page/DOMWindow+CSSOMView.idl \
    $(WebCore)/page/DOMWindow+Compat.idl \
    $(WebCore)/page/DOMWindow+DeviceMotion.idl \
    $(WebCore)/page/DOMWindow+DeviceOrientation.idl \
    $(WebCore)/page/DOMWindow+RequestIdleCallback.idl \
    $(WebCore)/page/DOMWindow+Selection.idl \
    $(WebCore)/page/DOMWindow+VisualViewport.idl \
    $(WebCore)/page/DOMWindow.idl \
    $(WebCore)/page/EventSource.idl \
    $(WebCore)/page/History.idl \
    $(WebCore)/page/IntersectionObserver.idl \
    $(WebCore)/page/IntersectionObserverCallback.idl \
    $(WebCore)/page/IntersectionObserverEntry.idl \
    $(WebCore)/page/Location.idl \
    $(WebCore)/page/Navigator.idl \
    $(WebCore)/page/Navigator+IsLoggedIn.idl \
    $(WebCore)/page/Navigator+UserActivation.idl \
    $(WebCore)/page/NavigatorCookies.idl \
    $(WebCore)/page/NavigatorID.idl \
    $(WebCore)/page/NavigatorLanguage.idl \
    $(WebCore)/page/NavigatorOnLine.idl \
    $(WebCore)/page/NavigatorPlugins.idl \
    $(WebCore)/page/NavigatorServiceWorker.idl \
    $(WebCore)/page/NavigatorShare.idl \
    $(WebCore)/page/NavigatorStorage.idl \
    $(WebCore)/page/Performance+NavigationTiming.idl \
    $(WebCore)/page/Performance+PerformanceTimeline.idl \
    $(WebCore)/page/Performance+ResourceTiming.idl \
    $(WebCore)/page/Performance+UserTiming.idl \
    $(WebCore)/page/Performance.idl \
    $(WebCore)/page/PerformanceEntry.idl \
    $(WebCore)/page/PerformanceMark.idl \
    $(WebCore)/page/PerformanceMarkOptions.idl \
    $(WebCore)/page/PerformanceMeasure.idl \
    $(WebCore)/page/PerformanceMeasureOptions.idl \
    $(WebCore)/page/PerformanceNavigation.idl \
    $(WebCore)/page/PerformanceNavigationTiming.idl \
    $(WebCore)/page/PerformanceObserver.idl \
    $(WebCore)/page/PerformanceObserverCallback.idl \
    $(WebCore)/page/PerformanceObserverEntryList.idl \
    $(WebCore)/page/PerformancePaintTiming.idl \
    $(WebCore)/page/PerformanceResourceTiming.idl \
    $(WebCore)/page/PerformanceServerTiming.idl \
    $(WebCore)/page/PerformanceTiming.idl \
    $(WebCore)/page/RemoteDOMWindow.idl \
    $(WebCore)/page/ResizeObserver.idl \
    $(WebCore)/page/ResizeObserverBoxOptions.idl \
    $(WebCore)/page/ResizeObserverCallback.idl \
    $(WebCore)/page/ResizeObserverEntry.idl \
    $(WebCore)/page/ResizeObserverOptions.idl \
    $(WebCore)/page/ResizeObserverSize.idl \
    $(WebCore)/page/Screen.idl \
    $(WebCore)/page/ScreenOrientation.idl \
    $(WebCore)/page/ScrollBehavior.idl \
    $(WebCore)/page/ScrollIntoViewOptions.idl \
    $(WebCore)/page/ScrollLogicalPosition.idl \
    $(WebCore)/page/ScrollOptions.idl \
    $(WebCore)/page/ScrollToOptions.idl \
    $(WebCore)/page/ShadowRealmGlobalScope.idl \
    $(WebCore)/page/ShareData.idl \
    $(WebCore)/page/StructuredSerializeOptions.idl \
    $(WebCore)/page/UndoItem.idl \
    $(WebCore)/page/UndoManager.idl \
    $(WebCore)/page/UserMessageHandler.idl \
    $(WebCore)/page/UserMessageHandlersNamespace.idl \
    $(WebCore)/page/VisualViewport.idl \
    $(WebCore)/page/WebKitNamespace.idl \
    $(WebCore)/page/WebKitPoint.idl \
    $(WebCore)/page/WindowEventHandlers.idl \
    $(WebCore)/page/WindowLocalStorage.idl \
    $(WebCore)/page/WindowOrWorkerGlobalScope+Crypto.idl \
    $(WebCore)/page/WindowOrWorkerGlobalScope+Performance.idl \
    $(WebCore)/page/WindowOrWorkerGlobalScope.idl \
    $(WebCore)/page/WindowSessionStorage.idl \
    $(WebCore)/page/WorkerNavigator.idl \
    $(WebCore)/page/csp/CSPViolationReportBody.idl \
    $(WebCore)/plugins/DOMMimeType.idl \
    $(WebCore)/plugins/DOMMimeTypeArray.idl \
    $(WebCore)/plugins/DOMPlugin.idl \
    $(WebCore)/plugins/DOMPluginArray.idl \
    $(WebCore)/storage/Storage.idl \
    $(WebCore)/storage/StorageEvent.idl \
    $(WebCore)/svg/Document+SVG.idl \
    $(WebCore)/svg/SVGAElement.idl \
    $(WebCore)/svg/SVGAngle.idl \
    $(WebCore)/svg/SVGAnimateColorElement.idl \
    $(WebCore)/svg/SVGAnimateElement.idl \
    $(WebCore)/svg/SVGAnimateMotionElement.idl \
    $(WebCore)/svg/SVGAnimateTransformElement.idl \
    $(WebCore)/svg/SVGAnimatedAngle.idl \
    $(WebCore)/svg/SVGAnimatedBoolean.idl \
    $(WebCore)/svg/SVGAnimatedEnumeration.idl \
    $(WebCore)/svg/SVGAnimatedInteger.idl \
    $(WebCore)/svg/SVGAnimatedLength.idl \
    $(WebCore)/svg/SVGAnimatedLengthList.idl \
    $(WebCore)/svg/SVGAnimatedNumber.idl \
    $(WebCore)/svg/SVGAnimatedNumberList.idl \
    $(WebCore)/svg/SVGAnimatedPreserveAspectRatio.idl \
    $(WebCore)/svg/SVGAnimatedRect.idl \
    $(WebCore)/svg/SVGAnimatedString.idl \
    $(WebCore)/svg/SVGAnimatedTransformList.idl \
    $(WebCore)/svg/SVGAnimationElement.idl \
    $(WebCore)/svg/SVGCircleElement.idl \
    $(WebCore)/svg/SVGClipPathElement.idl \
    $(WebCore)/svg/SVGComponentTransferFunctionElement.idl \
    $(WebCore)/svg/SVGDefsElement.idl \
    $(WebCore)/svg/SVGDescElement.idl \
    $(WebCore)/svg/SVGElement.idl \
    $(WebCore)/svg/SVGEllipseElement.idl \
    $(WebCore)/svg/SVGFEBlendElement.idl \
    $(WebCore)/svg/SVGFEColorMatrixElement.idl \
    $(WebCore)/svg/SVGFEComponentTransferElement.idl \
    $(WebCore)/svg/SVGFECompositeElement.idl \
    $(WebCore)/svg/SVGFEConvolveMatrixElement.idl \
    $(WebCore)/svg/SVGFEDiffuseLightingElement.idl \
    $(WebCore)/svg/SVGFEDisplacementMapElement.idl \
    $(WebCore)/svg/SVGFEDistantLightElement.idl \
    $(WebCore)/svg/SVGFEDropShadowElement.idl \
    $(WebCore)/svg/SVGFEFloodElement.idl \
    $(WebCore)/svg/SVGFEFuncAElement.idl \
    $(WebCore)/svg/SVGFEFuncBElement.idl \
    $(WebCore)/svg/SVGFEFuncGElement.idl \
    $(WebCore)/svg/SVGFEFuncRElement.idl \
    $(WebCore)/svg/SVGFEGaussianBlurElement.idl \
    $(WebCore)/svg/SVGFEImageElement.idl \
    $(WebCore)/svg/SVGFEMergeElement.idl \
    $(WebCore)/svg/SVGFEMergeNodeElement.idl \
    $(WebCore)/svg/SVGFEMorphologyElement.idl \
    $(WebCore)/svg/SVGFEOffsetElement.idl \
    $(WebCore)/svg/SVGFEPointLightElement.idl \
    $(WebCore)/svg/SVGFESpecularLightingElement.idl \
    $(WebCore)/svg/SVGFESpotLightElement.idl \
    $(WebCore)/svg/SVGFETileElement.idl \
    $(WebCore)/svg/SVGFETurbulenceElement.idl \
    $(WebCore)/svg/SVGFilterElement.idl \
    $(WebCore)/svg/SVGFilterPrimitiveStandardAttributes.idl \
    $(WebCore)/svg/SVGFitToViewBox.idl \
    $(WebCore)/svg/SVGForeignObjectElement.idl \
    $(WebCore)/svg/SVGGElement.idl \
    $(WebCore)/svg/SVGGeometryElement.idl \
    $(WebCore)/svg/SVGGradientElement.idl \
    $(WebCore)/svg/SVGGraphicsElement.idl \
    $(WebCore)/svg/SVGImageElement.idl \
    $(WebCore)/svg/SVGLength.idl \
    $(WebCore)/svg/SVGLengthList.idl \
    $(WebCore)/svg/SVGLineElement.idl \
    $(WebCore)/svg/SVGLinearGradientElement.idl \
    $(WebCore)/svg/SVGMPathElement.idl \
    $(WebCore)/svg/SVGMarkerElement.idl \
    $(WebCore)/svg/SVGMaskElement.idl \
    $(WebCore)/svg/SVGMatrix.idl \
    $(WebCore)/svg/SVGMetadataElement.idl \
    $(WebCore)/svg/SVGNumber.idl \
    $(WebCore)/svg/SVGNumberList.idl \
    $(WebCore)/svg/SVGPathElement.idl \
    $(WebCore)/svg/SVGPathSeg.idl \
    $(WebCore)/svg/SVGPathSegArcAbs.idl \
    $(WebCore)/svg/SVGPathSegArcRel.idl \
    $(WebCore)/svg/SVGPathSegClosePath.idl \
    $(WebCore)/svg/SVGPathSegCurvetoCubicAbs.idl \
    $(WebCore)/svg/SVGPathSegCurvetoCubicRel.idl \
    $(WebCore)/svg/SVGPathSegCurvetoCubicSmoothAbs.idl \
    $(WebCore)/svg/SVGPathSegCurvetoCubicSmoothRel.idl \
    $(WebCore)/svg/SVGPathSegCurvetoQuadraticAbs.idl \
    $(WebCore)/svg/SVGPathSegCurvetoQuadraticRel.idl \
    $(WebCore)/svg/SVGPathSegCurvetoQuadraticSmoothAbs.idl \
    $(WebCore)/svg/SVGPathSegCurvetoQuadraticSmoothRel.idl \
    $(WebCore)/svg/SVGPathSegLinetoAbs.idl \
    $(WebCore)/svg/SVGPathSegLinetoHorizontalAbs.idl \
    $(WebCore)/svg/SVGPathSegLinetoHorizontalRel.idl \
    $(WebCore)/svg/SVGPathSegLinetoRel.idl \
    $(WebCore)/svg/SVGPathSegLinetoVerticalAbs.idl \
    $(WebCore)/svg/SVGPathSegLinetoVerticalRel.idl \
    $(WebCore)/svg/SVGPathSegList.idl \
    $(WebCore)/svg/SVGPathSegMovetoAbs.idl \
    $(WebCore)/svg/SVGPathSegMovetoRel.idl \
    $(WebCore)/svg/SVGPatternElement.idl \
    $(WebCore)/svg/SVGPoint.idl \
    $(WebCore)/svg/SVGPointList.idl \
    $(WebCore)/svg/SVGPolygonElement.idl \
    $(WebCore)/svg/SVGPolylineElement.idl \
    $(WebCore)/svg/SVGPreserveAspectRatio.idl \
    $(WebCore)/svg/SVGRadialGradientElement.idl \
    $(WebCore)/svg/SVGRect.idl \
    $(WebCore)/svg/SVGRectElement.idl \
    $(WebCore)/svg/SVGRenderingIntent.idl \
    $(WebCore)/svg/SVGSVGElement.idl \
    $(WebCore)/svg/SVGScriptElement.idl \
    $(WebCore)/svg/SVGSetElement.idl \
    $(WebCore)/svg/SVGStopElement.idl \
    $(WebCore)/svg/SVGStringList.idl \
    $(WebCore)/svg/SVGStyleElement.idl \
    $(WebCore)/svg/SVGSwitchElement.idl \
    $(WebCore)/svg/SVGSymbolElement.idl \
    $(WebCore)/svg/SVGTSpanElement.idl \
    $(WebCore)/svg/SVGTests.idl \
    $(WebCore)/svg/SVGTextContentElement.idl \
    $(WebCore)/svg/SVGTextElement.idl \
    $(WebCore)/svg/SVGTextPathElement.idl \
    $(WebCore)/svg/SVGTextPositioningElement.idl \
    $(WebCore)/svg/SVGTitleElement.idl \
    $(WebCore)/svg/SVGTransform.idl \
    $(WebCore)/svg/SVGTransformList.idl \
    $(WebCore)/svg/SVGURIReference.idl \
    $(WebCore)/svg/SVGUnitTypes.idl \
    $(WebCore)/svg/SVGUseElement.idl \
    $(WebCore)/svg/SVGViewElement.idl \
    $(WebCore)/svg/SVGViewSpec.idl \
    $(WebCore)/svg/SVGZoomAndPan.idl \
    $(WebCore)/testing/GCObservation.idl \
    $(WebCore)/testing/InternalSettings.idl \
    $(WebCore)/testing/Internals.idl \
    $(WebCore)/testing/InternalsMapLike.idl \
    $(WebCore)/testing/InternalsSetLike.idl \
    $(WebCore)/testing/MallocStatistics.idl \
    $(WebCore)/testing/MemoryInfo.idl \
    $(WebCore)/testing/MockCDMFactory.idl \
    $(WebCore)/testing/MockContentFilterSettings.idl \
    $(WebCore)/testing/MockPageOverlay.idl \
    $(WebCore)/testing/MockPaymentAddress.idl \
    $(WebCore)/testing/MockPaymentContactFields.idl \
    $(WebCore)/testing/MockPaymentCoordinator.idl \
    $(WebCore)/testing/MockPaymentError.idl \
    $(WebCore)/testing/MockWebAuthenticationConfiguration.idl \
    $(WebCore)/testing/ServiceWorkerInternals.idl \
    $(WebCore)/testing/TypeConversions.idl \
    $(WebCore)/testing/FakeXRBoundsPoint.idl \
    $(WebCore)/testing/FakeXRButtonStateInit.idl \
    $(WebCore)/testing/FakeXRInputSourceInit.idl \
    $(WebCore)/testing/FakeXRJointStateInit.idl \
    $(WebCore)/testing/FakeXRRigidTransformInit.idl \
    $(WebCore)/testing/FakeXRViewInit.idl \
    $(WebCore)/testing/WebFakeXRDevice.idl \
    $(WebCore)/testing/WebFakeXRInputController.idl \
    $(WebCore)/testing/WebXRTest.idl \
    $(WebCore)/testing/XRSimulateUserActivationFunction.idl \
    $(WebCore)/workers/AbstractWorker.idl \
    $(WebCore)/workers/DedicatedWorkerGlobalScope.idl \
    $(WebCore)/workers/Worker.idl \
    $(WebCore)/workers/WorkerGlobalScope.idl \
    $(WebCore)/workers/WorkerLocation.idl \
    $(WebCore)/workers/WorkerOptions.idl \
    $(WebCore)/workers/WorkerType.idl \
    $(WebCore)/workers/service/ExtendableEvent.idl \
    $(WebCore)/workers/service/ExtendableEventInit.idl \
    $(WebCore)/workers/service/ExtendableMessageEvent.idl \
    $(WebCore)/workers/service/FetchEvent.idl \
    $(WebCore)/workers/service/NavigationPreloadManager.idl \
    $(WebCore)/workers/service/NavigationPreloadState.idl \
    $(WebCore)/workers/service/ServiceWorker.idl \
    $(WebCore)/workers/service/ServiceWorkerClient.idl \
    $(WebCore)/workers/service/ServiceWorkerClientType.idl \
    $(WebCore)/workers/service/ServiceWorkerClients.idl \
    $(WebCore)/workers/service/ServiceWorkerContainer.idl \
    $(WebCore)/workers/service/ServiceWorkerGlobalScope.idl \
    $(WebCore)/workers/service/ServiceWorkerRegistration.idl \
    $(WebCore)/workers/service/ServiceWorkerUpdateViaCache.idl \
    $(WebCore)/workers/service/ServiceWorkerWindowClient.idl \
    $(WebCore)/workers/shared/SharedWorker.idl \
    $(WebCore)/workers/shared/SharedWorkerGlobalScope.idl \
    $(WebCore)/worklets/PaintWorkletGlobalScope.idl \
    $(WebCore)/worklets/Worklet.idl \
    $(WebCore)/worklets/WorkletGlobalScope.idl \
    $(WebCore)/worklets/WorkletOptions.idl \
    $(WebCore)/xml/CustomXPathNSResolver.idl \
    $(WebCore)/xml/DOMParser.idl \
    $(WebCore)/xml/ParseFromStringOptions.idl \
    $(WebCore)/xml/XMLHttpRequest.idl \
    $(WebCore)/xml/XMLHttpRequestEventTarget.idl \
    $(WebCore)/xml/XMLHttpRequestProgressEvent.idl \
    $(WebCore)/xml/XMLHttpRequestUpload.idl \
    $(WebCore)/xml/XMLSerializer.idl \
    $(WebCore)/xml/XPathEvaluator.idl \
    $(WebCore)/xml/XPathEvaluatorBase.idl \
    $(WebCore)/xml/XPathExpression.idl \
    $(WebCore)/xml/XPathNSResolver.idl \
    $(WebCore)/xml/XPathResult.idl \
    $(WebCore)/xml/XSLTProcessor.idl \
    InternalSettingsGenerated.idl \
    CSSStyleDeclaration+PropertyNames.idl \
#

# --------

ADDITIONAL_BINDING_IDLS = \
    DocumentTouch.idl \
    GestureEvent.idl \
    Internals+Additions.idl \
    InternalsAdditions.idl \
    Touch.idl \
    TouchEvent.idl \
    TouchList.idl \
#

vpath %.in $(WEBKITADDITIONS_HEADER_SEARCH_PATHS)

ADDITIONAL_EVENT_NAMES =
ADDITIONAL_EVENT_TARGET_FACTORY =

-include WebCoreDerivedSourcesAdditions.make

# Convert the "bare" IDL names in ADDITIONAL_BINDING_IDLS to full paths by
# looking for those files in the expected locations: in /usr/local/include in
# the build output directory, in /usr/local/include in the SDK, or in any of
# the paths indicated in JS_BINDING_IDLS.
#
# The first step is to get the paths of the IDL files in JS_BINDING_IDLS. We
# take just the directory parts, convert them to something that has symlinks and
# other non-canonical parts resolved, and then sort them (which removes
# duplicates).
#
# Next we prepend the "bare" IDL names with the locations in
# BUILD_PRODUCTS_DIR, SDKROOT, and the paths generated from JS_BINDING_IDLS
# until we find each file. The resulting full path is added to JS_BINDING_IDLS.

IDL_PATHS := $(sort $(foreach IDL_FILE, $(JS_BINDING_IDLS), $(realpath $(dir $(IDL_FILE)))))

ADDITIONS_PATHS = \
    $(BUILT_PRODUCTS_DIR)$(WK_LIBRARY_HEADERS_FOLDER_PATH)/WebKitAdditions \
    $(SDKROOT)$(WK_LIBRARY_HEADERS_FOLDER_PATH)/WebKitAdditions

ADDITIONAL_BINDING_IDLS_PATHS = \
    $(ADDITIONS_PATHS) \
    $(IDL_PATHS)

JS_BINDING_IDLS += \
    $(foreach \
        IDL_FILE, \
        $(ADDITIONAL_BINDING_IDLS), \
        $(firstword $(realpath $(foreach \
            IDL_PATH, \
            $(ADDITIONAL_BINDING_IDLS_PATHS), \
            $(IDL_PATH)/$(IDL_FILE)))))

.PHONY : all

get_bare_name = $(basename $(notdir $(1)))
JS_DOM_CLASSES=$(call get_bare_name,$(JS_BINDING_IDLS))

JS_DOM_HEADERS=$(filter-out JSEventListener.h, $(JS_DOM_CLASSES:%=JS%.h))
JS_DOM_IMPLEMENTATIONS=$(filter-out JSEventListener.cpp, $(JS_DOM_CLASSES:%=JS%.cpp))

WEB_DOM_HEADERS :=

all : \
    $(JS_DOM_HEADERS) \
    $(JS_DOM_IMPLEMENTATIONS) \
    $(WEB_DOM_HEADERS) \
    \
    CSSPropertyNames.cpp \
    CSSPropertyNames.h \
    CSSPropertyParsing.cpp \
    CSSPropertyParsing.h \
    CSSValueKeywords.cpp \
    CSSValueKeywords.h \
    ColorData.cpp \
    DOMJITAbstractHeapRepository.h \
    ElementName.cpp \
    ElementName.h \
    EventInterfaces.h \
    EventTargetInterfaces.h \
    HTMLElementFactory.cpp \
    HTMLElementFactory.h \
    HTMLElementTypeHelpers.h \
    HTMLEntityTable.cpp \
    HTMLNames.cpp \
    HTMLNames.h \
    JSHTMLElementWrapperFactory.cpp \
    JSHTMLElementWrapperFactory.h \
    JSMathMLElementWrapperFactory.cpp \
    JSMathMLElementWrapperFactory.h \
    JSSVGElementWrapperFactory.cpp \
    JSSVGElementWrapperFactory.h \
    LocalizableAdditions.strings.out \
    Namespace.cpp \
    Namespace.h \
    SVGElementFactory.cpp \
    SVGElementFactory.h \
    SVGElementTypeHelpers.h \
    SVGNames.cpp \
    SVGNames.h \
    SelectorPseudoClassAndCompatibilityElementMap.cpp \
    SelectorPseudoElementTypeMap.cpp \
    StyleBuilderGenerated.cpp \
    StylePropertyShorthandFunctions.cpp \
    StylePropertyShorthandFunctions.h \
    TagName.cpp \
    TagName.h \
    CSSStyleDeclaration+PropertyNames.idl \
    WebKitFontFamilyNames.cpp \
    WebKitFontFamilyNames.h \
    MathMLElementFactory.cpp \
    MathMLElementFactory.h \
    MathMLElementTypeHelpers.h \
    MathMLNames.cpp \
    MathMLNames.h \
#

# --------

# CSS property names and value keywords

WEBCORE_CSS_PROPERTY_NAMES := $(WebCore)/css/CSSProperties.json
WEBCORE_CSS_VALUE_KEYWORDS := $(WebCore)/css/CSSValueKeywords.in
WEBCORE_CSS_VALUE_KEYWORDS := $(WEBCORE_CSS_VALUE_KEYWORDS) $(WebCore)/css/SVGCSSValueKeywords.in

CSS_PROPERTY_NAME_FILES = \
    CSSPropertyNames.cpp \
    CSSPropertyNames.h \
    CSSPropertyParsing.cpp \
    CSSPropertyParsing.h \
    CSSStyleDeclaration+PropertyNames.idl \
    StyleBuilderGenerated.cpp \
    StylePropertyShorthandFunctions.cpp \
    StylePropertyShorthandFunctions.h \
#
CSS_PROPERTY_NAME_FILES_PATTERNS = $(subst .,%,$(CSS_PROPERTY_NAME_FILES))

all : $(CSS_PROPERTY_NAME_FILES)
$(CSS_PROPERTY_NAME_FILES_PATTERNS) : $(WEBCORE_CSS_PROPERTY_NAMES) $(WebCore)/css/process-css-properties.py $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) -pe '' $(WEBCORE_CSS_PROPERTY_NAMES) > CSSProperties.json
	$(PYTHON) "$(WebCore)/css/process-css-properties.py" --gperf-executable gperf --defines "$(FEATURE_AND_PLATFORM_DEFINES)"

CSS_VALUE_KEYWORD_FILES = \
    CSSValueKeywords.h \
    CSSValueKeywords.cpp \
#
CSS_VALUE_KEYWORD_FILES_PATTERNS = $(subst .,%,$(CSS_VALUE_KEYWORD_FILES))

all : $(CSS_VALUE_KEYWORD_FILES)
$(CSS_VALUE_KEYWORD_FILES_PATTERNS) : $(WEBCORE_CSS_VALUE_KEYWORDS) $(WebCore)/css/process-css-values.py $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) -pe '' $(WEBCORE_CSS_VALUE_KEYWORDS) > CSSValueKeywords.in
	$(PYTHON) "$(WebCore)/css/process-css-values.py" --gperf-executable gperf --defines "$(FEATURE_AND_PLATFORM_DEFINES)"

# --------

# CSS Selector pseudo type name to value map.

SelectorPseudoClassAndCompatibilityElementMap.cpp : $(WebCore)/css/makeSelectorPseudoClassAndCompatibilityElementMap.py $(WebCore)/css/SelectorPseudoClassAndCompatibilityElementMap.in $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PYTHON) "$(WebCore)/css/makeSelectorPseudoClassAndCompatibilityElementMap.py" $(WebCore)/css/SelectorPseudoClassAndCompatibilityElementMap.in gperf "$(FEATURE_AND_PLATFORM_DEFINES)"

SelectorPseudoElementTypeMap.cpp : $(WebCore)/css/makeSelectorPseudoElementsMap.py $(WebCore)/css/SelectorPseudoElementTypeMap.in $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PYTHON) "$(WebCore)/css/makeSelectorPseudoElementsMap.py" $(WebCore)/css/SelectorPseudoElementTypeMap.in gperf "$(FEATURE_AND_PLATFORM_DEFINES)"

# --------

# DOMJIT Abstract Heap

all : DOMJITAbstractHeapRepository.h

DOMJITAbstractHeapRepository.h : $(WebCore)/domjit/generate-abstract-heap.rb $(WebCore)/domjit/DOMJITAbstractHeapRepository.yaml
	$(RUBY) "$(WebCore)/domjit/generate-abstract-heap.rb" $(WebCore)/domjit/DOMJITAbstractHeapRepository.yaml DOMJITAbstractHeapRepository.h

# --------

# XMLViewer CSS

all : XMLViewerCSS.h

XMLViewerCSS.h : $(WebCore)/xml/XMLViewer.css
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/cssmin.py < "$(WebCore)/xml/XMLViewer.css" > XMLViewer.min.css
	$(PERL) $(JavaScriptCore_SCRIPTS_DIR)/xxd.pl XMLViewer_css XMLViewer.min.css XMLViewerCSS.h
	$(DELETE) XMLViewer.min.css

# --------

# XMLViewer JS

all : XMLViewerJS.h

XMLViewerJS.h : $(WebCore)/xml/XMLViewer.js
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/jsmin.py < "$(WebCore)/xml/XMLViewer.js" > XMLViewer.min.js
	$(PERL) $(JavaScriptCore_SCRIPTS_DIR)/xxd.pl XMLViewer_js XMLViewer.min.js XMLViewerJS.h
	$(DELETE) XMLViewer.min.js

# --------

# HTML entity names

HTMLEntityTable.cpp : $(WebCore)/html/parser/HTMLEntityNames.in $(WebCore)/html/parser/create-html-entity-table
	$(PYTHON) $(WebCore)/html/parser/create-html-entity-table -o HTMLEntityTable.cpp $(WebCore)/html/parser/HTMLEntityNames.in

# --------

# HTTP header names

HTTP_HEADER_NAMES_FILES = \
    HTTPHeaderNames.cpp \
    HTTPHeaderNames.gperf \
    HTTPHeaderNames.h \
#
HTTP_HEADER_NAMES_FILES_PATTERNS = $(subst .,%,$(HTTP_HEADER_NAMES_FILES))

all : $(HTTP_HEADER_NAMES_FILES)
$(HTTP_HEADER_NAMES_FILES_PATTERNS) : $(WebCore)/platform/network/HTTPHeaderNames.in $(WebCore)/platform/network/create-http-header-name-table
	$(PYTHON) $(WebCore)/platform/network/create-http-header-name-table $(WebCore)/platform/network/HTTPHeaderNames.in gperf

# --------

# color names

ColorData.cpp : $(WebCore)/platform/ColorData.gperf $(WebCore)/make-hash-tools.pl
	$(PERL) $(WebCore)/make-hash-tools.pl . $(WebCore)/platform/ColorData.gperf

# --------

# .strings files

POSSIBLE_LOCALIZABLE_STRINGS_FILES := $(wildcard $(foreach ADDITIONS_PATH,$(ADDITIONS_PATHS),$(ADDITIONS_PATH)/LocalizableAdditions.strings.txt))

LOCALIZABLE_STRINGS_FILE = $(word 1,$(POSSIBLE_LOCALIZABLE_STRINGS_FILES))

LocalizableAdditions.strings.out : $(WebCore)/preprocess-localizable-strings.pl $(WebCore)/bindings/scripts/preprocessor.pm $(LOCALIZABLE_STRINGS_FILE) $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) $< --defines "$(FEATURE_AND_PLATFORM_DEFINES)" $@ $(LOCALIZABLE_STRINGS_FILE)

# --------

# modern media controls

POSSIBLE_ADDITIONAL_MODERN_MEDIA_CONTROLS_STYLE_SHEETS = \
    $(foreach \
        STYLE_SHEET, \
        $(ADDITIONAL_MODERN_MEDIA_CONTROLS_STYLE_SHEETS), \
        $(firstword $(realpath $(foreach \
            ADDITIONS_PATH, \
            $(ADDITIONS_PATHS), \
            $(ADDITIONS_PATH)/$(STYLE_SHEET)))))

MODERN_MEDIA_CONTROLS_STYLE_SHEETS = \
    $(WebCore)/Modules/modern-media-controls/controls/activity-indicator.css \
    $(WebCore)/Modules/modern-media-controls/controls/airplay-button.css \
    $(WebCore)/Modules/modern-media-controls/controls/background-tint.css \
    $(WebCore)/Modules/modern-media-controls/controls/button.css \
    $(WebCore)/Modules/modern-media-controls/controls/buttons-container.css \
    $(WebCore)/Modules/modern-media-controls/controls/controls-bar.css \
    $(WebCore)/Modules/modern-media-controls/controls/inline-media-controls.css \
    $(WebCore)/Modules/modern-media-controls/controls/ios-inline-media-controls.css \
    $(WebCore)/Modules/modern-media-controls/controls/macos-fullscreen-media-controls.css \
    $(WebCore)/Modules/modern-media-controls/controls/macos-inline-media-controls.css \
    $(WebCore)/Modules/modern-media-controls/controls/media-controls.css \
    $(WebCore)/Modules/modern-media-controls/controls/media-document.css \
    $(WebCore)/Modules/modern-media-controls/controls/placard.css \
    $(WebCore)/Modules/modern-media-controls/controls/slider-base.css \
    $(WebCore)/Modules/modern-media-controls/controls/slider.css \
    $(WebCore)/Modules/modern-media-controls/controls/status-label.css \
    $(WebCore)/Modules/modern-media-controls/controls/text-tracks.css \
    $(WebCore)/Modules/modern-media-controls/controls/time-label.css \
    $(WebCore)/Modules/modern-media-controls/controls/watchos-activity-indicator.css \
    $(WebCore)/Modules/modern-media-controls/controls/watchos-media-controls.css \
    $(POSSIBLE_ADDITIONAL_MODERN_MEDIA_CONTROLS_STYLE_SHEETS) \
#

all : ModernMediaControls.css

ModernMediaControls.css : $(MODERN_MEDIA_CONTROLS_STYLE_SHEETS)
	cat $^ > ModernMediaControls.css

# user agent style sheets

POSSIBLE_ADDITIONAL_USER_AGENT_STYLE_SHEETS = \
    $(foreach \
        STYLE_SHEET, \
        $(ADDITIONAL_USER_AGENT_STYLE_SHEETS), \
        $(firstword $(realpath $(foreach \
            ADDITIONS_PATH, \
            $(ADDITIONS_PATHS), \
            $(ADDITIONS_PATH)/$(STYLE_SHEET)))))

USER_AGENT_STYLE_SHEETS = \
    $(WebCore)/css/dialog.css \
    $(WebCore)/css/fullscreen.css \
    $(WebCore)/css/horizontalFormControls.css \
    $(WebCore)/css/html.css \
    $(WebCore)/css/legacyFormControlsIOS.css \
    $(WebCore)/css/mathml.css \
    $(WebCore)/css/mediaControls.css \
    $(WebCore)/css/plugIns.css \
    $(WebCore)/css/quirks.css \
    $(WebCore)/css/svg.css \
    $(WebCore)/html/shadow/mac/imageControlsMac.css \
    $(WebCore)/html/shadow/imageOverlay.css \
    $(WebCore)/html/shadow/meterElementShadow.css \
    ModernMediaControls.css \
    $(POSSIBLE_ADDITIONAL_USER_AGENT_STYLE_SHEETS) \
#

USER_AGENT_STYLE_SHEETS_FILES = UserAgentStyleSheets.h UserAgentStyleSheetsData.cpp
USER_AGENT_STYLE_SHEETS_FILES_PATTERNS = $(subst .,%,$(USER_AGENT_STYLE_SHEETS_FILES))

all : $(USER_AGENT_STYLE_SHEETS_FILES)

$(USER_AGENT_STYLE_SHEETS_FILES_PATTERNS) : $(WebCore)/css/make-css-file-arrays.pl $(WebCore)/bindings/scripts/preprocessor.pm $(USER_AGENT_STYLE_SHEETS) $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) $< --defines "$(FEATURE_AND_PLATFORM_DEFINES)" $(USER_AGENT_STYLE_SHEETS_FILES) $(USER_AGENT_STYLE_SHEETS)

# --------

# modern media controls

POSSIBLE_ADDITIONAL_MODERN_MEDIA_CONTROLS_SCRIPTS = \
    $(foreach \
        SCRIPT, \
        $(ADDITIONAL_MODERN_MEDIA_CONTROLS_SCRIPTS), \
        $(firstword $(realpath $(foreach \
            ADDITIONS_PATH, \
            $(ADDITIONS_PATHS), \
            $(ADDITIONS_PATH)/$(SCRIPT)))))

MODERN_MEDIA_CONTROLS_SCRIPTS = \
    $(WebCore)/Modules/modern-media-controls/main.js \
    $(WebCore)/Modules/modern-media-controls/gesture-recognizers/gesture-recognizer.js \
    $(WebCore)/Modules/modern-media-controls/gesture-recognizers/tap.js \
    $(WebCore)/Modules/modern-media-controls/gesture-recognizers/pinch.js \
    $(WebCore)/Modules/modern-media-controls/controls/scheduler.js \
    $(WebCore)/Modules/modern-media-controls/controls/layout-traits.js \
    $(WebCore)/Modules/modern-media-controls/controls/layout-node.js \
    $(WebCore)/Modules/modern-media-controls/controls/layout-item.js \
    $(WebCore)/Modules/modern-media-controls/controls/icon-service.js \
    $(WebCore)/Modules/modern-media-controls/controls/background-tint.js \
    $(WebCore)/Modules/modern-media-controls/controls/time-control.js \
    $(WebCore)/Modules/modern-media-controls/controls/time-label.js \
    $(WebCore)/Modules/modern-media-controls/controls/slider-base.js \
    $(WebCore)/Modules/modern-media-controls/controls/slider.js \
    $(WebCore)/Modules/modern-media-controls/controls/button.js \
    $(WebCore)/Modules/modern-media-controls/controls/play-pause-button.js \
    $(WebCore)/Modules/modern-media-controls/controls/skip-back-button.js \
    $(WebCore)/Modules/modern-media-controls/controls/skip-forward-button.js \
    $(WebCore)/Modules/modern-media-controls/controls/mute-button.js \
    $(WebCore)/Modules/modern-media-controls/controls/airplay-button.js \
    $(WebCore)/Modules/modern-media-controls/controls/pip-button.js \
    $(WebCore)/Modules/modern-media-controls/controls/tracks-button.js \
    $(WebCore)/Modules/modern-media-controls/controls/fullscreen-button.js \
    $(WebCore)/Modules/modern-media-controls/controls/seek-button.js \
    $(WebCore)/Modules/modern-media-controls/controls/rewind-button.js \
    $(WebCore)/Modules/modern-media-controls/controls/forward-button.js \
    $(WebCore)/Modules/modern-media-controls/controls/overflow-button.js \
    $(WebCore)/Modules/modern-media-controls/controls/close-button.js \
    $(WebCore)/Modules/modern-media-controls/controls/buttons-container.js \
    $(WebCore)/Modules/modern-media-controls/controls/status-label.js \
    $(WebCore)/Modules/modern-media-controls/controls/controls-bar.js \
    $(WebCore)/Modules/modern-media-controls/controls/auto-hide-controller.js \
    $(WebCore)/Modules/modern-media-controls/controls/media-controls.js \
    $(WebCore)/Modules/modern-media-controls/controls/background-click-delegate-notifier.js \
    $(WebCore)/Modules/modern-media-controls/controls/inline-media-controls.js \
    $(WebCore)/Modules/modern-media-controls/controls/ios-inline-media-controls.js \
    $(WebCore)/Modules/modern-media-controls/controls/ios-layout-traits.js \
    $(WebCore)/Modules/modern-media-controls/controls/macos-inline-media-controls.js \
    $(WebCore)/Modules/modern-media-controls/controls/macos-fullscreen-media-controls.js \
    $(WebCore)/Modules/modern-media-controls/controls/macos-layout-traits.js \
    $(WebCore)/Modules/modern-media-controls/controls/placard.js \
    $(WebCore)/Modules/modern-media-controls/controls/airplay-placard.js \
    $(WebCore)/Modules/modern-media-controls/controls/invalid-placard.js \
    $(WebCore)/Modules/modern-media-controls/controls/pip-placard.js \
    $(WebCore)/Modules/modern-media-controls/controls/watchos-activity-indicator.js \
    $(WebCore)/Modules/modern-media-controls/controls/watchos-media-controls.js \
    $(WebCore)/Modules/modern-media-controls/controls/watchos-layout-traits.js \
    $(WebCore)/Modules/modern-media-controls/media/media-controller-support.js \
    $(WebCore)/Modules/modern-media-controls/media/airplay-support.js \
    $(WebCore)/Modules/modern-media-controls/media/audio-support.js \
    $(WebCore)/Modules/modern-media-controls/media/close-support.js \
    $(WebCore)/Modules/modern-media-controls/media/controls-visibility-support.js \
    $(WebCore)/Modules/modern-media-controls/media/fullscreen-support.js \
    $(WebCore)/Modules/modern-media-controls/media/mute-support.js \
    $(WebCore)/Modules/modern-media-controls/media/overflow-support.js \
    $(WebCore)/Modules/modern-media-controls/media/pip-support.js \
    $(WebCore)/Modules/modern-media-controls/media/placard-support.js \
    $(WebCore)/Modules/modern-media-controls/media/playback-support.js \
    $(WebCore)/Modules/modern-media-controls/media/scrubbing-support.js \
    $(WebCore)/Modules/modern-media-controls/media/seek-support.js \
    $(WebCore)/Modules/modern-media-controls/media/seek-backward-support.js \
    $(WebCore)/Modules/modern-media-controls/media/seek-forward-support.js \
    $(WebCore)/Modules/modern-media-controls/media/skip-back-support.js \
    $(WebCore)/Modules/modern-media-controls/media/skip-forward-support.js \
    $(WebCore)/Modules/modern-media-controls/media/start-support.js \
    $(WebCore)/Modules/modern-media-controls/media/status-support.js \
    $(WebCore)/Modules/modern-media-controls/media/time-control-support.js \
    $(WebCore)/Modules/modern-media-controls/media/tracks-support.js \
    $(WebCore)/Modules/modern-media-controls/media/volume-support.js \
    $(WebCore)/Modules/modern-media-controls/media/media-document-controller.js \
    $(WebCore)/Modules/modern-media-controls/media/watchos-media-controls-support.js \
    $(WebCore)/Modules/modern-media-controls/media/media-controller.js \
    $(POSSIBLE_ADDITIONAL_MODERN_MEDIA_CONTROLS_SCRIPTS) \
#

all : ModernMediaControls.js

ModernMediaControls.js : $(MODERN_MEDIA_CONTROLS_SCRIPTS)
	cat $^ > ModernMediaControls.js

# user agent scripts

USER_AGENT_SCRIPTS = \
    ModernMediaControls.js \
#

USER_AGENT_SCRIPTS_FILES = \
    UserAgentScripts.h \
    UserAgentScriptsData.cpp \
#
USER_AGENT_SCRIPTS_FILES_PATTERNS = $(subst .,%,$(USER_AGENT_SCRIPTS_FILES))

all : $(USER_AGENT_SCRIPTS_FILES)

$(USER_AGENT_SCRIPTS_FILES_PATTERNS) : $(JavaScriptCore_SCRIPTS_DIR)/make-js-file-arrays.py $(USER_AGENT_SCRIPTS)
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/make-js-file-arrays.py -n WebCore --fail-if-non-ascii $(USER_AGENT_SCRIPTS_FILES) $(USER_AGENT_SCRIPTS)

# --------

# plug-ins resources

PLUG_INS_RESOURCES = $(WebCore)/Resources/plugIns.js

# order matters -- make-css-file-arrays.pl takes the header and then the source file path
PLUG_INS_RESOURCES_FILES = PlugInsResources.h PlugInsResourcesData.cpp
PLUG_INS_RESOURCES_FILES_PATTERNS = $(subst .,%,$(PLUG_INS_RESOURCES_FILES))

all : $(PLUG_INS_RESOURCES_FILES)

$(PLUG_INS_RESOURCES_FILES_PATTERNS) : $(WebCore)/css/make-css-file-arrays.pl $(WebCore)/bindings/scripts/preprocessor.pm $(PLUG_INS_RESOURCES) $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) $< --defines "$(FEATURE_AND_PLATFORM_DEFINES)" $(PLUG_INS_RESOURCES_FILES) $(PLUG_INS_RESOURCES)

# --------

WEBKIT_FONT_FAMILY_NAME_FILES = \
    WebKitFontFamilyNames.cpp \
    WebKitFontFamilyNames.h \
#
WEBKIT_FONT_FAMILY_NAME_FILES_PATTERNS = $(subst .,%,$(WEBKIT_FONT_FAMILY_NAME_FILES))

all : $(WEBKIT_FONT_FAMILY_NAME_FILES)
$(WEBKIT_FONT_FAMILY_NAME_FILES_PATTERNS): $(WebCore)/dom/make_names.pl $(WebCore)/bindings/scripts/Hasher.pm $(WebCore)/bindings/scripts/StaticString.pm $(WebCore)/css/WebKitFontFamilyNames.in
	$(PERL) $< --fonts $(WebCore)/css/WebKitFontFamilyNames.in

# HTML tag and attribute names

HTML_TAG_FILES = \
    JSHTMLElementWrapperFactory.cpp \
    JSHTMLElementWrapperFactory.h \
    HTMLElementFactory.cpp \
    HTMLElementFactory.h \
    HTMLElementTypeHelpers.h \
    HTMLNames.cpp \
    HTMLNames.h \
#
HTML_TAG_FILES_PATTERNS = $(subst .,%,$(HTML_TAG_FILES))

all : $(HTML_TAG_FILES)

$(HTML_TAG_FILES_PATTERNS) : $(WebCore)/dom/make_names.pl $(WebCore)/bindings/scripts/Hasher.pm $(WebCore)/bindings/scripts/StaticString.pm $(WebCore)/html/HTMLTagNames.in $(WebCore)/html/HTMLAttributeNames.in $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) $< --elements $(WebCore)/html/HTMLTagNames.in --attrs $(WebCore)/html/HTMLAttributeNames.in --factory --wrapperFactory

XML_NS_NAMES_FILES = XMLNSNames.cpp XMLNSNames.h
XML_NS_NAMES_FILES_PATTERNS = $(subst .,%,$(XML_NS_NAMES_FILES))

all : $(XML_NS_NAMES_FILES)

$(XML_NS_NAMES_FILES_PATTERNS) : $(WebCore)/dom/make_names.pl $(WebCore)/bindings/scripts/Hasher.pm $(WebCore)/bindings/scripts/StaticString.pm $(WebCore)/xml/xmlnsattrs.in
	$(PERL) $< --attrs $(WebCore)/xml/xmlnsattrs.in
	
XML_NAMES_FILES = XMLNames.cpp XMLNames.h
XML_NAMES_FILES_PATTERNS = $(subst .,%,$(XML_NAMES_FILES))

all : $(XML_NAMES_FILES)

$(XML_NAMES_FILES_PATTERNS) : $(WebCore)/dom/make_names.pl $(WebCore)/bindings/scripts/Hasher.pm $(WebCore)/bindings/scripts/StaticString.pm $(WebCore)/xml/xmlattrs.in
	$(PERL) $< --attrs $(WebCore)/xml/xmlattrs.in

# --------

# SVG tag and attribute names, and element factory

SVG_TAG_FILES = \
    JSSVGElementWrapperFactory.cpp \
    JSSVGElementWrapperFactory.h \
    SVGElementFactory.cpp \
    SVGElementFactory.h \
    SVGElementTypeHelpers.h \
    SVGNames.cpp \
    SVGNames.h \
#
SVG_TAG_FILES_PATTERNS = $(subst .,%,$(SVG_TAG_FILES))

all : $(SVG_TAG_FILES)

$(SVG_TAG_FILES_PATTERNS) : $(WebCore)/dom/make_names.pl $(WebCore)/bindings/scripts/Hasher.pm $(WebCore)/bindings/scripts/StaticString.pm $(WebCore)/svg/svgtags.in $(WebCore)/svg/svgattrs.in $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) $< --elements $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in --factory --wrapperFactory

XLINK_NAMES_FILES = XLinkNames.cpp XLinkNames.h
XLINK_NAMES_FILES_PATTERNS = $(subst .,%,$(XLINK_NAMES_FILES))

all : $(XLINK_NAMES_FILES)

$(XLINK_NAMES_FILES_PATTERNS) : $(WebCore)/dom/make_names.pl $(WebCore)/bindings/scripts/Hasher.pm $(WebCore)/bindings/scripts/StaticString.pm $(WebCore)/svg/xlinkattrs.in
	$(PERL) $< --attrs $(WebCore)/svg/xlinkattrs.in

# --------

# Register event constructors and targets

EVENT_NAMES = $(WebCore)/dom/EventNames.in $(ADDITIONAL_EVENT_NAMES)

EVENT_FACTORY_FILES = \
    EventFactory.cpp \
    EventHeaders.h \
    EventInterfaces.h \
#
EVENT_FACTORY_PATTERNS = $(subst .,%,$(EVENT_FACTORY_FILES))

all : $(EVENT_FACTORY_FILES)
$(EVENT_FACTORY_PATTERNS) : $(WebCore)/dom/make_event_factory.pl $(EVENT_NAMES)
	$(PERL) $< $(addprefix --input , $(filter-out $(WebCore)/dom/make_event_factory.pl, $^))

EVENT_TARGET_FACTORY = $(WebCore)/dom/EventTargetFactory.in $(ADDITIONAL_EVENT_TARGET_FACTORY)

EVENT_TARGET_FACTORY_FILES = \
    EventTargetFactory.cpp \
    EventTargetHeaders.h \
    EventTargetInterfaces.h \
#
EVENT_TARGET_FACTORY_PATTERNS = $(subst .,%,$(EVENT_TARGET_FACTORY_FILES))

all : $(EVENT_TARGET_FACTORY_FILES)
$(EVENT_TARGET_FACTORY_PATTERNS) : $(WebCore)/dom/make_event_factory.pl $(EVENT_TARGET_FACTORY)
	$(PERL) $< $(addprefix --input , $(filter-out $(WebCore)/dom/make_event_factory.pl, $^))

# --------

# MathML tag and attribute names, and element factory

MATH_ML_GENERATED_FILES = \
    JSMathMLElementWrapperFactory.cpp \
    JSMathMLElementWrapperFactory.h \
    MathMLElementFactory.cpp \
    MathMLElementFactory.h \
    MathMLElementTypeHelpers.h \
    MathMLNames.cpp \
    MathMLNames.h \
#
MATH_ML_GENERATED_PATTERNS = $(subst .,%,$(MATH_ML_GENERATED_FILES))

all : $(MATH_ML_GENERATED_FILES)
$(MATH_ML_GENERATED_PATTERNS) : $(WebCore)/dom/make_names.pl $(WebCore)/bindings/scripts/Hasher.pm $(WebCore)/bindings/scripts/StaticString.pm $(WebCore)/mathml/mathtags.in $(WebCore)/mathml/mathattrs.in
	$(PERL) $< --elements $(WebCore)/mathml/mathtags.in --attrs $(WebCore)/mathml/mathattrs.in --factory --wrapperFactory

# --------

# TagName, ElementName, and Namespace enums

DOM_NAME_ENUM_DEPS = \
    $(WebCore)/dom/make_names.pl \
    $(WebCore)/bindings/scripts/Hasher.pm \
    $(WebCore)/bindings/scripts/StaticString.pm \
    $(WebCore)/html/HTMLTagNames.in \
    $(WebCore)/svg/svgtags.in \
    $(WebCore)/mathml/mathtags.in \
    $(WebCore)/html/HTMLAttributeNames.in \
    $(WebCore)/mathml/mathattrs.in \
    $(WebCore)/svg/svgattrs.in \
    $(WebCore)/svg/xlinkattrs.in \
    $(WebCore)/xml/xmlattrs.in \
    $(WebCore)/xml/xmlnsattrs.in \
#

DOM_NAME_ENUM_ARGUMENTS = \
    --elements $(WebCore)/html/HTMLTagNames.in \
    --elements $(WebCore)/svg/svgtags.in \
    --elements $(WebCore)/mathml/mathtags.in \
    --attrs $(WebCore)/html/HTMLAttributeNames.in \
    --attrs $(WebCore)/mathml/mathattrs.in \
    --attrs $(WebCore)/svg/svgattrs.in \
    --attrs $(WebCore)/svg/xlinkattrs.in \
    --attrs $(WebCore)/xml/xmlattrs.in \
    --attrs $(WebCore)/xml/xmlnsattrs.in \
#

TAG_NAME_GENERATED_FILES = \
    TagName.cpp \
    TagName.h \
#
TAG_NAME_GENERATED_PATTERNS = $(subst .,%,$(TAG_NAME_GENERATED_FILES))

all : $(TAG_NAME_GENERATED_FILES)
$(TAG_NAME_GENERATED_PATTERNS) : $(DOM_NAME_ENUM_DEPS)
	$(PERL) $< --enum TagName $(DOM_NAME_ENUM_ARGUMENTS)

ELEMENT_NAME_GENERATED_FILES = \
    ElementName.cpp \
    ElementName.h \
#
ELEMENT_NAME_GENERATED_PATTERNS = $(subst .,%,$(ELEMENT_NAME_GENERATED_FILES))

all : $(ELEMENT_NAME_GENERATED_FILES)
$(ELEMENT_NAME_GENERATED_PATTERNS) : $(DOM_NAME_ENUM_DEPS)
	$(PERL) $< --enum ElementName $(DOM_NAME_ENUM_ARGUMENTS)

NAMESPACE_GENERATED_FILES = \
    Namespace.cpp \
    Namespace.h \
#
NAMESPACE_GENERATED_PATTERNS = $(subst .,%,$(NAMESPACE_GENERATED_FILES))

all : $(NAMESPACE_GENERATED_FILES)
$(NAMESPACE_GENERATED_PATTERNS) : $(DOM_NAME_ENUM_DEPS)
	$(PERL) $< --enum Namespace $(DOM_NAME_ENUM_ARGUMENTS)

# --------

# Internal Settings

WEB_PREFERENCES_INPUT_FILES = \
    ${WTF_BUILD_SCRIPTS_DIR}/Preferences/UnifiedWebPreferences.yaml \
    ${WebCore}/page/Settings.yaml \
#

GENERATE_SETTINGS = \
    InternalSettingsGenerated.cpp \
    InternalSettingsGenerated.idl \
    InternalSettingsGenerated.h \
    Settings.cpp \
    Settings.h \
#

all : $(GENERATE_SETTINGS)

$(GENERATE_SETTINGS) : % : $(WebCore)/Scripts/SettingsTemplates/%.erb $(WEB_PREFERENCES_INPUT_FILES) $(WebCore)/Scripts/GenerateSettings.rb
	$(RUBY) $(WebCore)/Scripts/GenerateSettings.rb $(WEB_PREFERENCES_INPUT_FILES) --template $<

# --------

# Common generator things

COMMON_BINDINGS_SCRIPTS = \
    $(WebCore)/bindings/scripts/CodeGenerator.pm \
    $(WebCore)/bindings/scripts/IDLParser.pm \
    $(WebCore)/bindings/scripts/generate-bindings.pl \
    $(WebCore)/bindings/scripts/preprocessor.pm

PREPROCESS_IDLS_SCRIPTS = \
    $(WebCore)/bindings/scripts/preprocess-idls.pl

# JS bindings generator

IDL_INCLUDES = \
    $(WebCore)/Modules \
    $(ADDITIONAL_BINDING_IDLS_PATHS)

IDL_COMMON_ARGS = $(IDL_INCLUDES:%=--include %) --write-dependencies --outputDir .

JS_BINDINGS_SCRIPTS = $(COMMON_BINDINGS_SCRIPTS) $(WebCore)/bindings/scripts/CodeGeneratorJS.pm

SUPPLEMENTAL_DEPENDENCY_FILE = SupplementalDependencies.txt
SUPPLEMENTAL_MAKEFILE_DEPS = SupplementalDependencies.dep
ISO_SUBSPACES_HEADER_FILE = DOMIsoSubspaces.h
CLIENT_ISO_SUBSPACES_HEADER_FILE = DOMClientIsoSubspaces.h
CONSTRUCTORS_HEADER_FILE = DOMConstructors.h
WINDOW_CONSTRUCTORS_FILE = DOMWindowConstructors.idl
WORKERGLOBALSCOPE_CONSTRUCTORS_FILE = WorkerGlobalScopeConstructors.idl
SHADOWREALMGLOBALSCOPE_CONSTRUCTORS_FILE = ShadowRealmGlobalScopeConstructors.idl
DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE = DedicatedWorkerGlobalScopeConstructors.idl
SERVICEWORKERGLOBALSCOPE_CONSTRUCTORS_FILE = ServiceWorkerGlobalScopeConstructors.idl
SHAREDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE = SharedWorkerGlobalScopeConstructors.idl
WORKLETGLOBALSCOPE_CONSTRUCTORS_FILE = WorkletGlobalScopeConstructors.idl
PAINTWORKLETGLOBALSCOPE_CONSTRUCTORS_FILE = PaintWorkletGlobalScopeConstructors.idl
AUDIOWORKLETGLOBALSCOPE_CONSTRUCTORS_FILE = AudioWorkletGlobalScopeConstructors.idl
IDL_ATTRIBUTES_FILE = $(WebCore)/bindings/scripts/IDLAttributes.json

IDL_INTERMEDIATE_FILES = \
    $(SUPPLEMENTAL_MAKEFILE_DEPS) \
    $(SUPPLEMENTAL_DEPENDENCY_FILE) \
    $(ISO_SUBSPACES_HEADER_FILE) \
    $(CLIENT_ISO_SUBSPACES_HEADER_FILE) \
    $(CONSTRUCTORS_HEADER_FILE) \
    $(WINDOW_CONSTRUCTORS_FILE) \
    $(WORKERGLOBALSCOPE_CONSTRUCTORS_FILE) \
    $(SHADOWREALMGLOBALSCOPE_CONSTRUCTORS_FILE) \
    $(DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE) \
    $(SERVICEWORKERGLOBALSCOPE_CONSTRUCTORS_FILE) \
    $(WORKLETGLOBALSCOPE_CONSTRUCTORS_FILE) \
    $(PAINTWORKLETGLOBALSCOPE_CONSTRUCTORS_FILE) \
    $(AUDIOWORKLETGLOBALSCOPE_CONSTRUCTORS_FILE)
#
IDL_INTERMEDIATE_PATTERNS = $(subst .,%,$(IDL_INTERMEDIATE_FILES))

IDL_FILE_NAMES_LIST = IDLFileNamesList.txt
$(IDL_FILE_NAMES_LIST) : $(JS_BINDING_IDLS)
	echo $(JS_BINDING_IDLS) | tr " " "\n" > $@

$(IDL_INTERMEDIATE_PATTERNS) : $(PREPROCESS_IDLS_SCRIPTS) $(IDL_ATTRIBUTES_FILE) $(IDL_FILE_NAMES_LIST) $(JS_BINDING_IDLS) $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) $(WebCore)/bindings/scripts/preprocess-idls.pl --defines "$(FEATURE_AND_PLATFORM_DEFINES) LANGUAGE_JAVASCRIPT" --idlFileNamesList $(IDL_FILE_NAMES_LIST) --idlAttributesFile $(IDL_ATTRIBUTES_FILE) --supplementalDependencyFile $(SUPPLEMENTAL_DEPENDENCY_FILE) --isoSubspacesHeaderFile $(ISO_SUBSPACES_HEADER_FILE) --clientISOSubspacesHeaderFile $(CLIENT_ISO_SUBSPACES_HEADER_FILE) --constructorsHeaderFile $(CONSTRUCTORS_HEADER_FILE) --windowConstructorsFile $(WINDOW_CONSTRUCTORS_FILE) --workerGlobalScopeConstructorsFile $(WORKERGLOBALSCOPE_CONSTRUCTORS_FILE) --shadowRealmGlobalScopeConstructorsFile $(SHADOWREALMGLOBALSCOPE_CONSTRUCTORS_FILE) --dedicatedWorkerGlobalScopeConstructorsFile $(DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE) --serviceWorkerGlobalScopeConstructorsFile $(SERVICEWORKERGLOBALSCOPE_CONSTRUCTORS_FILE) --sharedWorkerGlobalScopeConstructorsFile $(SHAREDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE) --workletGlobalScopeConstructorsFile $(WORKLETGLOBALSCOPE_CONSTRUCTORS_FILE) --paintWorkletGlobalScopeConstructorsFile $(PAINTWORKLETGLOBALSCOPE_CONSTRUCTORS_FILE) --audioWorkletGlobalScopeConstructorsFile $(AUDIOWORKLETGLOBALSCOPE_CONSTRUCTORS_FILE) --supplementalMakefileDeps $(SUPPLEMENTAL_MAKEFILE_DEPS)

#
# Emit the rules to generate bindings from IDL files. Note that there are
# several scenarios we need to support:
#
# * Files such as GestureEvent.idl can be provided by some facility external to
#   the WebKit repository. The file can be made available by appearing in
#   BUILT_PRODUCTS_DIR or SDKROOT. If found in either of those locations, the
#   file will be used, otherwise, there will be no GestureEvent facility.
#
# * Files such as TouchEvent.idl can also be found in BUILT_PRODUCTS_DIR or
#   SDKROOT. However, there is also a version of the file in WebCore/dom. If
#   the file can't be found in either of the first two locations, the version
#   in WebCore/dom will be used.
#
# * Files such as ApplePaySetup.idl used to be provided externally in
#   BUILT_PRODUCTS_DIR or SDKROOT, but are now "upstreamed" into
#   WebCore/Modules/applepay. Because they were previously found in SDKROOT,
#   building against old SDKs will cause the build system to be exposed to old,
#   out-of-date versions of ApplePaySetup.idl. In these cases, we want to
#   *ignore* the externally-provided versions and use the versions found in
#   WebCore.
#
# To build the IDL files, we used to have a rule like the following:
#
#   JS%.cpp JS%.h : %.idl ...
#       ...
#
# This rule made use of vpath to find the %.idl files in all their myriad
# locations. However, because of the three scenarios previously outlined, that
# approach is problematic. In some cases, we want to prefer the files in
# BUILT_PRODUCTS_DIR or SDKROOT over those in WebCore. In other cases, it's the
# other way around. So we can't use vpath to find the files indicated by %.idl.
#
# Instead, since the IDL files come from many different sources, we need a
# separate rule for each one, with each rule incorporating the full path of the
# file. Fortunately, we can do this pretty simply with the $(foreach) function.
#
# All that said, we do still need to invoke vpath. There are some
# auto-generated dependency files (such as SupplementalDependencies.dep) that
# generate file information without paths, and those still need to benefit from
# setting search paths with vpath.

vpath %.idl $(ADDITIONAL_BINDING_IDLS_PATHS) $(WebCore)/bindings/scripts

# -------------------------------------------------
define GENERATE_BINDINGS_template

JS$(call get_bare_name,$(1)).cpp JS$(call get_bare_name,$(1)).h: $(1) $$(JS_BINDINGS_SCRIPTS) $$(IDL_ATTRIBUTES_FILE) $$(IDL_INTERMEDIATE_FILES) $$(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$$(PERL) $$(WebCore)/bindings/scripts/generate-bindings.pl \
		$$(IDL_COMMON_ARGS) \
		--defines "$$(FEATURE_AND_PLATFORM_DEFINES) LANGUAGE_JAVASCRIPT" \
		--generator JS \
		--idlAttributesFile $$(IDL_ATTRIBUTES_FILE) \
		--supplementalDependencyFile $$(SUPPLEMENTAL_DEPENDENCY_FILE) \
		$$<

endef
# -------------------------------------------------

$(foreach IDL_FILE,$(JS_BINDING_IDLS),$(eval $(call GENERATE_BINDINGS_template,$(IDL_FILE))))

ifneq ($(NO_SUPPLEMENTAL_FILES),1)
-include $(SUPPLEMENTAL_MAKEFILE_DEPS)
endif

ifneq ($(NO_SUPPLEMENTAL_FILES),1)
-include $(JS_DOM_HEADERS:.h=.dep)
endif

# WebCore JS Builtins

WebCore_BUILTINS_SOURCES = \
    $(WebCore)/Modules/compression/CompressionStream.js \
    $(WebCore)/Modules/compression/DecompressionStream.js \
    $(WebCore)/Modules/streams/ByteLengthQueuingStrategy.js \
    $(WebCore)/Modules/streams/CountQueuingStrategy.js \
    $(WebCore)/Modules/streams/ReadableByteStreamController.js \
    $(WebCore)/Modules/streams/ReadableByteStreamInternals.js \
    $(WebCore)/Modules/streams/ReadableStream.js \
    $(WebCore)/Modules/streams/ReadableStreamBYOBRequest.js \
    $(WebCore)/Modules/streams/ReadableStreamDefaultController.js \
    $(WebCore)/Modules/streams/ReadableStreamInternals.js \
    $(WebCore)/Modules/streams/ReadableStreamBYOBReader.js \
    $(WebCore)/Modules/streams/ReadableStreamDefaultReader.js \
    $(WebCore)/Modules/streams/StreamInternals.js \
    $(WebCore)/Modules/streams/TransformStream.js \
    $(WebCore)/Modules/streams/TransformStreamDefaultController.js \
    $(WebCore)/Modules/streams/TransformStreamInternals.js \
    $(WebCore)/Modules/streams/WritableStreamDefaultController.js \
    $(WebCore)/Modules/streams/WritableStreamDefaultWriter.js \
    $(WebCore)/Modules/streams/WritableStreamInternals.js \
    $(WebCore)/dom/TextDecoderStream.js \
    $(WebCore)/dom/TextEncoderStream.js \
    $(WebCore)/bindings/js/JSDOMBindingInternals.js \
    $(WebCore)/inspector/CommandLineAPIModuleSource.js \
#

BUILTINS_GENERATOR_SCRIPTS = \
    $(JavaScriptCore_SCRIPTS_DIR)/wkbuiltins.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generator.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_model.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_templates.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_combined_header.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_combined_implementation.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_separate_header.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_separate_implementation.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_internals_wrapper_header.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_internals_wrapper_implementation.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_wrapper_header.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_wrapper_implementation.py \
    $(JavaScriptCore_SCRIPTS_DIR)/generate-js-builtins.py \
    $(JavaScriptCore_SCRIPTS_DIR)/lazywriter.py \
#

WebCore_BUILTINS_WRAPPERS = $(addsuffix Builtins.cpp, $(notdir $(basename $(WebCore_BUILTINS_SOURCES))))
WebCore_BUILTINS_WRAPPERS += \
    WebCoreJSBuiltins.h \
    WebCoreJSBuiltins.cpp \
    WebCoreJSBuiltinInternals.h \
    WebCoreJSBuiltinInternals.cpp \
#
WebCore_BUILTINS_WRAPPERS_PATTERNS = $(subst .,%,$(WebCore_BUILTINS_WRAPPERS))

# Adding/removing scripts should trigger regeneration, but changing which builtins are
# generated should not affect other builtins when not passing '--combined' to the generator.

WebCore_BUILTINS_SOURCES_LIST : $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py $(WebCore)/DerivedSources.make
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py '$(WebCore_BUILTINS_SOURCES)' $@

WebCore_BUILTINS_DEPENDENCIES_LIST : $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py $(WebCore)/DerivedSources.make
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py '$(BUILTINS_GENERATOR_SCRIPTS)' $@

$(WebCore_BUILTINS_WRAPPERS_PATTERNS) : $(WebCore_BUILTINS_SOURCES) WebCore_BUILTINS_SOURCES_LIST $(BUILTINS_GENERATOR_SCRIPTS) WebCore_BUILTINS_DEPENDENCIES_LIST
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/generate-js-builtins.py --wrappers-only --output-directory . --framework WebCore $(WebCore_BUILTINS_SOURCES)

# See comments for IDL_PATHS for a description of what this is for.
vpath %.js $(sort $(foreach f,$(WebCore_BUILTINS_SOURCES),$(realpath $(dir $(f)))))

%Builtins.h: %.js $(BUILTINS_GENERATOR_SCRIPTS) WebCore_BUILTINS_DEPENDENCIES_LIST
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/generate-js-builtins.py --output-directory . --framework WebCore $<

all : $(notdir $(WebCore_BUILTINS_SOURCES:%.js=%Builtins.h)) $(WebCore_BUILTINS_WRAPPERS)

# ------------------------

