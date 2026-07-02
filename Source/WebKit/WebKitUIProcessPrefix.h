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

#include "WebKitPrefix.h"

#ifdef __cplusplus
#undef new
#undef delete

#include <WebCore/PlatformExportMacros.h>
#include <pal/ExportMacros.h>

#include "APIPageConfiguration.h"
#include "ArgumentCoders.h"
#include "GeneratedSerializers.h"
#include "NetworkProcessProxy.h"
#include "PageClient.h"
#include "WebPageProxy.h"
#include "WebPreferencesDefinitions.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"

#include <WebCore/DocumentLoader.h>
#include <WebCore/Element.h>
#include <WebCore/NetworkStorageSession.h>

#include <WebCore/Document.h>
#include <WebCore/RenderObject.h>
#include <WebCore/Settings.h>
#include "WKPageUIClient.h"
#include "WebProcessMessages.h"

#if PLATFORM(COCOA)
#include <pal/spi/cf/CFNetworkSPI.h>
#include <simd/simd.h>
#endif

#include <sqlite3.h>

#include <JavaScriptCore/Strong.h>
#include <WebCore/Allowlist.h>
#include <WebCore/BidiContext.h>
#include <WebCore/BoundaryPoint.h>
#include <WebCore/CSSCalcSymbolTable.h>
#include <WebCore/CSSGridAutoFlow.h>
#include <WebCore/CSSKeyword.h>
#include <WebCore/CSSKeywordList.h>
#include <WebCore/CSSNoConversionDataRequiredToken.h>
#include <WebCore/CSSPrimitiveData.h>
#include <WebCore/CSSPrimitiveNumeric.h>
#include <WebCore/CSSPrimitiveNumericCategory.h>
#include <WebCore/CSSPrimitiveNumericConcepts.h>
#include <WebCore/CSSPrimitiveNumericRange.h>
#include <WebCore/CSSPrimitiveNumericRaw.h>
#include <WebCore/CSSPrimitiveNumericUnits.h>
#include <WebCore/CSSUnevaluatedCalc.h>
#include <WebCore/CSSUnits.h>
#include <WebCore/CSSValue.h>
#include <WebCore/CSSValueAggregates.h>
#include <WebCore/CSSValueConcepts.h>
#include <WebCore/CSSValueKeywords.h>
#include <WebCore/CSSValueTypes.h>
#include <WebCore/CachedImage.h>
#include <WebCore/CharacterData.h>
#include <WebCore/ComputedStyleDependencies.h>
#include <WebCore/EditingBoundary.h>
#include <WebCore/ElementContext.h>
#include <WebCore/FindOptions.h>
#include <WebCore/FloatRoundedRect.h>
#include <WebCore/Font.h>
#include <WebCore/FontAttributes.h>
#include <WebCore/FontBaseline.h>
#include <WebCore/FontCascade.h>
#include <WebCore/FontCascadeDescription.h>
#include <WebCore/FontCascadeEnums.h>
#include <WebCore/FontDescription.h>
#if PLATFORM(COCOA)
#include <WebCore/FontFamilySpecificationCoreText.h>
#endif
#include <WebCore/FontMetrics.h>
#include <WebCore/FontPalette.h>
#include <WebCore/FontSelectionAlgorithm.h>
#include <WebCore/FontShadow.h>
#include <WebCore/FontSizeAdjust.h>
#include <WebCore/GapRects.h>
#include <WebCore/Glyph.h>
#include <WebCore/GlyphBuffer.h>
#include <WebCore/GlyphBufferMembers.h>
#include <WebCore/GlyphDisplayListCacheRemoval.h>
#include <WebCore/GlyphMetricsMap.h>
#include <WebCore/ImageObserver.h>
#include <WebCore/InlineDisplayBox.h>
#include <WebCore/InlineDisplayContent.h>
#include <WebCore/InlineDisplayLine.h>
#include <WebCore/InlineItem.h>
#include <WebCore/InlineIteratorBox.h>
#include <WebCore/InlineIteratorBoxLegacyPath.h>
#include <WebCore/InlineIteratorBoxModernPath.h>
#include <WebCore/InlineIteratorLineBox.h>
#include <WebCore/InlineIteratorLineBoxLegacyPath.h>
#include <WebCore/InlineIteratorLineBoxModernPath.h>
#include <WebCore/InlineIteratorLogicalOrderTraversal.h>
#include <WebCore/InlineIteratorTextBox.h>
#include <WebCore/InlineLine.h>
#include <WebCore/InlineLineTypes.h>
#include <WebCore/InlineRect.h>
#include <WebCore/InlineTextItem.h>
#include <WebCore/IntSizeHash.h>
#include <WebCore/LayoutBox.h>
#include <WebCore/LayoutElementBox.h>
#include <WebCore/LayoutInlineTextBox.h>
#include <WebCore/LayoutIntegrationInlineContent.h>
#include <WebCore/LayoutShape.h>
#include <WebCore/LayoutUnits.h>
#include <WebCore/LegacyInlineBox.h>
#include <WebCore/LegacyInlineFlowBox.h>
#include <WebCore/LegacyInlineTextBox.h>
#include <WebCore/LegacyRootInlineBox.h>
#include <WebCore/LineWidth.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/MarginTypes.h>
#include <WebCore/OwnerPermissionsPolicyData.h>
#include <WebCore/PaintPhase.h>
#include <WebCore/Path.h>
#include <WebCore/PathElement.h>
#include <WebCore/PathImpl.h>
#include <WebCore/PathSegment.h>
#include <WebCore/PathSegmentData.h>
#include <WebCore/PermissionsPolicy.h>
#include <WebCore/PlatformEvent.h>
#include <WebCore/PlatformPath.h>
#include <WebCore/PointerEventTypeNames.h>
#include <WebCore/PointerID.h>
#include <WebCore/Position.h>
#include <WebCore/RectCorners.h>
#include <WebCore/Region.h>
#include <WebCore/RenderBlock.h>
#include <WebCore/RenderBlockFlow.h>
#include <WebCore/RenderBox.h>
#include <WebCore/RenderBoxModelObject.h>
#include <WebCore/RenderElement.h>
#include <WebCore/RenderLayerModelObject.h>
#include <WebCore/RenderObjectNode.h>
#include <WebCore/RenderOverflow.h>
#include <WebCore/RenderSVGInlineText.h>
#include <WebCore/RenderText.h>
#include <WebCore/RenderTextLineBoxes.h>
#include <WebCore/RotationDirection.h>
#include <WebCore/SVGImageCache.h>
#include <WebCore/SVGInlineTextBox.h>
#include <WebCore/SVGTextLayoutAttributes.h>
#include <WebCore/SVGTextMetrics.h>
#include <WebCore/ScrollSnapOffsetsInfo.h>
#include <WebCore/SelectionGeometry.h>
#include <WebCore/SelectionType.h>
#include <WebCore/ShapeOutsideInfo.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/StyleComputedStyle.h>
#include <WebCore/StyleComputedStyleBase.h>
#include <WebCore/StyleComputedStyleProperties.h>
#include <WebCore/StyleCounterStyle.h>
#include <WebCore/StyleCustomIdent.h>
#include <WebCore/StyleDifference.h>
#include <WebCore/StyleGridAutoFlow.h>
#include <WebCore/StyleListStyleType.h>
#include <WebCore/StylePrimitiveNumeric+Forward.h>
#include <WebCore/StylePrimitiveNumericConcepts.h>
#include <WebCore/StyleShapeForward.h>
#include <WebCore/StyleString.h>
#include <WebCore/StyleUnevaluatedCalculation.h>
#include <WebCore/StyleValueTypes.h>
#include <WebCore/TabSize.h>
#include <WebCore/Text.h>
#include <WebCore/TextBoxSelectableRange.h>
#include <WebCore/TextIterator.h>
#include <WebCore/TextIteratorBehavior.h>
#include <WebCore/TextRun.h>
#include <WebCore/TextSpacing.h>
#include <WebCore/TextUtil.h>
#include <WebCore/Widget.h>
#if PLATFORM(COCOA)
#include <pal/cf/OTSVGTable.h>
#endif
#include <unicode/parseerr.h>
#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/uenum.h>
#include <unicode/uloc.h>
#include <unicode/utext.h>
#include <wtf/DataRef.h>
#include <wtf/PointerComparison.h>
#include <wtf/Range.h>
#include <wtf/RefCountedWithInlineWeakPtr.h>
#include <wtf/RefTrackerMixin.h>
#include <wtf/StackShot.h>
#include <wtf/UniquelyOwned.h>
#include <wtf/UniquelyOwnedPtr.h>
#if PLATFORM(COCOA)
#include <wtf/spi/cf/CFStringSPI.h>
#endif
#include <wtf/text/CharacterProperties.h>
#include <wtf/text/TextBreakIterator.h>
#if PLATFORM(COCOA)
#include <wtf/text/cf/TextBreakIteratorCF.h>
#include <wtf/text/cf/TextBreakIteratorCFCharacterCluster.h>
#include <wtf/text/cf/TextBreakIteratorCFStringTokenizer.h>
#include <wtf/text/cocoa/ContextualizedCFString.h>
#endif
#include <wtf/text/icu/TextBreakIteratorICU.h>
#include <wtf/text/icu/UTextProviderLatin1.h>
#include <wtf/text/icu/UTextProviderUTF16.h>
#include <wtf/unicode/icu/ICUHelpers.h>

#include "EditorState.h"
#include "WebEvent.h"
#include "WebEventModifier.h"
#include "WebEventType.h"

#define new ("if you use new/delete make sure to include config.h at the top of the file"())
#define delete ("if you use new/delete make sure to include config.h at the top of the file"())
#endif
