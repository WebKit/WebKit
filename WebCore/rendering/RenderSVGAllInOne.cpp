/*
 * Copyright (C) 2009, 2010 Apple Inc. All Rights Reserved.
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

// This all-in-one cpp file cuts down on template bloat to allow us to build our Windows release build.

#include "RenderSVGBlock.cpp"
#include "RenderSVGContainer.cpp"
#include "RenderSVGGradientStop.cpp"
#include "RenderSVGHiddenContainer.cpp"
#include "RenderSVGImage.cpp"
#include "RenderSVGModelObject.cpp"
#include "RenderSVGResource.cpp"
#include "RenderSVGResourceClipper.cpp"
#include "RenderSVGResourceContainer.cpp"
#include "RenderSVGResourceFilter.cpp"
#include "RenderSVGResourceFilterPrimitive.cpp"
#include "RenderSVGResourceGradient.cpp"
#include "RenderSVGResourceLinearGradient.cpp"
#include "RenderSVGResourceMarker.cpp"
#include "RenderSVGResourceMasker.cpp"
#include "RenderSVGResourcePattern.cpp"
#include "RenderSVGResourceRadialGradient.cpp"
#include "RenderSVGResourceSolidColor.cpp"
#include "RenderSVGRoot.cpp"
#include "RenderSVGShadowTreeRootContainer.cpp"
#include "RenderSVGTransformableContainer.cpp"
#include "RenderSVGViewportContainer.cpp"
#include "SVGImageBufferTools.cpp"
#include "SVGMarkerLayoutInfo.cpp"
#include "SVGRenderSupport.cpp"
#include "SVGRenderTreeAsText.cpp"
#include "SVGResources.cpp"
#include "SVGResourcesCache.cpp"
#include "SVGResourcesCycleSolver.cpp"
#include "SVGShadowTreeElements.cpp"

// FIXME: As soon as all SVG renderers live in rendering/svg, this file should be moved there as well, removing the need for the svg/ includes below.
#include "svg/RenderSVGInline.cpp"
#include "svg/RenderSVGInlineText.cpp"
#include "svg/RenderSVGTSpan.cpp"
#include "svg/RenderSVGText.cpp"
#include "svg/RenderSVGTextPath.cpp"
#include "svg/SVGInlineFlowBox.cpp"
#include "svg/SVGInlineTextBox.cpp"
#include "svg/SVGRootInlineBox.cpp"
#include "svg/SVGTextQuery.cpp"
