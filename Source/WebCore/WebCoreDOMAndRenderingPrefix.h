/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebCorePrefix.h"

#ifdef __cplusplus
#undef new
#undef delete

#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/Strong.h>
#include <JavaScriptCore/VM.h>

#include <pal/SessionID.h>
#include <pal/text/TextEncoding.h>
#include <vector>
#include <wtf/BitVector.h>
#include <wtf/JSONValues.h>
#include <wtf/SegmentedVector.h>
#include <wtf/WeakHashCountedSet.h>
#include <wtf/WeakListHashSet.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/text/AdaptiveStringSearcher.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>
#if PLATFORM(COCOA)
#include <wtf/spi/cocoa/IOSurfaceSPI.h>
#endif

#include "AffineTransform.h"
#include "CSSNumericValue.h"
#include "CSSPropertyNames.h"
#include "CSSValue.h"
#include "CSSValueKeywords.h"
#include "CachedResource.h"
#include "Document.h"
#include "Event.h"
#include "HTMLElement.h"
#include "HTMLElementTypeHelpers.h"
#include "HTMLNames.h"
#include "Image.h"
#include "LocalFrame.h"
#include "NodeInlines.h"
#include "Path.h"
#include "Region.h"
#include "RenderBox.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SimpleRange.h"
#include "TextFlags.h"
#include "WebAnimationTime.h"

#include "AcceleratedTimeline.h"
#include "ApplicationManifest.h"
#include "ControlFactory.h"
#include "ControlPart.h"
#include "ControlStyle.h"
#include "PlatformControl.h"
#include "ProgressResolutionData.h"
#include "StyleAppearance.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "Styleable.h"
#include "WebAnimationTypes.h"

#if PLATFORM(MAC)
#include <libkern/OSCacheControl.h>
#endif

#include <JavaScriptCore/ArgList.h>
#include <JavaScriptCore/BytecodeConventions.h>
#include <JavaScriptCore/CellContainerInlines.h>
#include <JavaScriptCore/CodeSpecializationKind.h>
#include <JavaScriptCore/CommonIdentifiers.h>
#include <JavaScriptCore/ConstantMode.h>
#include <JavaScriptCore/ConstructAbility.h>
#include <JavaScriptCore/DeferredWorkTimer.h>
#include <JavaScriptCore/DirectArgumentsOffset.h>
#include <JavaScriptCore/DumpContext.h>
#include <JavaScriptCore/ECMAMode.h>
#include <JavaScriptCore/EmbedderArrayLike.h>
#include <JavaScriptCore/GCDeferralContext.h>
#include <JavaScriptCore/GetPutInfo.h>
#include <JavaScriptCore/HeapCellInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/InferredValue.h>
#include <JavaScriptCore/JSBigInt.h>
#include <JavaScriptCore/JSEmbedderArrayLike.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSGlobalProxy.h>
#include <JavaScriptCore/JSScope.h>
#include <JavaScriptCore/JSSegmentedVariableObject.h>
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/JSSymbolTableObject.h>
#include <JavaScriptCore/LazyClassStructure.h>
#include <JavaScriptCore/LazyProperty.h>
#include <JavaScriptCore/MarkedBlockInlines.h>
#include <JavaScriptCore/MarkedVector.h>
#include <JavaScriptCore/OpcodeSize.h>
#include <JavaScriptCore/PrototypeKey.h>
#include <JavaScriptCore/RegExpCachedResult.h>
#include <JavaScriptCore/RegExpGlobalData.h>
#include <JavaScriptCore/RegExpSubstringGlobalAtomCache.h>
#include <JavaScriptCore/ScopedArgumentsTable.h>
#include <JavaScriptCore/Scribble.h>
#include <JavaScriptCore/StructureCache.h>
#include <JavaScriptCore/StructureSet.h>
#include <JavaScriptCore/SuperSampler.h>
#include <JavaScriptCore/SymbolTable.h>
#include <JavaScriptCore/TypeLocation.h>
#include <JavaScriptCore/TypeSet.h>
#include <JavaScriptCore/VarOffset.h>
#include <JavaScriptCore/VariableWriteFireDetail.h>
#include <JavaScriptCore/VirtualRegister.h>
#include <JavaScriptCore/WeakGCSet.h>
#include <JavaScriptCore/WeakInlines.h>
#include <JavaScriptCore/WeakSetInlines.h>
#include <wtf/MachSendRight.h>
#include <wtf/MediaTime.h>
#include <wtf/ReducedResolutionSeconds.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/SetForScope.h>
#include <wtf/SignedPtr.h>
#include <wtf/StringHashDumpContext.h>
#include <wtf/TinyPtrSet.h>
#include <wtf/text/Base64.h>
#include <wtf/text/OrdinalNumber.h>
#include <wtf/text/TextPosition.h>

#include "AcceleratedTimelinesUpdater.h"
#include "ActivityState.h"
#include "AnimationFrameRate.h"
#include "AnimationMalloc.h"
#if PLATFORM(COCOA)
#include "AttributedString.h"
#endif
#include "BackForwardFrameItemIdentifier.h"
#include "BrowsingContextGroupIdentifier.h"
#include "CSSToLengthConversionData.h"
#include "CaretAnimator.h"
#include "CharacterRange.h"
#include "CompositionUnderline.h"
#include "CopyImageOptions.h"
#include "DOMWindow.h"
#include "DashArray.h"
#include "DocumentPage.h"
#include "EditAction.h"
#include "EditingBehavior.h"
#include "EditingBoundary.h"
#include "EditingStyle.h"
#include "Editor.h"
#include "EditorInsertAction.h"
#include "EventTimingInteractionID.h"
#include "ExceptionDetails.h"
#include "FindOptions.h"
#include "FloatQuad.h"
#include "FloatSegment.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameSelection.h"
#include "GlobalWindowIdentifier.h"
#include "GraphicsContext.h"
#include "GraphicsContextState.h"
#include "GraphicsContextStateSaver.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "GraphicsTypesGL.h"
#include "HTMLFrameOwnerElement.h"
#if USE(CG)
#include "IOSurfacePool.h"
#include "IOSurfacePoolIdentifier.h"
#endif
#include "ImageBitmap.h"
#include "ImageBuffer.h"
#include "ImageBufferAllocator.h"
#include "ImageBufferBackend.h"
#include "ImageBufferBackendParameters.h"
#include "IntPointHash.h"
#include "IntRectHash.h"
#include "JSDOMConvertBase.h"
#include "JSDOMConvertResult.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMWrapper.h"
#include "KeyboardScroll.h"
#include "LayerHostingContextIdentifier.h"
#include "LayoutMilestone.h"
#include "LoadSchedulingMode.h"
#include "LocalDOMWindow.h"
#include "LocalFrameInlines.h"
#include "Page.h"
#include "Pagination.h"
#include "PasteboardWriterData.h"
#include "Pattern.h"
#include "PerformanceEventTimingCandidate.h"
#include "PixelBufferFormat.h"
#include "PlatformGraphicsContext.h"
#include "Position.h"
#include "ProcessIdentity.h"
#include "ProcessSwapDisposition.h"
#include "PushSubscriptionIdentifier.h"
#include "PushSubscriptionOwner.h"
#include "ScriptTrackingPrivacyCategory.h"
#include "ScrollAlignment.h"
#include "ScrollBehavior.h"
#include "ScrollView.h"
#include "ScrollableArea.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "SelectionType.h"
#include "SourceBrush.h"
#include "SourceBrushLogicalGradient.h"
#include "SourceImage.h"
#include "Text.h"
#include "TextAffinity.h"
#if PLATFORM(COCOA)
#include "TextAttachmentForSerialization.h"
#endif
#include "TextChecking.h"
#include "TextCheckingRequestIdentifier.h"
#include "TextEventInputType.h"
#include "TextGranularity.h"
#include "TextIteratorBehavior.h"
#include "UserInterfaceLayoutDirection.h"
#include "VisiblePosition.h"
#include "VisibleSelection.h"
#include "Widget.h"
#include "WindowOrWorkerGlobalScope.h"
#include "WritingDirection.h"


#define new ("if you use new/delete make sure to include config.h at the top of the file"())
#define delete ("if you use new/delete make sure to include config.h at the top of the file"())
#endif
