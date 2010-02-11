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

#include "CString.h"
#include "WKCACFContextFlusher.h"
#include "WKCACFLayerRenderer.h"

#include <stdio.h>
#include <QuartzCore/CACFContext.h>
#include <QuartzCore/CARender.h>
#include <QuartzCoreInterface/QuartzCoreInterface.h>

#ifndef NDEBUG
#include <wtf/CurrentTime.h>
#endif

#ifdef DEBUG_ALL
#pragma comment(lib, "QuartzCore_debug")
#pragma comment(lib, "QuartzCoreInterface_debug")
#else
#pragma comment(lib, "QuartzCore")
#pragma comment(lib, "QuartzCoreInterface")
#endif

namespace WebCore {

using namespace std;

static void displayInContext(CACFLayerRef layer, CGContextRef context)
{
    ASSERT_ARG(layer, WKCACFLayer::layer(layer));
    WKCACFLayer::layer(layer)->display(context);
}

#define STATIC_CACF_STRING(name) \
    static CFStringRef name() \
    { \
        static CFStringRef name = wkqcCFStringRef(wkqc##name); \
        return name; \
    }

STATIC_CACF_STRING(kCACFLayer)
STATIC_CACF_STRING(kCACFTransformLayer)
STATIC_CACF_STRING(kCACFGravityCenter)
STATIC_CACF_STRING(kCACFGravityTop)
STATIC_CACF_STRING(kCACFGravityBottom)
STATIC_CACF_STRING(kCACFGravityLeft)
STATIC_CACF_STRING(kCACFGravityRight)
STATIC_CACF_STRING(kCACFGravityTopLeft)
STATIC_CACF_STRING(kCACFGravityTopRight)
STATIC_CACF_STRING(kCACFGravityBottomLeft)
STATIC_CACF_STRING(kCACFGravityBottomRight)
STATIC_CACF_STRING(kCACFGravityResize)
STATIC_CACF_STRING(kCACFGravityResizeAspect)
STATIC_CACF_STRING(kCACFGravityResizeAspectFill)
STATIC_CACF_STRING(kCACFFilterLinear)
STATIC_CACF_STRING(kCACFFilterNearest)
STATIC_CACF_STRING(kCACFFilterTrilinear)
STATIC_CACF_STRING(kCACFFilterLanczos)

static CFStringRef toCACFLayerType(WKCACFLayer::LayerType type)
{
    switch (type) {
    case WKCACFLayer::Layer: return kCACFLayer();
    case WKCACFLayer::TransformLayer: return kCACFTransformLayer();
    default: return 0;
    }
}

static CFStringRef toCACFContentsGravityType(WKCACFLayer::ContentsGravityType type)
{
    switch (type) {
    case WKCACFLayer::Center: return kCACFGravityCenter();
    case WKCACFLayer::Top: return kCACFGravityTop();
    case WKCACFLayer::Bottom: return kCACFGravityBottom();
    case WKCACFLayer::Left: return kCACFGravityLeft();
    case WKCACFLayer::Right: return kCACFGravityRight();
    case WKCACFLayer::TopLeft: return kCACFGravityTopLeft();
    case WKCACFLayer::TopRight: return kCACFGravityTopRight();
    case WKCACFLayer::BottomLeft: return kCACFGravityBottomLeft();
    case WKCACFLayer::BottomRight: return kCACFGravityBottomRight();
    case WKCACFLayer::Resize: return kCACFGravityResize();
    case WKCACFLayer::ResizeAspect: return kCACFGravityResizeAspect();
    case WKCACFLayer::ResizeAspectFill: return kCACFGravityResizeAspectFill();
    default: return 0;
    }
}

static WKCACFLayer::ContentsGravityType fromCACFContentsGravityType(CFStringRef string)
{
    if (CFEqual(string, kCACFGravityTop()))
        return WKCACFLayer::Top;

    if (CFEqual(string, kCACFGravityBottom()))
        return WKCACFLayer::Bottom;

    if (CFEqual(string, kCACFGravityLeft()))
        return WKCACFLayer::Left;

    if (CFEqual(string, kCACFGravityRight()))
        return WKCACFLayer::Right;

    if (CFEqual(string, kCACFGravityTopLeft()))
        return WKCACFLayer::TopLeft;

    if (CFEqual(string, kCACFGravityTopRight()))
        return WKCACFLayer::TopRight;

    if (CFEqual(string, kCACFGravityBottomLeft()))
        return WKCACFLayer::BottomLeft;

    if (CFEqual(string, kCACFGravityBottomRight()))
        return WKCACFLayer::BottomRight;

    if (CFEqual(string, kCACFGravityResize()))
        return WKCACFLayer::Resize;

    if (CFEqual(string, kCACFGravityResizeAspect()))
        return WKCACFLayer::ResizeAspect;

    if (CFEqual(string, kCACFGravityResizeAspectFill()))
        return WKCACFLayer::ResizeAspectFill;

    return WKCACFLayer::Center;
}

static CFStringRef toCACFFilterType(WKCACFLayer::FilterType type)
{
    switch (type) {
    case WKCACFLayer::Linear: return kCACFFilterLinear();
    case WKCACFLayer::Nearest: return kCACFFilterNearest();
    case WKCACFLayer::Trilinear: return kCACFFilterTrilinear();
    case WKCACFLayer::Lanczos: return kCACFFilterLanczos();
    default: return 0;
    }
}

static WKCACFLayer::FilterType fromCACFFilterType(CFStringRef string)
{
    if (CFEqual(string, kCACFFilterNearest()))
        return WKCACFLayer::Nearest;

    if (CFEqual(string, kCACFFilterTrilinear()))
        return WKCACFLayer::Trilinear;

    if (CFEqual(string, kCACFFilterLanczos()))
        return WKCACFLayer::Lanczos;

    return WKCACFLayer::Linear;
}

PassRefPtr<WKCACFLayer> WKCACFLayer::create(LayerType type, GraphicsLayerCACF* owner)
{
    if (!WKCACFLayerRenderer::acceleratedCompositingAvailable())
        return 0;
    return adoptRef(new WKCACFLayer(type, owner));
}

// FIXME: It might be good to have a way of ensuring that all WKCACFLayers eventually
// get destroyed in debug builds. A static counter could accomplish this pretty easily.

WKCACFLayer::WKCACFLayer(LayerType type, GraphicsLayerCACF* owner)
    : m_layer(AdoptCF, CACFLayerCreate(toCACFLayerType(type)))
    , m_needsDisplayOnBoundsChange(false)
    , m_owner(owner)
{
    CACFLayerSetUserData(layer(), this);
    CACFLayerSetDisplayCallback(layer(), displayInContext);
}

WKCACFLayer::~WKCACFLayer()
{
    // Our superlayer should be holding a reference to us, so there should be no way for us to be destroyed while we still have a superlayer.
    ASSERT(!superlayer());

    CACFLayerSetUserData(layer(), 0);
    CACFLayerSetDisplayCallback(layer(), 0);
}

void WKCACFLayer::display(PlatformGraphicsContext* context)
{
    if (!m_owner)
        return;

    CGContextSaveGState(context);

    CGRect layerBounds = bounds();
    if (m_owner->contentsOrientation() == WebCore::GraphicsLayer::CompositingCoordinatesTopDown) {
        CGContextScaleCTM(context, 1, -1);
        CGContextTranslateCTM(context, 0, -layerBounds.size.height);
    }

    if (m_owner->client()) {
        GraphicsContext graphicsContext(context);

        // It's important to get the clip from the context, because it may be significantly
        // smaller than the layer bounds (e.g. tiled layers)
        CGRect clipBounds = CGContextGetClipBoundingBox(context);
        IntRect clip(enclosingIntRect(clipBounds));
        m_owner->paintGraphicsLayerContents(graphicsContext, clip);
    }
#ifndef NDEBUG
    else {
        ASSERT_NOT_REACHED();

        // FIXME: ideally we'd avoid calling -setNeedsDisplay on a layer that is a plain color,
        // so CA never makes backing store for it (which is what -setNeedsDisplay will do above).
        CGContextSetRGBFillColor(context, 0.0f, 1.0f, 0.0f, 1.0f);
        CGContextFillRect(context, layerBounds);
    }
#endif

    if (m_owner->showRepaintCounter()) {
        char text[16]; // that's a lot of repaints
        _snprintf(text, sizeof(text), "%d", m_owner->incrementRepaintCount());

        CGContextSaveGState(context);
        CGContextSetRGBFillColor(context, 1.0f, 0.0f, 0.0f, 0.8f);
        
        CGRect aBounds = layerBounds;

        aBounds.size.width = 10 + 12 * strlen(text);
        aBounds.size.height = 25;
        CGContextFillRect(context, aBounds);
        
        CGContextSetRGBFillColor(context, 0.0f, 0.0f, 0.0f, 1.0f);

        CGContextSetTextMatrix(context, CGAffineTransformMakeScale(1.0f, -1.0f));
        CGContextSelectFont(context, "Helvetica", 25, kCGEncodingMacRoman);
        CGContextShowTextAtPoint(context, aBounds.origin.x + 3.0f, aBounds.origin.y + 20.0f, text, strlen(text));
        
        CGContextRestoreGState(context);        
    }

    CGContextRestoreGState(context);
}

void WKCACFLayer::becomeRootLayerForContext(CACFContextRef context)
{
    CACFContextSetLayer(context, layer());
    setNeedsCommit();
}

void WKCACFLayer::setNeedsCommit()
{
    CACFContextRef context = CACFLayerGetContext(rootLayer()->layer());

    // The context might now be set yet. This happens if a property gets set
    // before placing the layer in the tree. In this case we don't need to 
    // worry about remembering the context because we will when the layer is
    // added to the tree.
    if (context)
        WKCACFContextFlusher::shared().addContext(context);

    // Call notifySyncRequired(), which in this implementation plumbs through to
    // call setRootLayerNeedsDisplay() on the WebView, which causes the CACFRenderer
    // to render a frame.
    if (m_owner)
        m_owner->notifySyncRequired();
}

bool WKCACFLayer::isTransformLayer() const
{
    return CACFLayerGetClass(layer()) == kCACFTransformLayer();
}

void WKCACFLayer::addSublayer(PassRefPtr<WKCACFLayer> sublayer)
{
    insertSublayer(sublayer, numSublayers());
}

void WKCACFLayer::insertSublayer(PassRefPtr<WKCACFLayer> sublayer, size_t index)
{
    index = min(index, numSublayers());
    CACFLayerInsertSublayer(layer(), sublayer->layer(), index);
    setNeedsCommit();
}

void WKCACFLayer::insertSublayerAboveLayer(PassRefPtr<WKCACFLayer> sublayer, const WKCACFLayer* reference)
{
    if (!reference) {
        insertSublayer(sublayer, 0);
        return;
    }

    int referenceIndex = indexOfSublayer(reference);
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

    int referenceIndex = indexOfSublayer(reference);
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

    if (!newLayer) {
        removeSublayer(reference);
        return;
    }

    newLayer->removeFromSuperlayer();

    int referenceIndex = indexOfSublayer(reference);
    ASSERT(referenceIndex != -1);
    if (referenceIndex == -1)
        return;

    // FIXME: Can we make this more efficient? The current CACF API doesn't seem to give us a way to do so.
    reference->removeFromSuperlayer();
    insertSublayer(newLayer, referenceIndex);
}

void WKCACFLayer::removeFromSuperlayer()
{
    WKCACFLayer* superlayer = this->superlayer();
    if (!superlayer)
        return;

    superlayer->removeSublayer(this);
    CACFLayerRemoveFromSuperlayer(layer());
    superlayer->setNeedsCommit();
}

void WKCACFLayer::removeSublayer(const WKCACFLayer* sublayer)
{
    int foundIndex = indexOfSublayer(sublayer);
    if (foundIndex == -1)
        return;

    CACFLayerRemoveFromSuperlayer(sublayer->layer());
    setNeedsCommit();
}

const WKCACFLayer* WKCACFLayer::sublayerAtIndex(int index) const
{
    CFArrayRef sublayers = CACFLayerGetSublayers(layer());
    if (index < 0 || CFArrayGetCount(sublayers) <= index)
        return 0;
    
    return layer(static_cast<CACFLayerRef>(const_cast<void*>(CFArrayGetValueAtIndex(sublayers, index))));
}

int WKCACFLayer::indexOfSublayer(const WKCACFLayer* reference)
{
    CACFLayerRef ref = reference->layer();
    if (!ref)
        return -1;

    CFArrayRef sublayers = CACFLayerGetSublayers(layer());
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
}

void WKCACFLayer::setFrame(const CGRect& rect)
{
    CGRect oldFrame = frame();
    if (CGRectEqualToRect(rect, oldFrame))
        return;

    CACFLayerSetFrame(layer(), rect);
    setNeedsCommit();

    if (m_needsDisplayOnBoundsChange)
        setNeedsDisplay();
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

void WKCACFLayer::removeAllSublayers()
{
    CACFLayerSetSublayers(layer(), 0);
    setNeedsCommit();
}

void WKCACFLayer::setSublayers(const Vector<RefPtr<WKCACFLayer> >& sublayers)
{
    if (sublayers.isEmpty())
        CACFLayerSetSublayers(layer(), 0);
    else {
        // Create a vector of CACFLayers.
        Vector<const void*> layers;
        for (size_t i = 0; i < sublayers.size(); i++)
            layers.append(sublayers[i]->layer());
    
        RetainPtr<CFArrayRef> layersArray(AdoptCF, CFArrayCreate(0, layers.data(), layers.size(), 0));
        CACFLayerSetSublayers(layer(), layersArray.get());
    }
    
    setNeedsCommit();
}

void WKCACFLayer::moveSublayers(WKCACFLayer* fromLayer, WKCACFLayer* toLayer)
{
    if (!fromLayer || !toLayer)
        return;

    CACFLayerSetSublayers(toLayer->layer(), CACFLayerGetSublayers(fromLayer->layer()));
    fromLayer->setNeedsCommit();
    toLayer->setNeedsCommit();
}

WKCACFLayer* WKCACFLayer::superlayer() const
{
    CACFLayerRef super = CACFLayerGetSuperlayer(layer());
    if (!super)
        return 0;
    return WKCACFLayer::layer(super);
}

void WKCACFLayer::setNeedsDisplay(const CGRect& dirtyRect)
{
    if (m_owner)
        CACFLayerSetNeedsDisplay(layer(), &dirtyRect);
    setNeedsCommit();
}

void WKCACFLayer::setNeedsDisplay()
{
    if (m_owner)
        CACFLayerSetNeedsDisplay(layer(), 0);
    setNeedsCommit();
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
    fprintf(stderr, "(%s [%g %g %g] [%g %g %g %g] [%g %g %g]\n",
        isTransformLayer() ? "transform-layer" : "layer",
        layerPosition.x, layerPosition.y, zPosition(), 
        layerBounds.origin.x, layerBounds.origin.y, layerBounds.size.width, layerBounds.size.height,
        layerAnchorPoint.x, layerAnchorPoint.y, anchorPointZ());

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
    CGImageRef layerContents = contents();
    if (layerContents) {
        printIndent(indent + 1);
        fprintf(stderr, "(contents (image [%d %d]))\n",
            CGImageGetWidth(layerContents), CGImageGetHeight(layerContents));
    }

    // Print sublayers if needed
    int n = numSublayers();
    if (n > 0) {
        printIndent(indent + 1);
        fprintf(stderr, "(sublayers\n");
        for (int i = 0; i < n; ++i)
            sublayerAtIndex(i)->printLayer(indent + 2);

        printIndent(indent + 1);
        fprintf(stderr, ")\n");
    }

    printIndent(indent);
    fprintf(stderr, ")\n");
}
#endif // #ifndef NDEBUG
}

#endif // USE(ACCELERATED_COMPOSITING)
