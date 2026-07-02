/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2013 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

/* This prefix file should contain only:
 *    1) files to precompile for faster builds
 *    2) in one case at least: OS-X-specific performance bug workarounds
 *    3) the special trick to catch us using new or delete without including "config.h"
 * The project should be able to build without this header, although we rarely test that.
 */

/* Things that need to be defined globally should go into "config.h". */

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif

#include <wtf/Platform.h>

#if defined(__APPLE__)
#ifdef __cplusplus
#include <cstddef>
#else
#include <stddef.h>
#endif
#endif

#if !OS(WINDOWS)
#include <pthread.h>
#endif // !OS(WINDOWS)

#include <sys/types.h>
#include <fcntl.h>
#if HAVE(REGEX_H)
#include <regex.h>
#endif

#include <setjmp.h>

#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(__APPLE__)
#include <unistd.h>
#endif

#ifdef __cplusplus
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <typeinfo>
#include <wtf/Variant.h>
#endif

#if defined(__APPLE__)
#include <sys/param.h>
#endif
#include <sys/stat.h>
#if defined(__APPLE__)
#include <sys/time.h>
#include <sys/resource.h>
#endif

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if OS(WINDOWS)
#ifndef CF_IMPLICIT_BRIDGING_ENABLED
#define CF_IMPLICIT_BRIDGING_ENABLED
#endif

#ifndef CF_IMPLICIT_BRIDGING_DISABLED
#define CF_IMPLICIT_BRIDGING_DISABLED
#endif

#if USE(CF)
#include <CoreFoundation/CFBase.h>
#endif

#ifndef CF_ENUM
#define CF_ENUM(_type, _name) _type _name; enum
#endif
#ifndef CF_OPTIONS
#define CF_OPTIONS(_type, _name) _type _name; enum
#endif
#ifndef CF_ENUM_DEPRECATED
#define CF_ENUM_DEPRECATED(_macIntro, _macDep, _iosIntro, _iosDep)
#endif
#ifndef CF_ENUM_AVAILABLE
#define CF_ENUM_AVAILABLE(_mac, _ios)
#endif
#endif

#if PLATFORM(WIN)
#include <windows.h>
#else

#if OS(WINDOWS)
#include <windows.h>
#endif // OS(WINDOWS)

#if USE(OS_LOG)
#include <os/log.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include <MobileCoreServices/MobileCoreServices.h>
#endif

#if PLATFORM(MAC)
#if !USE(APPLE_INTERNAL_SDK)
/* SecTrustedApplication.h declares SecTrustedApplicationCreateFromPath(...) to
 * be unavailable on macOS, so do not include that header. */
#define _SECURITY_SECTRUSTEDAPPLICATION_H_
#endif
#include <CoreServices/CoreServices.h>
#endif

#endif

#ifdef __OBJC__
#if PLATFORM(IOS_FAMILY)
#import <Foundation/Foundation.h>
#else
#if USE(APPKIT)
#import <Cocoa/Cocoa.h>
#endif
#endif // PLATFORM(IOS_FAMILY)
#endif

#ifdef __cplusplus

#if !PLATFORM(WIN)

#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/CPU.h>
#include <JavaScriptCore/Forward.h>
#include <JavaScriptCore/JSCConfig.h>
#include <JavaScriptCore/OptionsList.h>
#include <JavaScriptCore/SourceID.h>
#include <JavaScriptCore/Weak.h>
#include <JavaScriptCore/WeakInlinesLight.h>
#include <set>
#include <unicode/uscript.h>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/ArgumentCoder.h>
#include <wtf/Box.h>
#include <wtf/CheckedPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/DataLog.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/FastMalloc.h>
#include <wtf/FileSystem.h>
#include <wtf/FixedVector.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/Logger.h>
#include <wtf/MappedFileData.h>
#include <wtf/NumberOfCores.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ThreadSafeWeakHashSet.h>
#include <wtf/TriState.h>
#include <wtf/URLHash.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <wtf/cf/TypeCastsCF.h>
#endif

#if PLATFORM(COCOA)
#include <objc/runtime.h>
#include <wtf/OSObjectPtr.h>
#endif

#include "Color.h"
#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "FrameIdentifier.h"
#include "GraphicsTypes.h"
#include "HitTestRequest.h"
#include "ImageTypes.h"
#include "IntRect.h"
#include "LayoutRect.h"
#include "LayoutSize.h"
#include "NodeIdentifier.h"
#include "NodeType.h"
#include "PageIdentifier.h"
#include "ProcessQualified.h"
#include "PublicSuffixStore.h"
#include "QualifiedName.h"
#include "RenderPtr.h"
#include "RenderStyleConstants.h"
#include "RenderingResource.h"
#include "ScriptExecutionContext.h"
#include "ScriptExecutionContextIdentifier.h"
#include "ScriptWrappable.h"
#include "ScrollTypes.h"
#include "SecurityContext.h"
#include "SecurityOriginData.h"
#include "ServiceWorkerIdentifier.h"
#include "SharedBuffer.h"
#include "Timer.h"

#include <wtf/DateMath.h>
#endif

#if PLATFORM(MAC)
#include <AudioToolbox/AudioSession.h>
#include <IOKit/IOMessage.h>
#include <IOKit/hid/IOHIDBase.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDDeviceKeys.h>
#include <IOKit/hid/IOHIDDeviceTypes.h>
#include <IOKit/hid/IOHIDElement.h>
#include <IOKit/hid/IOHIDEventServiceKeys.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDLibObsolete.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDProperties.h>
#include <IOKit/hid/IOHIDQueue.h>
#include <IOKit/hid/IOHIDTransaction.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDValue.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <Security/Security.h>
#include <dlfcn.h>
#include <execinfo.h>
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
#endif

#include <JavaScriptCore/AbstractSlotVisitor.h>
#include <JavaScriptCore/AlignedMemoryAllocator.h>
#include <JavaScriptCore/AllocationFailureMode.h>
#include <JavaScriptCore/Allocator.h>
#include <JavaScriptCore/AllocatorForMode.h>
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/ArrayConventions.h>
#include <JavaScriptCore/ArrayStorage.h>
#include <JavaScriptCore/AuxiliaryBarrier.h>
#include <JavaScriptCore/BlockDirectory.h>
#include <JavaScriptCore/BlockDirectoryBits.h>
#include <JavaScriptCore/Butterfly.h>
#include <JavaScriptCore/BytecodeIndex.h>
#include <JavaScriptCore/CacheableIdentifier.h>
#include <JavaScriptCore/CagedBarrierPtr.h>
#include <JavaScriptCore/CallData.h>
#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/CalleeBits.h>
#include <JavaScriptCore/CellAttributes.h>
#include <JavaScriptCore/CellContainer.h>
#include <JavaScriptCore/CellState.h>
#include <JavaScriptCore/ClassInfo.h>
#include <JavaScriptCore/CollectionScope.h>
#include <JavaScriptCore/CollectorPhase.h>
#include <JavaScriptCore/CompleteSubspace.h>
#include <JavaScriptCore/Concurrency.h>
#include <JavaScriptCore/ConcurrentJSLock.h>
#include <JavaScriptCore/ConstructData.h>
#include <JavaScriptCore/CustomGetterSetter.h>
#include <JavaScriptCore/DFGDoesGCCheck.h>
#include <JavaScriptCore/DOMAnnotation.h>
#include <JavaScriptCore/DOMAttributeGetterSetter.h>
#include <JavaScriptCore/DateInstanceCache.h>
#include <JavaScriptCore/DeferGC.h>
#include <JavaScriptCore/DefinePropertyAttributes.h>
#include <JavaScriptCore/DeleteAllCodeEffort.h>
#include <JavaScriptCore/DeletePropertySlot.h>
#include <JavaScriptCore/DestructionMode.h>
#include <JavaScriptCore/DisallowScope.h>
#include <JavaScriptCore/DisallowVMEntry.h>
#include <JavaScriptCore/EnsureStillAliveHere.h>
#include <JavaScriptCore/EnumerationMode.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/ErrorInstance.h>
#include <JavaScriptCore/ErrorType.h>
#include <JavaScriptCore/ExceptionEventLocation.h>
#include <JavaScriptCore/ExceptionScope.h>
#include <JavaScriptCore/ExecutableMemoryHandle.h>
#include <JavaScriptCore/FreeList.h>
#include <JavaScriptCore/FunctionHasExecutedCache.h>
#include <JavaScriptCore/GCAssertions.h>
#include <JavaScriptCore/GCConductor.h>
#include <JavaScriptCore/GCIncomingRefCountedSet.h>
#include <JavaScriptCore/GCOwnedDataScope.h>
#include <JavaScriptCore/GCRequest.h>
#include <JavaScriptCore/GCSegmentedArray.h>
#include <JavaScriptCore/GenericOffset.h>
#include <JavaScriptCore/GetVM.h>
#include <JavaScriptCore/Handle.h>
#include <JavaScriptCore/HandleBlock.h>
#include <JavaScriptCore/HandleSet.h>
#include <JavaScriptCore/HandleTypes.h>
#include <JavaScriptCore/Heap.h>
#include <JavaScriptCore/HeapCell.h>
#include <JavaScriptCore/HeapCellType.h>
#include <JavaScriptCore/HeapFinalizerCallback.h>
#include <JavaScriptCore/HeapObserver.h>
#include <JavaScriptCore/Identifier.h>
#include <JavaScriptCore/ImplementationVisibility.h>
#include <JavaScriptCore/IndexingHeader.h>
#include <JavaScriptCore/IndexingHeaderInlines.h>
#include <JavaScriptCore/IndexingType.h>
#include <JavaScriptCore/Integrity.h>
#include <JavaScriptCore/Interpreter.h>
#include <JavaScriptCore/Intrinsic.h>
#include <JavaScriptCore/IsoCellSet.h>
#include <JavaScriptCore/IsoHeapCellType.h>
#include <JavaScriptCore/IsoInlinedHeapCellType.h>
#include <JavaScriptCore/IsoSubspace.h>
#include <JavaScriptCore/IterationKind.h>
#include <JavaScriptCore/JITOperationList.h>
#include <JavaScriptCore/JITOperationValidation.h>
#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSCJSValueCell.h>
#include <JavaScriptCore/JSCPtrTag.h>
#include <JavaScriptCore/JSCTimeZone.h>
#include <JavaScriptCore/JSCast.h>
#include <JavaScriptCore/JSCell.h>
#include <JavaScriptCore/JSDateMath.h>
#include <JavaScriptCore/JSDestructibleObject.h>
#include <JavaScriptCore/JSDestructibleObjectHeapCellType.h>
#include <JavaScriptCore/JSGlobalObjectFunctions.h>
#include <JavaScriptCore/JSHeapFinalizerPrivate.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSONAtomStringCache.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSRunLoopTimer.h>
#include <JavaScriptCore/JSType.h>
#include <JavaScriptCore/JSTypeInfo.h>
#include <JavaScriptCore/KeyAtomStringCache.h>
#include <JavaScriptCore/LineColumn.h>
#include <JavaScriptCore/LocalAllocator.h>
#include <JavaScriptCore/MacroAssemblerCodeRef.h>
#include <JavaScriptCore/MarkStack.h>
#include <JavaScriptCore/MarkedBlock.h>
#include <JavaScriptCore/MarkedBlockSet.h>
#include <JavaScriptCore/MarkedSpace.h>
#include <JavaScriptCore/MatchResult.h>
#include <JavaScriptCore/MathCommon.h>
#include <JavaScriptCore/Microtask.h>
#include <JavaScriptCore/MutatorState.h>
#include <JavaScriptCore/NativeFunction.h>
#include <JavaScriptCore/NumericStrings.h>
#include <JavaScriptCore/OperationResult.h>
#include <JavaScriptCore/Options.h>
#include <JavaScriptCore/PreciseAllocation.h>
#include <JavaScriptCore/PreciseSubspace.h>
#include <JavaScriptCore/PrivateName.h>
#include <JavaScriptCore/PropertyDescriptor.h>
#include <JavaScriptCore/PropertyName.h>
#include <JavaScriptCore/PropertyNameArray.h>
#include <JavaScriptCore/PropertyOffset.h>
#include <JavaScriptCore/PropertySlot.h>
#include <JavaScriptCore/PropertyStorage.h>
#include <JavaScriptCore/PutDirectIndexMode.h>
#include <JavaScriptCore/PutPropertySlot.h>
#include <JavaScriptCore/Register.h>
#include <JavaScriptCore/RootMarkReason.h>
#include <JavaScriptCore/RuntimeFlags.h>
#include <JavaScriptCore/RuntimeType.h>
#include <JavaScriptCore/ScopeOffset.h>
#include <JavaScriptCore/SlotVisitor.h>
#include <JavaScriptCore/SlotVisitorMacros.h>
#include <JavaScriptCore/SmallStrings.h>
#include <JavaScriptCore/SourceTaintedOrigin.h>
#include <JavaScriptCore/SparseArrayValueMap.h>
#include <JavaScriptCore/StackFrame.h>
#include <JavaScriptCore/StackManager.h>
#include <JavaScriptCore/StackVisitor.h>
#include <JavaScriptCore/StringReplaceCache.h>
#include <JavaScriptCore/StringSplitCache.h>
#include <JavaScriptCore/Strong.h>
#include <JavaScriptCore/Structure.h>
#include <JavaScriptCore/StructureID.h>
#include <JavaScriptCore/StructureRareData.h>
#include <JavaScriptCore/StructureTransitionTable.h>
#include <JavaScriptCore/Subspace.h>
#include <JavaScriptCore/SubspaceAccess.h>
#include <JavaScriptCore/Synchronousness.h>
#include <JavaScriptCore/ThrowScope.h>
#include <JavaScriptCore/ThunkGenerator.h>
#include <JavaScriptCore/TinyBloomFilter.h>
#include <JavaScriptCore/TypeInfoBlob.h>
#include <JavaScriptCore/TypedArrayType.h>
#include <JavaScriptCore/TypeofType.h>
#include <JavaScriptCore/UGPRPair.h>
#include <JavaScriptCore/VM.h>
#include <JavaScriptCore/VMThreadContext.h>
#include <JavaScriptCore/VMTraps.h>
#include <JavaScriptCore/VariableEnvironment.h>
#include <JavaScriptCore/VisitRaceKey.h>
#include <JavaScriptCore/WasmContext.h>
#include <JavaScriptCore/WasmIndexOrName.h>
#include <JavaScriptCore/WasmName.h>
#include <JavaScriptCore/WasmNameSection.h>
#include <JavaScriptCore/WeakBlock.h>
#include <JavaScriptCore/WeakGCHashTable.h>
#include <JavaScriptCore/WeakGCMap.h>
#include <JavaScriptCore/WeakHandleOwner.h>
#include <JavaScriptCore/WeakSet.h>
#include <JavaScriptCore/WriteBarrier.h>
#include <bmalloc/BVMTags.h>
#include <pal/SessionID.h>
#if PLATFORM(COCOA)
#include <pal/cf/OTSVGTable.h>
#include <pal/spi/cf/CoreTextSPI.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <pal/spi/cocoa/IOKitSPI.h>
#include <pal/spi/mac/IOKitSPIMac.h>
#endif
#include <pal/text/TextEncoding.h>
#include <pal/text/UnencodableHandling.h>
#include <wtf/AccessibleAddress.h>
#include <wtf/AutomaticThread.h>
#include <wtf/BitVector.h>
#include <wtf/BumpPointerAllocator.h>
#include <wtf/CancellableTask.h>
#include <wtf/CompactRefPtr.h>
#include <wtf/CompactUniquePtrTuple.h>
#include <wtf/CompactVariant.h>
#include <wtf/CompactVariantOperations.h>
#include <wtf/ConcurrentBuffer.h>
#include <wtf/ConcurrentPtrHashSet.h>
#include <wtf/ConcurrentVector.h>
#include <wtf/CountingLock.h>
#include <wtf/DataRef.h>
#include <wtf/EnumClassOperatorOverloads.h>
#include <wtf/FastBitVector.h>
#include <wtf/FastTLS.h>
#include <wtf/FlatteningVariantAdaptor.h>
#include <wtf/GenericHashKey.h>
#include <wtf/GregorianDateTime.h>
#include <wtf/Indenter.h>
#include <wtf/InlineMap.h>
#include <wtf/LazyRef.h>
#include <wtf/LazyUniqueRef.h>
#include <wtf/MallocPtr.h>
#include <wtf/MetaAllocatorHandle.h>
#include <wtf/NakedPtr.h>
#include <wtf/OSAllocator.h>
#include <wtf/PackedRefPtr.h>
#include <wtf/PageAllocation.h>
#include <wtf/ParallelHelperPool.h>
#include <wtf/PointerComparison.h>
#include <wtf/ProcessID.h>
#include <wtf/RawValueTraits.h>
#include <wtf/RedBlackTree.h>
#include <wtf/RefCountedFixedVector.h>
#include <wtf/RefCountedWithInlineWeakPtr.h>
#include <wtf/RefTrackerMixin.h>
#include <wtf/SegmentedVector.h>
#include <wtf/SinglyLinkedList.h>
#include <wtf/SinglyLinkedListWithTail.h>
#include <wtf/SixCharacterHash.h>
#include <wtf/StackAllocation.h>
#include <wtf/StackBounds.h>
#include <wtf/StackPointer.h>
#include <wtf/StackShot.h>
#include <wtf/StackStats.h>
#include <wtf/StackTrace.h>
#include <wtf/StringPrintStream.h>
#include <wtf/SystemFree.h>
#include <wtf/ThreadSafeRefCountedWithSuppressingSaferCPPChecking.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/Threading.h>
#include <wtf/UniqueArray.h>
#include <wtf/UniquelyOwned.h>
#include <wtf/UniquelyOwnedPtr.h>
#include <wtf/VMTags.h>
#include <wtf/VariantExtras.h>
#include <wtf/WeakHashCountedSet.h>
#include <wtf/WeakListHashSet.h>
#include <wtf/WeakRandom.h>
#include <wtf/ZippedRange.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
#if PLATFORM(COCOA)
#include <wtf/spi/cocoa/IOSurfaceSPI.h>
#include <wtf/spi/cocoa/SecuritySPI.h>
#endif
#include <wtf/text/AdaptiveStringSearcher.h>
#include <wtf/text/AtomStringTable.h>
#include <wtf/text/CharacterProperties.h>
#include <wtf/text/SymbolImpl.h>
#include <wtf/text/SymbolRegistry.h>

#if !PLATFORM(WIN)
#include "AXTextStateChangeIntent.h"
#include "AcceleratedTimeline.h"
#include "ActiveDOMObject.h"
#include "AffineTransform.h"
#include "AlphaPremultiplication.h"
#include "ApplicationManifest.h"
#include "AsyncNodeDeletionQueue.h"
#include "Attribute.h"
#include "BlobData.h"
#include "BlobDataFileReference.h"
#include "BlobURL.h"
#include "BoundaryPoint.h"
#include "BufferSource.h"
#include "CSSCalcSymbolTable.h"
#include "CSSColorType.h"
#include "CSSGridAutoFlow.h"
#include "CSSKeyword.h"
#include "CSSKeywordList.h"
#include "CSSNamespacePrefixMap.h"
#include "CSSNoConversionDataRequiredToken.h"
#include "CSSNumericBaseType.h"
#include "CSSNumericType.h"
#include "CSSNumericValue.h"
#include "CSSParserContext.h"
#include "CSSParserMode.h"
#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveData.h"
#include "CSSPrimitiveNumeric.h"
#include "CSSPrimitiveNumericCategory.h"
#include "CSSPrimitiveNumericConcepts.h"
#include "CSSPrimitiveNumericOrKeyword.h"
#include "CSSPrimitiveNumericRange.h"
#include "CSSPrimitiveNumericRaw.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPrimitiveNumericUnits.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSRatio.h"
#include "CSSStyleValue.h"
#include "CSSTokenizer.h"
#include "CSSTokenizerInputStream.h"
#include "CSSUnevaluatedCalc.h"
#include "CSSUnits.h"
#include "CSSValue.h"
#include "CSSValueAggregates.h"
#include "CSSValueConcepts.h"
#include "CSSValueKeywords.h"
#include "CSSValueTypes.h"
#include "CSSVariableData.h"
#include "CacheValidation.h"
#include "CachedImageClient.h"
#include "CachedResource.h"
#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "CertificateInfo.h"
#include "CharacterData.h"
#include "ClipboardAccessPolicy.h"
#include "ColorInterpolationMethod.h"
#include "CommonAtomStrings.h"
#include "CompositeOperation.h"
#include "ComputedStyleDependencies.h"
#include "ContainerNode.h"
#include "ContentSecurityPolicyResponseHeaders.h"
#include "ContentType.h"
#include "ContentsFormat.h"
#include "ControlFactory.h"
#include "ControlPart.h"
#include "ControlStyle.h"
#include "CornerRadii.h"
#include "CrossOriginAccessControl.h"
#include "DOMHighResTimeStamp.h"
#include "DOMPasteAccess.h"
#if PLATFORM(COCOA)
#include "DataDetectorType.h"
#endif
#include "DecodingOptions.h"
#include "DestinationColorSpace.h"
#include "Document.h"
#include "DocumentClasses.h"
#include "DocumentEnums.h"
#include "DocumentEventTiming.h"
#include "DocumentFragment.h"
#include "DocumentSecurityPolicy.h"
#include "EditableLinkBehavior.h"
#include "EditingBehaviorType.h"
#include "Element.h"
#include "ElementData.h"
#include "Event.h"
#include "EventInit.h"
#include "EventInterfaces.h"
#include "EventLoop.h"
#include "EventOptions.h"
#include "EventTargetInlines.h"
#include "EventTargetInterfaces.h"
#include "FetchIdentifier.h"
#include "FetchOptions.h"
#include "FetchOptionsCache.h"
#include "FetchOptionsCredentials.h"
#include "FetchOptionsDestination.h"
#include "FetchOptionsMode.h"
#include "FetchOptionsRedirect.h"
#include "FetchingWorkerIdentifier.h"
#include "FloatPoint3D.h"
#include "FloatRoundedRect.h"
#include "FocusControllerTypes.h"
#include "FocusDirection.h"
#include "FocusOptions.h"
#include "Font.h"
#include "FontBaseline.h"
#include "FontCascade.h"
#include "FontCascadeDescription.h"
#include "FontCascadeEnums.h"
#include "FontDescription.h"
#if PLATFORM(COCOA)
#include "FontFamilySpecificationCoreText.h"
#endif
#include "FontGenericFamilies.h"
#include "FontLoadTimingOverride.h"
#include "FontMetrics.h"
#include "FontPalette.h"
#include "FontPlatformData.h"
#include "FontSelectionAlgorithm.h"
#include "FontSelectorClient.h"
#include "FontSizeAdjust.h"
#include "FontTaggedSettings.h"
#include "ForcedAccessibilityValue.h"
#include "FormData.h"
#include "FourCC.h"
#include "Frame.h"
#include "FrameDestructionObserver.h"
#include "FrameLoaderTypes.h"
#include "FrameTree.h"
#include "FrameTreeSyncData.h"
#include "GCReachableRef.h"
#include "GainMap.h"
#include "Glyph.h"
#include "GlyphBuffer.h"
#include "GlyphBufferMembers.h"
#include "GlyphMetricsMap.h"
#include "Gradient.h"
#include "GradientColorStop.h"
#include "GradientColorStops.h"
#if USE(CG)
#include "GradientRendererCG.h"
#endif
#include "GraphicsStyle.h"
#include "HTMLElement.h"
#include "HTMLElementTypeHelpers.h"
#include "HTMLNames.h"
#include "HTMLParserScriptingFlagPolicy.h"
#include "HTTPHeaderMap.h"
#include "HTTPHeaderNames.h"
#include "IDLTypes.h"
#include "Image.h"
#include "ImageAdapter.h"
#include "ImageBufferFormat.h"
#include "ImageOrientation.h"
#include "ImagePaintingOptions.h"
#include "InspectorInstrumentationPublic.h"
#include "IntSizeHash.h"
#include "IsImportant.h"
#include "JSDOMPromiseDeferredForward.h"
#include "LayoutRoundedRect.h"
#include "LayoutShape.h"
#include "LoadedFromOpaqueSource.h"
#include "LoaderMalloc.h"
#include "LocalFrame.h"
#include "MediaPlayerEnums.h"
#include "MediaSessionGroupIdentifier.h"
#include "MutationObserverOptions.h"
#include "NativeImage.h"
#include "NetworkLoadMetrics.h"
#include "Node.h"
#include "NodeDocument.h"
#include "NodeInlines.h"
#include "OriginKeyed.h"
#include "PaintPhase.h"
#include "ParsedContentRange.h"
#include "ParserContentPolicy.h"
#include "Path.h"
#include "PathElement.h"
#include "PathImpl.h"
#include "PathSegment.h"
#include "PathSegmentData.h"
#include "PixelFormat.h"
#include "PlatformColorSpace.h"
#include "PlatformControl.h"
#include "PlatformDynamicRangeLimit.h"
#include "PlatformImage.h"
#include "PlatformLayer.h"
#include "PlatformLayerIdentifier.h"
#include "PlatformPath.h"
#include "PlatformScreen.h"
#include "PlaybackTargetClientContextIdentifier.h"
#include "PolicyContainer.h"
#include "ProgressResolutionData.h"
#include "PseudoElement.h"
#include "PseudoElementIdentifier.h"
#include "Quaternion.h"
#include "RectCorners.h"
#include "Region.h"
#include "RegistrableDomain.h"
#include "RemoteFrameLayoutInfo.h"
#include "RenderBox.h"
#include "RenderBoxModelObject.h"
#include "RenderElement.h"
#include "RenderLayerModelObject.h"
#include "RenderObject.h"
#include "RenderObjectEnums.h"
#include "RenderOverflow.h"
#include "RenderingMode.h"
#include "RepaintRectCalculation.h"
#include "ReportingClient.h"
#include "RequestPriority.h"
#include "ResourceCryptographicDigest.h"
#include "ResourceError.h"
#include "ResourceErrorBase.h"
#include "ResourceLoadPriority.h"
#include "ResourceLoaderIdentifier.h"
#include "ResourceLoaderOptions.h"
#include "ResourceRequest.h"
#include "ResourceRequestBase.h"
#include "ResourceResponse.h"
#include "ResourceResponseBase.h"
#include "RotationDirection.h"
#include "SVGParserUtilities.h"
#include "SVGPathByteStream.h"
#include "SVGPathConsumer.h"
#include "SVGPathUtilities.h"
#include "SVGPropertyTraits.h"
#include "ScreenOrientationLockType.h"
#include "ScreenProperties.h"
#include "ScriptBuffer.h"
#include "ScrollSnapOffsetsInfo.h"
#include "SecurityOrigin.h"
#include "SelectionRestorationMode.h"
#include "ServiceWorkerTypes.h"
#include "Settings.h"
#include "SettingsBase.h"
#include "ShadowRoot.h"
#include "ShadowRootMode.h"
#include "ShapeOutsideInfo.h"
#include "SharedWorkerIdentifier.h"
#include "ShouldLocalizeAxisNames.h"
#include "SimpleRange.h"
#include "SimulatedClickOptions.h"
#include "SlotAssignmentMode.h"
#include "SpaceSplitString.h"
#include "StorageBlockingPolicy.h"
#include "StoredCredentialsPolicy.h"
#include "StringAdaptors.h"
#include "StringWithDirection.h"
#include "StyleAppearance.h"
#include "StyleComputedStyle.h"
#include "StyleComputedStyleBase.h"
#include "StyleComputedStyleProperties.h"
#include "StyleDifference.h"
#include "StyleGridAutoFlow.h"
#include "StylePrimitiveNumeric+Forward.h"
#include "StylePrimitiveNumeric.h"
#include "StylePrimitiveNumericConcepts.h"
#include "StylePrimitiveNumericOrKeyword.h"
#include "StylePrimitiveNumericTypes.h"
#include "StyleRuleType.h"
#include "StyleScopeOrdinal.h"
#include "StyleShapeForward.h"
#include "StyleUnevaluatedCalculation.h"
#include "StyleValidity.h"
#include "StyleValueTypes.h"
#include "StyleZoomPrimitives.h"
#include "Styleable.h"
#include "StyledElement.h"
#include "Supplementable.h"
#include "TaskSource.h"
#include "TextDirectionSubmenuInclusionBehavior.h"
#include "TextFlags.h"
#include "TextSpacing.h"
#include "TimelineIdentifier.h"
#include "TransformationMatrix.h"
#include "TreeScope.h"
#include "TreeScopeInlines.h"
#include "TreeScopeOrderedMap.h"
#include "TrustedFonts.h"
#include "TrustedHTML.h"
#include "URLKeepingBlobAlive.h"
#include "UserActionElementSet.h"
#include "UserInterfaceDirectionPolicy.h"
#include "ViewportArguments.h"
#include "WebAnimationTime.h"
#include "WebAnimationTypes.h"
#include "WebCoreOpaqueRoot.h"
#include "WebKitFontFamilyNames.h"
#endif // !PLATFORM(WIN)


#define new ("if you use new/delete make sure to include config.h at the top of the file"()) 
#define delete ("if you use new/delete make sure to include config.h at the top of the file"()) 
#endif

/* When C++ exceptions are disabled, the C++ library defines |try| and |catch|
 * to allow C++ code that expects exceptions to build. These definitions
 * interfere with Objective-C++ uses of Objective-C exception handlers, which
 * use |@try| and |@catch|. As a workaround, undefine these macros. */
#ifdef __OBJC__
#undef try
#undef catch
#endif

