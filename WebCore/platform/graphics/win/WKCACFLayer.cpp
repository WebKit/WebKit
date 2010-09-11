/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "WKCACFLayer.h"

#include "WKCACFLayerRenderer.h"
#include <wtf/text/CString.h>

#include <stdio.h>
#include <QuartzCore/CACFContext.h>
#include <QuartzCore/CARender.h>

#ifndef NDEBUG
#include <wtf/CurrentTime.h>
#endif

namespace WebCore {

using namespace std;

#ifndef NDEBUG
void WKCACFLayer::internalCheckLayerConsistency()
{
    ASSERT(layer());
    size_t n = sublayerCount();
    for (size_t i = 0; i < n; ++i) {
        // This will ASSERT in internalSublayerAtIndex if this entry doesn't have proper user data
        WKCACFLayer* sublayer = internalSublayerAtIndex(i);

        // Make sure we don't have any null entries in the list
        ASSERT(sublayer);

        // Make sure the each layer has a corresponding CACFLayer
        ASSERT(sublayer->layer());
    }
}
#endif

static void displayCallback(CACFLayerRef layer, CGContextRef context)
{
    ASSERT_ARG(layer, WKCACFLayer::layer(layer));
    WKCACFLayer::layer(layer)->drawInContext(context);
}

static CFStringRef toCACFLayerType(WKCACFLayer::LayerType type)
{
    switch (type) {
    case WKCACFLayer::Layer: return kCACFLayer;
    case WKCACFLayer::TransformLayer: return kCACFTransformLayer;
    default: return 0;
    }
}

static CFStringRef toCACFContentsGravityType(WKCACFLayer::ContentsGravityType type)
{
    switch (type) {
    case WKCACFLayer::Center: return kCACFGravityCenter;
    case WKCACFLayer::Top: return kCACFGravityTop;
    case WKCACFLayer::Bottom: return kCACFGravityBottom;
    case WKCACFLayer::Left: return kCACFGravityLeft;
    case WKCACFLayer::Right: return kCACFGravityRight;
    case WKCACFLayer::TopLeft: return kCACFGravityTopLeft;
    case WKCACFLayer::TopRight: return kCACFGravityTopRight;
    case WKCACFLayer::BottomLeft: return kCACFGravityBottomLeft;
    case WKCACFLayer::BottomRight: return kCACFGravityBottomRight;
    case WKCACFLayer::Resize: return kCACFGravityResize;
    case WKCACFLayer::ResizeAspect: return kCACFGravityResizeAspect;
    case WKCACFLayer::ResizeAspectFill: return kCACFGravityResizeAspectFill;
    default: return 0;
    }
}

static WKCACFLayer::ContentsGravityType fromCACFContentsGravityType(CFStringRef string)
{
    if (CFEqual(string, kCACFGravityTop))
        return WKCACFLayer::Top;

    if (CFEqual(string, kCACFGravityBottom))
        return WKCACFLayer::Bottom;

    if (CFEqual(string, kCACFGravityLeft))
        return WKCACFLayer::Left;

    if (CFEqual(string, kCACFGravityRight))
        return WKCACFLayer::Right;

    if (CFEqual(string, kCACFGravityTopLeft))
        return WKCACFLayer::TopLeft;

    if (CFEqual(string, kCACFGravityTopRight))
        return WKCACFLayer::TopRight;

    if (CFEqual(string, kCACFGravityBottomLeft))
        return WKCACFLayer::BottomLeft;

    if (CFEqual(string, kCACFGravityBottomRight))
        return WKCACFLayer::BottomRight;

    if (CFEqual(string, kCACFGravityResize))
        return WKCACFLayer::Resize;

    if (CFEqual(string, kCACFGravityResizeAspect))
        return WKCACFLayer::ResizeAspect;

    if (CFEqual(string, kCACFGravityResizeAspectFill))
        return WKCACFLayer::ResizeAspectFill;

    return WKCACFLayer::Center;
}

static CFStringRef toCACFFilterType(WKCACFLayer::FilterType type)
{
    switch (type) {
    case WKCACFLayer::Linear: return kCACFFilterLinear;
    case WKCACFLayer::Nearest: return kCACFFilterNearest;
    case WKCACFLayer::Trilinear: return kCACFFilterTrilinear;
    default: return 0;
    }
}

static WKCACFLayer::FilterType fromCACFFilterType(CFStringRef string)
{
    if (CFEqual(string, kCACFFilterNearest))
        return WKCACFLayer::Nearest;

    if (CFEqual(string, kCACFFilterTrilinear))
        return WKCACFLayer::Trilinear;

    return WKCACFLayer::Linear;
}

PassRefPtr<WKCACFLayer> WKCACFLayer::create(LayerType type)
{
    if (!WKCACFLayerRenderer::acceleratedCompositingAvailable())
        return 0;
    return adoptRef(new WKCACFLayer(type));
}

// FIXME: It might be good to have a way of ensuring that all WKCACFLayers eventually
// get destroyed in debug builds. A static counter could accomplish this pretty easily.

WKCACFLayer::WKCACFLayer(LayerType type)
    : m_layer(AdoptCF, CACFLayerCreate(toCACFLayerType(type)))
    , m_layoutClient(0)
    , m_needsDisplayOnBoundsChange(false)
{
    CACFLayerSetUserData(layer(), this);
    CACFLayerSetDisplayCallback(layer(), displayCallback);
}

WKCACFLayer::~WKCACFLayer()
{
    // Our superlayer should be holding a reference to us, so there should be no way for us to be destroyed while we still have a superlayer.
    ASSERT(!superlayer());

    // Get rid of the children so we don't have any dangling references around
    removeAllSublayers();

#ifndef NDEBUG
    CACFLayerSetUserData(layer(), reinterpret_cast<void*>(0xDEADBEEF));
#else
    CACFLayerSetUserData(layer(), 0);
#endif
    CACFLayerSetDisplayCallback(layer(), 0);
}

void WKCACFLayer::becomeRootLayerForContext(CACFContextRef context)
{
    CACFContextSetLayer(context, layer());
    setNeedsCommit();
}

void WKCACFLayer::setNeedsCommit()
{
    WKCACFLayer* root = rootLayer();

    // Call setNeedsRender on the root layer, which will cause a render to 
    // happen in WKCACFLayerRenderer
    root->setNeedsRender();
}

bool WKCACFLayer::isTransformLayer() const
{
    return CACFLayerGetClass(layer()) == kCACFTransformLayer;
}

void WKCACFLayer::addSublayer(PassRefPtr<WKCACFLayer> sublayer)
{
    insertSublayer(sublayer, sublayerCount());
}

void WKCACFLayer::internalInsertSublayer(PassRefPtr<WKCACFLayer> sublayer, size_t index)
{
    index = min(index, sublayerCount() + 1);
    sublayer->removeFromSuperlayer();
    CACFLayerInsertSublayer(layer(), sublayer->layer(), index);
    setNeedsCommit();
    checkLayerConsistency();
}

void WKCACFLayer::insertSublayerAboveLayer(PassRefPtr<WKCACFLayer> sublayer, const WKCACFLayer* reference)
{
    if (!reference) {
        insertSublayer(sublayer, 0);
        return;
    }

    int referenceIndex = internalIndexOfSublayer(reference);
    if (referenceIndex == -1) {
        addSublayer(sublayer);
        return;
    }

    insertSublayer(sublayer, referenceIndex + 1);
}

void WKCACFLayer::insertSublayerBelowLayer(PassRefPtr<WKCACFLayer> sublayer, const WKCACFLayer* reference)
{
    if (!reference) {
        insertSublayer(sublayer, 0);
        return;
    }

    int referenceIndex = internalIndexOfSublayer(reference);
    if (referenceIndex == -1) {
        addSublayer(sublayer);
        return;
    }

    insertSublayer(sublayer, referenceIndex);
}

void WKCACFLayer::replaceSublayer(WKCACFLayer* reference, PassRefPtr<WKCACFLayer> newLayer)
{
    ASSERT_ARG(reference, reference);
    ASSERT_ARG(reference, reference->superlayer() == this);

    if (reference == newLayer)
        return;

    int referenceIndex = internalIndexOfSublayer(reference);
    ASSERT(referenceIndex != -1);
    if (referenceIndex == -1)
        return;

    reference->removeFromSuperlayer();

    if (newLayer) {
        newLayer->removeFromSuperlayer();
        insertSublayer(newLayer, referenceIndex);
    }
}

size_t WKCACFLayer::internalSublayerCount() const
{
    CFArrayRef sublayers = CACFLayerGetSublayers(layer());
    return sublayers ? CFArrayGetCount(sublayers) : 0;
}

void  WKCACFLayer::adoptSublayers(WKCACFLayer* source)
{
    // We will use setSublayers() because it properly nulls
    // out the superlayer pointer.
    Vector<RefPtr<WKCACFLayer> > sublayers;
    size_t n = source->sublayerCount();

    for (size_t i = 0; i < n; ++i)
        sublayers.append(source->internalSublayerAtIndex(i));

    setSublayers(sublayers);
    source->checkLayerConsistency();
}

void WKCACFLayer::removeFromSuperlayer()
{
    WKCACFLayer* superlayer = this->superlayer();
    CACFLayerRemoveFromSuperlayer(layer());
    checkLayerConsistency();

    if (superlayer)
        superlayer->setNeedsCommit();
}

WKCACFLayer* WKCACFLayer::internalSublayerAtIndex(int index) const
{
    CFArrayRef sublayers = CACFLayerGetSublayers(layer());
    if (!sublayers || index < 0 || CFArrayGetCount(sublayers) <= index)
        return 0;
    
    return layer(static_cast<CACFLayerRef>(const_cast<void*>(CFArrayGetValueAtIndex(sublayers, index))));
}

int WKCACFLayer::internalIndexOfSublayer(const WKCACFLayer* reference)
{
    CACFLayerRef ref = reference->layer();
    if (!ref)
        return -1;

    CFArrayRef sublayers = CACFLayerGetSublayers(layer());
    if (!sublayers)
        return -1;

    size_t n = CFArrayGetCount(sublayers);

    for (size_t i = 0; i < n; ++i)
        if (CFArrayGetValueAtIndex(sublayers, i) == ref)
            return i;

    return -1;
}

WKCACFLayer* WKCACFLayer::ancestorOrSelfWithSuperlayer(WKCACFLayer* superlayer) const
{
    WKCACFLayer* layer = const_cast<WKCACFLayer*>(this);
    for (WKCACFLayer* ancestor = this->superlayer(); ancestor; layer = ancestor, ancestor = ancestor->superlayer()) {
        if (ancestor == superlayer)
            return layer;
    }
    return 0;
}

void WKCACFLayer::setBounds(const CGRect& rect)
{
    if (CGRectEqualToRect(rect, bounds()))
        return;

    CACFLayerSetBounds(layer(), rect);
    setNeedsCommit();

    if (m_needsDisplayOnBoundsChange)
        setNeedsDisplay();

    if (m_layoutClient)
        setNeedsLayout();
}

void WKCACFLayer::setFrame(const CGRect& rect)
{
    CGRect oldFrame = frame();
    if (CGRectEqualToRect(rect, oldFrame))
        return;

    CACFLayerSetFrame(layer(), rect);
    setNeedsCommit();

    if (m_needsDisplayOnBoundsChange && !CGSizeEqualToSize(rect.size, oldFrame.size))
        setNeedsDisplay();

    if (m_layoutClient)
        setNeedsLayout();
}

void WKCACFLayer::setContentsGravity(ContentsGravityType type)
{
    CACFLayerSetContentsGravity(layer(), toCACFContentsGravityType(type));
    setNeedsCommit();
}

WKCACFLayer::ContentsGravityType WKCACFLayer::contentsGravity() const
{
    return fromCACFContentsGravityType(CACFLayerGetContentsGravity(layer()));
}

void WKCACFLayer::setMagnificationFilter(FilterType type)
{
    CACFLayerSetMagnificationFilter(layer(), toCACFFilterType(type));
    setNeedsCommit();
}

WKCACFLayer::FilterType WKCACFLayer::magnificationFilter() const
{
    return fromCACFFilterType(CACFLayerGetMagnificationFilter(layer()));
}

void WKCACFLayer::setMinificationFilter(FilterType type)
{
    CACFLayerSetMinificationFilter(layer(), toCACFFilterType(type));
    setNeedsCommit();
}

WKCACFLayer::FilterType WKCACFLayer::minificationFilter() const
{
    return fromCACFFilterType(CACFLayerGetMinificationFilter(layer()));
}

WKCACFLayer* WKCACFLayer::rootLayer() const
{
    WKCACFLayer* layer = const_cast<WKCACFLayer*>(this);
    for (WKCACFLayer* superlayer = layer->superlayer(); superlayer; layer = superlayer, superlayer = superlayer->superlayer()) { }
    return layer;
}

void WKCACFLayer::internalRemoveAllSublayers()
{
    CACFLayerSetSublayers(layer(), 0);
    setNeedsCommit();
}

void WKCACFLayer::internalSetSublayers(const Vector<RefPtr<WKCACFLayer> >& sublayers)
{
    // Remove all the current sublayers and add the passed layers
    CACFLayerSetSublayers(layer(), 0);

    // Perform removeFromSuperLayer in a separate pass. CACF requires superlayer to
    // be null or CACFLayerInsertSublayer silently fails.
    for (size_t i = 0; i < sublayers.size(); i++)
        CACFLayerRemoveFromSuperlayer(sublayers[i]->layer());

    for (size_t i = 0; i < sublayers.size(); i++)
        CACFLayerInsertSublayer(layer(), sublayers[i]->layer(), i);

    setNeedsCommit();
}

WKCACFLayer* WKCACFLayer::superlayer() const
{
    CACFLayerRef super = CACFLayerGetSuperlayer(layer());
    if (!super)
        return 0;
    return WKCACFLayer::layer(super);
}

void WKCACFLayer::internalSetNeedsDisplay(const CGRect* dirtyRect)
{
    CACFLayerSetNeedsDisplay(layer(), dirtyRect);
}

void WKCACFLayer::setLayoutClient(WKCACFLayerLayoutClient* layoutClient)
{
    if (layoutClient == m_layoutClient)
        return;

    m_layoutClient = layoutClient;
    CACFLayerSetLayoutCallback(layer(), m_layoutClient ? layoutSublayersProc : 0);    
}

void WKCACFLayer::layoutSublayersProc(CACFLayerRef caLayer) 
{
    WKCACFLayer* layer = WKCACFLayer::layer(caLayer);
    if (layer && layer->m_layoutClient)
        layer->m_layoutClient->layoutSublayersOfLayer(layer);
}

#ifndef NDEBUG
static void printIndent(int indent)
{
    for ( ; indent > 0; --indent)
        fprintf(stderr, "  ");
}

static void printTransform(const CATransform3D& transform)
{
    fprintf(stderr, "[%g %g %g %g; %g %g %g %g; %g %g %g %g; %g %g %g %g]",
                    transform.m11, transform.m12, transform.m13, transform.m14, 
                    transform.m21, transform.m22, transform.m23, transform.m24, 
                    transform.m31, transform.m32, transform.m33, transform.m34, 
                    transform.m41, transform.m42, transform.m43, transform.m44);
}

void WKCACFLayer::printTree() const
{
    // Print heading info
    CGRect rootBounds = bounds();
    fprintf(stderr, "\n\n** Render tree at time %g (bounds %g, %g %gx%g) **\n\n", 
        currentTime(), rootBounds.origin.x, rootBounds.origin.y, rootBounds.size.width, rootBounds.size.height);

    // Print layer tree from the root
    printLayer(0);
}

void WKCACFLayer::printLayer(int indent) const
{
    CGPoint layerPosition = position();
    CGPoint layerAnchorPoint = anchorPoint();
    CGRect layerBounds = bounds();
    printIndent(indent);
    fprintf(stderr, "(%s [%g %g %g] [%g %g %g %g] [%g %g %g] superlayer=%p\n",
        isTransformLayer() ? "transform-layer" : "layer",
        layerPosition.x, layerPosition.y, zPosition(), 
        layerBounds.origin.x, layerBounds.origin.y, layerBounds.size.width, layerBounds.size.height,
        layerAnchorPoint.x, layerAnchorPoint.y, anchorPointZ(), superlayer());

    // Print name if needed
    String layerName = name();
    if (!layerName.isEmpty()) {
        printIndent(indent + 1);
        fprintf(stderr, "(name %s)\n", layerName.utf8().data());
    }

    // Print masksToBounds if needed
    bool layerMasksToBounds = masksToBounds();
    if (layerMasksToBounds) {
        printIndent(indent + 1);
        fprintf(stderr, "(masksToBounds true)\n");
    }

    // Print opacity if needed
    float layerOpacity = opacity();
    if (layerOpacity != 1) {
        printIndent(indent + 1);
        fprintf(stderr, "(opacity %hf)\n", layerOpacity);
    }

    // Print sublayerTransform if needed
    CATransform3D layerTransform = sublayerTransform();
    if (!CATransform3DIsIdentity(layerTransform)) {
        printIndent(indent + 1);
        fprintf(stderr, "(sublayerTransform ");
        printTransform(layerTransform);
        fprintf(stderr, ")\n");
    }

    // Print transform if needed
    layerTransform = transform();
    if (!CATransform3DIsIdentity(layerTransform)) {
        printIndent(indent + 1);
        fprintf(stderr, "(transform ");
        printTransform(layerTransform);
        fprintf(stderr, ")\n");
    }

    // Print contents if needed
    CFTypeRef layerContents = contents();
    if (layerContents) {
        if (CFGetTypeID(layerContents) == CGImageGetTypeID()) {
            CGImageRef imageContents = static_cast<CGImageRef>(const_cast<void*>(layerContents));
            printIndent(indent + 1);
            fprintf(stderr, "(contents (image [%d %d]))\n",
                CGImageGetWidth(imageContents), CGImageGetHeight(imageContents));
        }
    }

    // Print sublayers if needed
    int n = sublayerCount();
    if (n > 0) {
        printIndent(indent + 1);
        fprintf(stderr, "(sublayers\n");
        for (int i = 0; i < n; ++i)
            internalSublayerAtIndex(i)->printLayer(indent + 2);

        printIndent(indent + 1);
        fprintf(stderr, ")\n");
    }

    printIndent(indent);
    fprintf(stderr, ")\n");
}
#endif // #ifndef NDEBUG
}

#endif // USE(ACCELERATED_COMPOSITING)
