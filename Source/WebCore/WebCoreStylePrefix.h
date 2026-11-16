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

#include "WebCoreDOMAndRenderingPrefix.h"

#ifdef __cplusplus
#undef new
#undef delete

#include <map>
#include <wtf/BloomFilter.h>
#include <wtf/EnumeratedArray.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/OrderedHashMap.h>

#include <JavaScriptCore/LLIntThunks.h>

#include "AdjustViewSize.h"
#include "AnchorPositionEvaluator.h"
#include "CSSKeywordValue.h"
#include "CSSKeywordValueInlines.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "CSSPropertyInitialValues.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "ColorHash.h"
#include "ColorInterpolationMethod.h"
#include "CompiledSelector.h"
#include "ContainerQuery.h"
#include "ContainerQueryEvaluator.h"
#include "DocumentView.h"
#include "FontFeatureValues.h"
#include "FontPaletteValues.h"
#include "FrameView.h"
#include "GenericMediaQueryEvaluator.h"
#include "HasSelectorFilter.h"
#include "IterationCompositeOperation.h"
#include "LocalFrameView.h"
#include "LocalFrameViewLayoutContext.h"
#include "MutableStyleProperties.h"
#include "Namespace.h"
#include "NodeName.h"
#include "PopupMenuStyle.h"
#include "PropertyAllowlist.h"
#include "PropertyCascade.h"
#include "RenderObjectStyle.h"
#include "RuleData.h"
#include "RuleFeature.h"
#include "RuleSet.h"
#include "SelectorChecker.h"
#include "SelectorCompiler.h"
#include "SelectorFilter.h"
#include "SelectorMatchingState.h"
#include "StyleBuilderState.h"
#include "StylePropertiesInlines.h"
#include "StyleRelations.h"
#include "StyleRule.h"
#include "StyleScrollbarState.h"
#include "StyleUpdate.h"
#include "Styleable.h"
#include "SubtreeScrollbarChangesState.h"
#include "TagName.h"
#include "ThemeTypes.h"
#include "TreeResolutionState.h"

#define new ("if you use new/delete make sure to include config.h at the top of the file"())
#define delete ("if you use new/delete make sure to include config.h at the top of the file"())
#endif
