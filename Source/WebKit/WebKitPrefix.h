/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif

#include <wtf/Platform.h>

#if PLATFORM(COCOA)

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#ifdef __OBJC__
#if !USE(APPLE_INTERNAL_SDK)
/* SecTask.h declares SecTaskGetCodeSignStatus(...) to
 * be unavailable on macOS, so do not include that header. */
#define _SECURITY_SECTASK_H_
#endif
#import <Foundation/Foundation.h>
#if USE(APPKIT)
#import <Cocoa/Cocoa.h>
#endif
#endif // __OBJC__

#endif // PLATFORM(COCOA)

/* When C++ exceptions are disabled, the C++ library defines |try| and |catch|
* to allow C++ code that expects exceptions to build. These definitions
* interfere with Objective-C++ uses of Objective-C exception handlers, which
* use |@try| and |@catch|. As a workaround, undefine these macros. */

#ifdef __cplusplus
#include <algorithm> // needed for exception_defines.h
#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ThreadSafeWeakHashSet.h>
#include <wtf/URL.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#include <wtf/NativePromise.h>
#include <JavaScriptCore/Heap.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/OptionsList.h>
#include <JavaScriptCore/VM.h>

#if PLATFORM(MAC)
#include <IOKit/hid/IOHIDLib.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreAudioTypes/CoreAudioTypes.h>
#include <CoreMedia/CoreMedia.h>
#include <Network/Network.h>
#include <Security/Security.h>
#include <dlfcn.h>
#include <dns_sd.h>
#include <execinfo.h>
#include <libkern/OSCacheControl.h>
#include <os/signpost.h>
#include <simd/common.h>
#include <simd/conversion.h>
#include <simd/extern.h>
#include <simd/geometry.h>
#include <simd/logic.h>
#include <simd/matrix.h>
#include <simd/matrix_types.h>
#include <simd/packed.h>
#include <simd/quaternion.h>
#include <simd/simd.h>
#include <simd/vector_make.h>
#include <simd/vector_types.h>
#include <sqlite3.h>
#endif

#if defined(BUILDING_WITH_CMAKE)
// The expanded prefix below is computed from CMake's `ninja -t deps` and is
// only known to resolve in the CMake header-search configuration; some Xcode
// production targets that share this prefix cannot find <WebCore/...> headers.
#include <JavaScriptCore/AbstractSlotVisitor.h>
#include <JavaScriptCore/ArgList.h>
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/ArrayConventions.h>
#include <JavaScriptCore/ArrayStorage.h>
#include <JavaScriptCore/AssemblerCommon.h>
#include <JavaScriptCore/AuxiliaryBarrier.h>
#include <JavaScriptCore/Butterfly.h>
#include <JavaScriptCore/BytecodeConventions.h>
#include <JavaScriptCore/CachePayload.h>
#include <JavaScriptCore/CacheUpdate.h>
#include <JavaScriptCore/CacheableIdentifier.h>
#include <JavaScriptCore/CachedBytecode.h>
#include <JavaScriptCore/CachedTypes.h>
#include <JavaScriptCore/CagedBarrierPtr.h>
#include <JavaScriptCore/CallData.h>
#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/CalleeBits.h>
#include <JavaScriptCore/CellContainerInlines.h>
#include <JavaScriptCore/ClassInfo.h>
#include <JavaScriptCore/CodeBlockHash.h>
#include <JavaScriptCore/CodeSpecializationKind.h>
#include <JavaScriptCore/Concurrency.h>
#include <JavaScriptCore/ConstantMode.h>
#include <JavaScriptCore/ConstructAbility.h>
#include <JavaScriptCore/ConstructData.h>
#include <JavaScriptCore/CrossTaskToken.h>
#include <JavaScriptCore/CustomGetterSetter.h>
#include <JavaScriptCore/DOMAnnotation.h>
#include <JavaScriptCore/DOMAttributeGetterSetter.h>
#include <JavaScriptCore/DeferredWorkTimer.h>
#include <JavaScriptCore/DefinePropertyAttributes.h>
#include <JavaScriptCore/DeletePropertySlot.h>
#include <JavaScriptCore/DirectArgumentsOffset.h>
#include <JavaScriptCore/DisallowVMEntry.h>
#include <JavaScriptCore/DumpContext.h>
#include <JavaScriptCore/ECMAMode.h>
#include <JavaScriptCore/EnumerationMode.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/ErrorInstance.h>
#include <JavaScriptCore/ErrorType.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/ExceptionScope.h>
#include <JavaScriptCore/ExecutableAllocator.h>
#include <JavaScriptCore/FastJITPermissions.h>
#include <JavaScriptCore/GCOwnedDataScope.h>
#include <JavaScriptCore/GCSegmentedArray.h>
#include <JavaScriptCore/GenericOffset.h>
#include <JavaScriptCore/GetPutInfo.h>
#include <JavaScriptCore/GetVM.h>
#include <JavaScriptCore/HeapCellInlines.h>
#include <JavaScriptCore/Identifier.h>
#include <JavaScriptCore/IndexingHeader.h>
#include <JavaScriptCore/IndexingHeaderInlines.h>
#include <JavaScriptCore/InferredValue.h>
#include <JavaScriptCore/Intrinsic.h>
#include <JavaScriptCore/IterationKind.h>
#include <JavaScriptCore/JITCompilationEffort.h>
#include <JavaScriptCore/JSBigInt.h>
#include <JavaScriptCore/JSCJSValueCell.h>
#include <JavaScriptCore/JSCast.h>
#include <JavaScriptCore/JSCell.h>
#include <JavaScriptCore/JSDestructibleObject.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSGlobalObjectFunctions.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSRunLoopTimer.h>
#include <JavaScriptCore/JSScope.h>
#include <JavaScriptCore/JSSegmentedVariableObject.h>
#include <JavaScriptCore/JSSymbolTableObject.h>
#include <JavaScriptCore/JSType.h>
#include <JavaScriptCore/JSTypeInfo.h>
#include <JavaScriptCore/LazyClassStructure.h>
#include <JavaScriptCore/LazyProperty.h>
#include <JavaScriptCore/LeafExecutable.h>
#include <JavaScriptCore/LineColumn.h>
#include <JavaScriptCore/MarkStack.h>
#include <JavaScriptCore/MarkedBlockInlines.h>
#include <JavaScriptCore/MarkedVector.h>
#include <JavaScriptCore/MathCommon.h>
#include <JavaScriptCore/Microtask.h>
#include <JavaScriptCore/OSCheck.h>
#include <JavaScriptCore/OperationResult.h>
#include <JavaScriptCore/ParserModes.h>
#include <JavaScriptCore/PrivateName.h>
#include <JavaScriptCore/PropertyDescriptor.h>
#include <JavaScriptCore/PropertyName.h>
#include <JavaScriptCore/PropertyNameArray.h>
#include <JavaScriptCore/PropertyOffset.h>
#include <JavaScriptCore/PropertySlot.h>
#include <JavaScriptCore/PropertyStorage.h>
#include <JavaScriptCore/PrototypeKey.h>
#include <JavaScriptCore/PutDirectIndexMode.h>
#include <JavaScriptCore/PutPropertySlot.h>
#include <JavaScriptCore/RegExpCachedResult.h>
#include <JavaScriptCore/RegExpGlobalData.h>
#include <JavaScriptCore/RegExpSubstringGlobalAtomCache.h>
#include <JavaScriptCore/Register.h>
#include <JavaScriptCore/RootMarkReason.h>
#include <JavaScriptCore/RuntimeFlags.h>
#include <JavaScriptCore/RuntimeType.h>
#include <JavaScriptCore/ScopeOffset.h>
#include <JavaScriptCore/ScopedArgumentsTable.h>
#include <JavaScriptCore/Scribble.h>
#include <JavaScriptCore/ScriptFetcher.h>
#include <JavaScriptCore/SlotVisitor.h>
#include <JavaScriptCore/SourceOrigin.h>
#include <JavaScriptCore/SourceProvider.h>
#include <JavaScriptCore/SourceTaintedOrigin.h>
#include <JavaScriptCore/SparseArrayValueMap.h>
#include <JavaScriptCore/StackFrame.h>
#include <JavaScriptCore/StackVisitor.h>
#include <JavaScriptCore/Structure.h>
#include <JavaScriptCore/StructureCache.h>
#include <JavaScriptCore/StructureRareData.h>
#include <JavaScriptCore/StructureSet.h>
#include <JavaScriptCore/StructureTransitionTable.h>
#include <JavaScriptCore/SuperSampler.h>
#include <JavaScriptCore/SymbolTable.h>
#include <JavaScriptCore/ThrowScope.h>
#include <JavaScriptCore/TypeInfoBlob.h>
#include <JavaScriptCore/TypeLocation.h>
#include <JavaScriptCore/TypeSet.h>
#include <JavaScriptCore/TypedArrayAdaptersForwardDeclarations.h>
#include <JavaScriptCore/TypedArrayType.h>
#include <JavaScriptCore/VarOffset.h>
#include <JavaScriptCore/VariableEnvironment.h>
#include <JavaScriptCore/VariableWriteFireDetail.h>
#include <JavaScriptCore/VirtualRegister.h>
#include <JavaScriptCore/VisitRaceKey.h>
#include <JavaScriptCore/WasmIndexOrName.h>
#include <JavaScriptCore/WasmName.h>
#include <JavaScriptCore/WasmNameSection.h>
#include <JavaScriptCore/WeakGCSet.h>
#include <JavaScriptCore/WeakInlines.h>
#include <JavaScriptCore/WeakInlinesLight.h>
#include <JavaScriptCore/WeakSetInlines.h>
#include <JavaScriptCore/WebKitAvailability.h>
#include <JavaScriptCore/YarrFlags.h>
#include <WebCore/AXTextStateChangeIntent.h>
#include <WebCore/AddEventListenerOptions.h>
#include <WebCore/AdvancedPrivacyProtections.h>
#include <WebCore/AffineTransform.h>
#include <WebCore/ApplicationManifest.h>
#include <WebCore/AttributionSecondsUntilSendData.h>
#include <WebCore/AttributionTimeToSendData.h>
#include <WebCore/AttributionTriggerData.h>
#include <WebCore/BackForwardFrameItemIdentifier.h>
#include <WebCore/BackForwardItemIdentifier.h>
#include <WebCore/BlobData.h>
#include <WebCore/BlobDataFileReference.h>
#include <WebCore/BlobURL.h>
#include <WebCore/BoxExtents.h>
#include <WebCore/BoxSides.h>
#if PLATFORM(COCOA)
#include <WebCore/CVPixelBufferUtilities.h>
#endif
#include <WebCore/CacheValidation.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/Color.h>
#include <WebCore/ColorComponents.h>
#include <WebCore/ColorConversion.h>
#include <WebCore/ColorMatrix.h>
#include <WebCore/ColorModels.h>
#include <WebCore/ColorSpace.h>
#include <WebCore/ColorTransferFunctions.h>
#include <WebCore/ColorTypes.h>
#include <WebCore/ColorUtilities.h>
#include <WebCore/CompositeOperation.h>
#include <WebCore/ContainerNode.h>
#include <WebCore/ContentFilterUnblockHandler.h>
#include <WebCore/ContentSecurityPolicyResponseHeaders.h>
#include <WebCore/ContentsFormat.h>
#include <WebCore/Cookie.h>
#include <WebCore/CopyImageOptions.h>
#include <WebCore/CornerRadii.h>
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/CrossOriginEmbedderPolicy.h>
#include <WebCore/CrossOriginEmbedderPolicyValue.h>
#include <WebCore/CrossOriginOpenerPolicy.h>
#include <WebCore/DOMPasteAccess.h>
#include <WebCore/DecodingOptions.h>
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/DocumentSecurityPolicy.h>
#include <WebCore/DoublePoint.h>
#include <WebCore/DoubleSize.h>
#include <WebCore/Element.h>
#include <WebCore/EphemeralNonce.h>
#include <WebCore/EventListener.h>
#include <WebCore/EventListenerMap.h>
#include <WebCore/EventListenerOptions.h>
#include <WebCore/EventOptions.h>
#include <WebCore/EventTarget.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/ExceptionData.h>
#include <WebCore/FetchIdentifier.h>
#include <WebCore/FetchOptions.h>
#include <WebCore/FetchOptionsCache.h>
#include <WebCore/FetchOptionsCredentials.h>
#include <WebCore/FetchOptionsDestination.h>
#include <WebCore/FetchOptionsMode.h>
#include <WebCore/FetchOptionsRedirect.h>
#include <WebCore/FetchingWorkerIdentifier.h>
#include <WebCore/FloatConversion.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/FloatPoint3D.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <WebCore/FocusDirection.h>
#include <WebCore/FocusOptions.h>
#include <WebCore/FontPlatformData.h>
#include <WebCore/FontTaggedSettings.h>
#include <WebCore/FormData.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/FrameTreeSyncClient.h>
#include <WebCore/GainMap.h>
#include <WebCore/GlobalFrameIdentifier.h>
#include <WebCore/GraphicsTypes.h>
#include <WebCore/HTTPHeaderMap.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/HitTestRequest.h>
#include <WebCore/HitTestSource.h>
#include <WebCore/IPAddressSpace.h>
#include <WebCore/Image.h>
#include <WebCore/ImageAdapter.h>
#include <WebCore/ImageOrientation.h>
#include <WebCore/ImagePaintingOptions.h>
#include <WebCore/ImageTypes.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/LayoutMilestone.h>
#include <WebCore/LayoutPoint.h>
#include <WebCore/LayoutRange.h>
#include <WebCore/LayoutRect.h>
#include <WebCore/LayoutRoundedRect.h>
#include <WebCore/LayoutSize.h>
#include <WebCore/LayoutUnit.h>
#include <WebCore/LinkIcon.h>
#include <WebCore/LinkIconType.h>
#include <WebCore/LoadedFromOpaqueSource.h>
#include <WebCore/LoaderMalloc.h>
#include <WebCore/LocalFrameLoaderClient.h>
#include <WebCore/NativeImage.h>
#include <WebCore/NavigationAction.h>
#include <WebCore/NavigationIdentifier.h>
#include <WebCore/NavigationRequester.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/Node.h>
#include <WebCore/NodeIdentifier.h>
#include <WebCore/NodeType.h>
#include <WebCore/PCMSites.h>
#include <WebCore/PCMTokens.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ParsedContentRange.h>
#include <WebCore/PixelFormat.h>
#include <WebCore/PlatformColorSpace.h>
#include <WebCore/PlatformDynamicRangeLimit.h>
#include <WebCore/PlatformExportMacros.h>
#include <WebCore/PlatformImage.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/PlatformLayerIdentifier.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/PolicyContainer.h>
#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/ProcessQualified.h>
#include <WebCore/PublicSuffix.h>
#include <WebCore/PublicSuffixStore.h>
#include <WebCore/QualifiedName.h>
#include <WebCore/Quaternion.h>
#include <WebCore/RectEdges.h>
#include <WebCore/ReferrerPolicy.h>
#include <WebCore/RegisteredEventListener.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/RemoteFrameLayoutInfo.h>
#include <WebCore/RenderPtr.h>
#include <WebCore/RenderingResource.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/RequestPriority.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceErrorBase.h>
#include <WebCore/ResourceLoadPriority.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceRequestBase.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/ResourceResponseBase.h>
#include <WebCore/SandboxFlags.h>
#include <WebCore/ScreenOrientationLockType.h>
#include <WebCore/ScreenProperties.h>
#include <WebCore/ScriptBuffer.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/ScriptWrappable.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ScrollbarMode.h>
#include <WebCore/ScrollingNodeID.h>
#include <WebCore/SecurityContext.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SelectionRestorationMode.h>
#include <WebCore/ServiceWorkerIdentifier.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <WebCore/ShareableBitmap.h>
#if PLATFORM(COCOA)
#include <WebCore/ShareableCVPixelBuffer.h>
#include <WebCore/ShareableCVPixelFormat.h>
#include <WebCore/ShareableGainMap.h>
#endif
#include <WebCore/SharedBuffer.h>
#include <WebCore/SharedMemory.h>
#include <WebCore/SharedWorkerIdentifier.h>
#include <WebCore/ShouldLocalizeAxisNames.h>
#include <WebCore/ShouldTreatAsContinuingLoad.h>
#include <WebCore/SimulatedClickOptions.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <WebCore/StyleValidity.h>
#include <WebCore/SubstituteData.h>
#include <WebCore/TextFlags.h>
#include <WebCore/ThreadTimers.h>
#include <WebCore/TransformationMatrix.h>
#include <WebCore/UserGestureIndicator.h>
#include <WebCore/WebCoreOpaqueRoot.h>
#include <WebCore/WindRule.h>
#include <WebCore/WritingMode.h>
#include <pal/SessionID.h>
#if PLATFORM(COCOA)
#include <pal/spi/cf/CoreTextSPI.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <pal/spi/cocoa/IOKitSPI.h>
#include <pal/spi/mac/IOKitSPIMac.h>
#endif
#include <wtf/AbstractCanMakeCheckedPtr.h>
#include <wtf/AbstractRefCounted.h>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/AbstractThreadSafeRefCountedAndCanMakeWeakPtr.h>
#include <wtf/ArgumentCoder.h>
#include <wtf/BitVector.h>
#include <wtf/CompactRefPtr.h>
#include <wtf/CompactUniquePtrTuple.h>
#include <wtf/CompletionHandler.h>
#include <wtf/EmbeddedFixedVector.h>
#include <wtf/EnumClassOperatorOverloads.h>
#include <wtf/EnumSet.h>
#include <wtf/FileLockMode.h>
#include <wtf/FileSystem.h>
#include <wtf/FixedVector.h>
#include <wtf/Identified.h>
#include <wtf/Indenter.h>
#include <wtf/InlineMap.h>
#include <wtf/InlineWeakPtr.h>
#include <wtf/InlineWeakRef.h>
#include <wtf/JSONValues.h>
#include <wtf/ListHashSet.h>
#include <wtf/MachSendRight.h>
#include <wtf/MappedFileData.h>
#include <wtf/MediaTime.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/MmapSpan.h>
#include <wtf/NakedPtr.h>
#include <wtf/Observer.h>
#include <wtf/OrderedHashSet.h>
#include <wtf/OrderedHashTable.h>
#include <wtf/PackedRefPtr.h>
#include <wtf/ProcessID.h>
#include <wtf/RefCountedFixedVector.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/RunLoop.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/SequesteredMalloc.h>
#include <wtf/SetForScope.h>
#include <wtf/SixCharacterHash.h>
#include <wtf/StackTrace.h>
#include <wtf/StdMap.h>
#include <wtf/StringHashDumpContext.h>
#include <wtf/StringPrintStream.h>
#include <wtf/SystemFree.h>
#include <wtf/SystemTracing.h>
#include <wtf/TinyPtrSet.h>
#include <wtf/URLHash.h>
#include <wtf/WeakHashSet.h>
#include <wtf/ZippedRange.h>
#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#include <wtf/darwin/XPCObjectPtr.h>
#endif
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
#if PLATFORM(COCOA)
#include <wtf/spi/cocoa/IOSurfaceSPI.h>
#include <wtf/spi/cocoa/SecuritySPI.h>
#include <wtf/spi/cocoa/objcSPI.h>
#include <wtf/spi/darwin/XPCSPI.h>
#endif
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/Base64.h>
#include <wtf/text/OrdinalNumber.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/SymbolImpl.h>
#include <wtf/text/SymbolRegistry.h>
#include <wtf/text/TextPosition.h>
#include <wtf/text/TextStream.h>

#include "APIObject.h"
#include "ArgumentCoders.h"
#include "ArrayReferenceTuple.h"
#include "Attachment.h"
#if PLATFORM(COCOA)
#include "ClassStructPtr.h"
#endif
#include "Connection.h"
#include "ConnectionHandle.h"
#include "Decoder.h"
#include "Encoder.h"
#include "GeneratedSerializers.h"
#include "IPCSemaphore.h"
#if PLATFORM(COCOA)
#include "ImportanceAssertion.h"
#endif
#include "MessageNames.h"
#include "MessageObserver.h"
#include "MessageReceiveQueue.h"
#include "MessageReceiveQueueMap.h"
#include "MessageReceiver.h"
#include "MessageReceiverMap.h"
#include "MessageSender.h"
#include "MonotonicObjectIdentifier.h"
#include "ReceiverMatcher.h"
#include "SandboxExtension.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include "SharedPreferencesForWebProcess.h"
#include "StreamConnectionBuffer.h"
#include "StreamConnectionEncoder.h"
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#include "StreamServerConnectionBuffer.h"
#include "SyncRequestID.h"
#include "Timeout.h"
#include "TransferString.h"
#include "WKBase.h"
#if PLATFORM(MAC)
#include "WKBaseMac.h"
#endif
#include "WKDeclarationSpecifiers.h"
#if PLATFORM(COCOA)
#include "WKFoundation.h"
#endif
#include "WebKitLogDefinitions.h"
#include "WebPageProxyIdentifier.h"
#include "WebPushDaemonConnectionConfiguration.h"
#endif // defined(BUILDING_WITH_CMAKE)

#endif

#ifdef __OBJC__
#undef try
#undef catch
#endif

#ifdef __cplusplus
#define new ("if you use new/delete make sure to include config.h at the top of the file"())
#define delete ("if you use new/delete make sure to include config.h at the top of the file"())
#endif

#if USE(OS_LOG)
#include <os/log.h>
#endif
