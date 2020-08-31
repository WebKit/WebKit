/*
 * Copyright (C) 2011, 2014-2015 Apple Inc. All rights reserved.
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

#include "config.h"

#include "PlatformCALayerWin.h"

#if USE(CA)

#include "AbstractCACFLayerTreeHost.h"
#include "ColorSerialization.h"
#include "FontCascade.h"
#include "GDIUtilities.h"
#include "GraphicsContext.h"
#include "PlatformCAAnimationWin.h"
#include "PlatformCALayerWinInternal.h"
#include "TextRun.h"
#include "TileController.h"
#include "WebTiledBackingLayerWin.h"
#include <QuartzCore/CoreAnimationCF.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

Ref<PlatformCALayer> PlatformCALayerWin::create(LayerType layerType, PlatformCALayerClient* owner)
{
    return adoptRef(*new PlatformCALayerWin(layerType, nullptr, owner));
}

Ref<PlatformCALayer> PlatformCALayerWin::create(PlatformLayer* platformLayer, PlatformCALayerClient* owner)
{
    return adoptRef(*new PlatformCALayerWin(LayerTypeCustom, platformLayer, owner));
}

static CFStringRef toCACFLayerType(PlatformCALayer::LayerType type)
{
    return (type == PlatformCALayer::LayerTypeTransformLayer) ? kCACFTransformLayer : kCACFLayer;
}

static CFStringRef toCACFFilterType(PlatformCALayer::FilterType type)
{
    switch (type) {
    case PlatformCALayer::Linear: return kCACFFilterLinear;
    case PlatformCALayer::Nearest: return kCACFFilterNearest;
    case PlatformCALayer::Trilinear: return kCACFFilterTrilinear;
    default: return nullptr;
    }
}

static AbstractCACFLayerTreeHost* layerTreeHostForLayer(const PlatformCALayer* layer)
{
    // We need the AbstractCACFLayerTreeHost associated with this layer, which is stored in the UserData of the CACFContext
    void* userData = nullptr;
    if (CACFContextRef context = CACFLayerGetContext(layer->platformLayer()))
        userData = CACFContextGetUserData(context);

    if (!userData)
        return nullptr;

    return static_cast<AbstractCACFLayerTreeHost*>(userData);
}

static PlatformCALayerWinInternal* intern(const PlatformCALayer* layer)
{
    return static_cast<PlatformCALayerWinInternal*>(CACFLayerGetUserData(layer->platformLayer()));
}

static PlatformCALayerWinInternal* intern(void* layer)
{
    return static_cast<PlatformCALayerWinInternal*>(CACFLayerGetUserData(static_cast<CACFLayerRef>(layer)));
}

RefPtr<PlatformCALayer> PlatformCALayer::platformCALayerForLayer(void* platformLayer)
{
    if (!platformLayer)
        return nullptr;
    
    PlatformCALayerWinInternal* layerIntern = intern(platformLayer);
    return layerIntern ? layerIntern->owner() : nullptr;
}

PlatformCALayer::RepaintRectList PlatformCALayer::collectRectsToPaint(GraphicsContext&, PlatformCALayer*)
{
    // FIXME: We should actually collect rects to use instead of defaulting to Windows'
    // normal drawing path.
    PlatformCALayer::RepaintRectList dirtyRects;
    return dirtyRects;
}

void PlatformCALayer::drawLayerContents(GraphicsContext& context, WebCore::PlatformCALayer* platformCALayer, RepaintRectList&, GraphicsLayerPaintBehavior)
{
    intern(platformCALayer)->displayCallback(platformCALayer->platformLayer(), context.platformContext());
}

CGRect PlatformCALayer::frameForLayer(const PlatformLayer* tileLayer)
{
    return CACFLayerGetFrame(static_cast<CACFLayerRef>(const_cast<PlatformLayer*>(tileLayer)));
}

static void displayCallback(CACFLayerRef caLayer, CGContextRef context)
{
    ASSERT_ARG(caLayer, CACFLayerGetUserData(caLayer));
    intern(caLayer)->displayCallback(caLayer, context);
}

static void layoutSublayersProc(CACFLayerRef caLayer) 
{
    auto layer = PlatformCALayer::platformCALayerForLayer(caLayer);
    if (layer && layer->owner())
        layer->owner()->platformCALayerLayoutSublayersOfLayer(layer.get());
}

PlatformCALayerWin::PlatformCALayerWin(LayerType layerType, PlatformLayer* layer, PlatformCALayerClient* owner)
    : PlatformCALayer(layer ? LayerTypeCustom : layerType, owner)
{
    if (layer) {
        m_layer = layer;
        return;
    }

    m_layer = adoptCF(CACFLayerCreate(toCACFLayerType(layerType)));

#if HAVE(CACFLAYER_SETCONTENTSSCALE)
    CACFLayerSetContentsScale(m_layer.get(), owner ? owner->platformCALayerDeviceScaleFactor() : deviceScaleFactorForWindow(nullptr));
#endif

    // Create the PlatformCALayerWinInternal object and point to it in the userdata.
    PlatformCALayerWinInternal* intern = nullptr;

    if (usesTiledBackingLayer()) {
        intern = new WebTiledBackingLayerWin(this);
        TileController* tileController = reinterpret_cast<WebTiledBackingLayerWin*>(intern)->createTileController(this);
        m_customSublayers = makeUnique<PlatformCALayerList>(tileController->containerLayers());
    } else
        intern = new PlatformCALayerWinInternal(this);

    CACFLayerSetUserData(m_layer.get(), intern);

    // Set the display callback
    CACFLayerSetDisplayCallback(m_layer.get(), displayCallback);
    CACFLayerSetLayoutCallback(m_layer.get(), layoutSublayersProc);
}

PlatformCALayerWin::~PlatformCALayerWin()
{
    // Toss all the kids
    removeAllSublayers();

    PlatformCALayerWinInternal* layerIntern = intern(this);
    if (usesTiledBackingLayer())
        reinterpret_cast<WebTiledBackingLayerWin*>(layerIntern)->invalidate();

    // Get rid of the user data
    CACFLayerSetUserData(m_layer.get(), nullptr);

    CACFLayerRemoveFromSuperlayer(m_layer.get());

    delete layerIntern;
}

Ref<PlatformCALayer> PlatformCALayerWin::clone(PlatformCALayerClient* owner) const
{
    PlatformCALayer::LayerType type = (layerType() == PlatformCALayer::LayerTypeTransformLayer) ?
        PlatformCALayer::LayerTypeTransformLayer : PlatformCALayer::LayerTypeLayer;
    auto newLayer = PlatformCALayerWin::create(type, owner);

    newLayer->setPosition(position());
    newLayer->setBounds(bounds());
    newLayer->setAnchorPoint(anchorPoint());
    newLayer->setTransform(transform());
    newLayer->setSublayerTransform(sublayerTransform());
    newLayer->setContents(contents());
    newLayer->setMasksToBounds(masksToBounds());
    newLayer->setDoubleSided(isDoubleSided());
    newLayer->setOpaque(isOpaque());
    newLayer->setBackgroundColor(backgroundColor());
    newLayer->setContentsScale(contentsScale());
    newLayer->copyFiltersFrom(*this);

    return newLayer;
}

PlatformCALayer* PlatformCALayerWin::rootLayer() const
{
    AbstractCACFLayerTreeHost* host = layerTreeHostForLayer(this);
    return host ? host->rootLayer() : nullptr;
}

void PlatformCALayerWin::animationStarted(const String& animationKey, MonotonicTime beginTime)
{
    // Update start time for any animation not yet started
    CFTimeInterval cacfBeginTime = currentTimeToMediaTime(beginTime);

    HashMap<String, RefPtr<PlatformCAAnimation> >::const_iterator end = m_animations.end();
    for (HashMap<String, RefPtr<PlatformCAAnimation> >::const_iterator it = m_animations.begin(); it != end; ++it)
        it->value->setActualStartTimeIfNeeded(cacfBeginTime);

    if (m_owner)
        m_owner->platformCALayerAnimationStarted(animationKey, beginTime);
}

void PlatformCALayerWin::animationEnded(const String& animationKey)
{
    if (m_owner)
        m_owner->platformCALayerAnimationEnded(animationKey);
}

void PlatformCALayerWin::setNeedsDisplayInRect(const FloatRect& dirtyRect)
{
    intern(this)->setNeedsDisplayInRect(dirtyRect);
}

void PlatformCALayerWin::setNeedsDisplay()
{
    intern(this)->setNeedsDisplay();
}

void PlatformCALayerWin::setNeedsCommit()
{
    AbstractCACFLayerTreeHost* host = layerTreeHostForLayer(this);
    if (host)
        host->layerTreeDidChange();
}

void PlatformCALayerWin::copyContentsFromLayer(PlatformCALayer* source)
{
    if (source) {
        RetainPtr<CFTypeRef> contents = CACFLayerGetContents(source->platformLayer());
        CACFLayerSetContents(m_layer.get(), contents.get());
    } else
        CACFLayerSetContents(m_layer.get(), nullptr);

    setNeedsCommit();
}

void PlatformCALayerWin::setNeedsLayout()
{
    if (!m_owner || !m_owner->platformCALayerRespondsToLayoutChanges())
        return;

    CACFLayerSetNeedsLayout(m_layer.get());
    setNeedsCommit();
}

PlatformCALayer* PlatformCALayerWin::superlayer() const
{
    return platformCALayerForLayer(CACFLayerGetSuperlayer(m_layer.get())).get();
}

void PlatformCALayerWin::removeFromSuperlayer()
{
    CACFLayerRemoveFromSuperlayer(m_layer.get());
    setNeedsCommit();
}

void PlatformCALayerWin::setSublayers(const PlatformCALayerList& list)
{
    intern(this)->setSublayers(list);
}

void PlatformCALayerWin::removeAllSublayers()
{
    intern(this)->removeAllSublayers();
}

void PlatformCALayerWin::appendSublayer(PlatformCALayer& layer)
{
    // This must be in terms of insertSublayer instead of a direct call so PlatformCALayerInternal can override.
    insertSublayer(layer, intern(this)->sublayerCount());
}

void PlatformCALayerWin::insertSublayer(PlatformCALayer& layer, size_t index)
{
    intern(this)->insertSublayer(layer, index);
}

void PlatformCALayerWin::replaceSublayer(PlatformCALayer& reference, PlatformCALayer& newLayer)
{
    // This must not use direct calls to allow PlatformCALayerInternal to override.
    ASSERT_ARG(reference, reference.superlayer() == this);

    if (&reference == &newLayer)
        return;

    int referenceIndex = intern(this)->indexOfSublayer(&reference);
    ASSERT(referenceIndex != -1);
    if (referenceIndex == -1)
        return;

    reference.removeFromSuperlayer();

    newLayer.removeFromSuperlayer();
    insertSublayer(newLayer, referenceIndex);
}

void PlatformCALayerWin::adoptSublayers(PlatformCALayer& source)
{
    PlatformCALayerList sublayers;
    intern(&source)->getSublayers(sublayers);

    // Use setSublayers() because it properly nulls out the superlayer pointers.
    setSublayers(sublayers);
}

void PlatformCALayerWin::addAnimationForKey(const String& key, PlatformCAAnimation& animation)
{
    // Add it to the animation list
    m_animations.add(key, &animation);

    CACFLayerAddAnimation(m_layer.get(), key.createCFString().get(), downcast<PlatformCAAnimationWin>(animation).platformAnimation());
    setNeedsCommit();

    // Tell the host about it so we can fire the start animation event
    AbstractCACFLayerTreeHost* host = layerTreeHostForLayer(this);
    if (host)
        host->addPendingAnimatedLayer(*this);
}

void PlatformCALayerWin::removeAnimationForKey(const String& key)
{
    // Remove it from the animation list
    m_animations.remove(key);

    CACFLayerRemoveAnimation(m_layer.get(), key.createCFString().get());

    // We don't "remove" a layer from AbstractCACFLayerTreeHost when it loses an animation.
    // There may be other active animations on the layer and if an animation
    // callback is fired on a layer without any animations no harm is done.

    setNeedsCommit();
}

RefPtr<PlatformCAAnimation> PlatformCALayerWin::animationForKey(const String& key)
{
    HashMap<String, RefPtr<PlatformCAAnimation> >::iterator it = m_animations.find(key);
    if (it == m_animations.end())
        return nullptr;

    return it->value;
}

void PlatformCALayerWin::setMask(PlatformCALayer* layer)
{
    CACFLayerSetMask(m_layer.get(), layer ? layer->platformLayer() : 0);
    setNeedsCommit();
}

bool PlatformCALayerWin::isOpaque() const
{
    return intern(this)->isOpaque();
}

void PlatformCALayerWin::setOpaque(bool value)
{
    intern(this)->setOpaque(value);
    setNeedsCommit();
}

FloatRect PlatformCALayerWin::bounds() const
{
    return CACFLayerGetBounds(m_layer.get());
}

void PlatformCALayerWin::setBounds(const FloatRect& value)
{
    intern(this)->setBounds(value);
    setNeedsLayout();
}

FloatPoint3D PlatformCALayerWin::position() const
{
    CGPoint point = CACFLayerGetPosition(m_layer.get());
    return FloatPoint3D(point.x, point.y, CACFLayerGetZPosition(m_layer.get()));
}

void PlatformCALayerWin::setPosition(const FloatPoint3D& value)
{
    CACFLayerSetPosition(m_layer.get(), CGPointMake(value.x(), value.y()));
    CACFLayerSetZPosition(m_layer.get(), value.z());
    setNeedsCommit();
}

FloatPoint3D PlatformCALayerWin::anchorPoint() const
{
    CGPoint point = CACFLayerGetAnchorPoint(m_layer.get());
    float z = CACFLayerGetAnchorPointZ(m_layer.get());
    return FloatPoint3D(point.x, point.y, z);
}

void PlatformCALayerWin::setAnchorPoint(const FloatPoint3D& value)
{
    CACFLayerSetAnchorPoint(m_layer.get(), CGPointMake(value.x(), value.y()));
    CACFLayerSetAnchorPointZ(m_layer.get(), value.z());
    setNeedsCommit();
}

TransformationMatrix PlatformCALayerWin::transform() const
{
    return CACFLayerGetTransform(m_layer.get());
}

void PlatformCALayerWin::setTransform(const TransformationMatrix& value)
{
    CACFLayerSetTransform(m_layer.get(), value);
    setNeedsCommit();
}

TransformationMatrix PlatformCALayerWin::sublayerTransform() const
{
    return CACFLayerGetSublayerTransform(m_layer.get());
}

void PlatformCALayerWin::setSublayerTransform(const TransformationMatrix& value)
{
    CACFLayerSetSublayerTransform(m_layer.get(), value);
    setNeedsCommit();
}

bool PlatformCALayerWin::isHidden() const
{
    return CACFLayerIsHidden(m_layer.get());
}

void PlatformCALayerWin::setHidden(bool value)
{
    CACFLayerSetHidden(m_layer.get(), value);
    setNeedsCommit();
}

bool PlatformCALayerWin::contentsHidden() const
{
    return false;
}

void PlatformCALayerWin::setContentsHidden(bool)
{
}

void PlatformCALayerWin::setBackingStoreAttached(bool)
{
}

bool PlatformCALayerWin::backingStoreAttached() const
{
    return true;
}

bool PlatformCALayerWin::userInteractionEnabled() const
{
    return true;
}
 
void PlatformCALayerWin::setUserInteractionEnabled(bool)
{
}

bool PlatformCALayerWin::geometryFlipped() const
{
    return CACFLayerIsGeometryFlipped(m_layer.get());
}

void PlatformCALayerWin::setGeometryFlipped(bool value)
{
    CACFLayerSetGeometryFlipped(m_layer.get(), value);
    setNeedsCommit();
}

bool PlatformCALayerWin::isDoubleSided() const
{
    return CACFLayerIsDoubleSided(m_layer.get());
}

void PlatformCALayerWin::setDoubleSided(bool value)
{
    CACFLayerSetDoubleSided(m_layer.get(), value);
    setNeedsCommit();
}

bool PlatformCALayerWin::masksToBounds() const
{
    return CACFLayerGetMasksToBounds(m_layer.get());
}

void PlatformCALayerWin::setMasksToBounds(bool value)
{
    CACFLayerSetMasksToBounds(m_layer.get(), value);
    setNeedsCommit();
}

bool PlatformCALayerWin::acceleratesDrawing() const
{
    return false;
}

void PlatformCALayerWin::setAcceleratesDrawing(bool)
{
}

bool PlatformCALayerWin::wantsDeepColorBackingStore() const
{
    return false;
}

void PlatformCALayerWin::setWantsDeepColorBackingStore(bool)
{
}

bool PlatformCALayerWin::supportsSubpixelAntialiasedText() const
{
    return false;
}

void PlatformCALayerWin::setSupportsSubpixelAntialiasedText(bool)
{
}

bool PlatformCALayerWin::hasContents() const
{
    return !!CACFLayerGetContents(m_layer.get());
}

CFTypeRef PlatformCALayerWin::contents() const
{
    return CACFLayerGetContents(m_layer.get());
}

void PlatformCALayerWin::setContents(CFTypeRef value)
{
    CACFLayerSetContents(m_layer.get(), value);
    setNeedsCommit();
}

void PlatformCALayerWin::setContentsRect(const FloatRect& value)
{
    CACFLayerSetContentsRect(m_layer.get(), value);
    setNeedsCommit();
}

void PlatformCALayerWin::setMinificationFilter(FilterType value)
{
    CACFLayerSetMinificationFilter(m_layer.get(), toCACFFilterType(value));
}

void PlatformCALayerWin::setMagnificationFilter(FilterType value)
{
    CACFLayerSetMagnificationFilter(m_layer.get(), toCACFFilterType(value));
    setNeedsCommit();
}

Color PlatformCALayerWin::backgroundColor() const
{
    return CACFLayerGetBackgroundColor(m_layer.get());
}

void PlatformCALayerWin::setBackgroundColor(const Color& value)
{
    CACFLayerSetBackgroundColor(m_layer.get(), cachedCGColor(value));
    setNeedsCommit();
}

void PlatformCALayerWin::setBorderWidth(float value)
{
    intern(this)->setBorderWidth(value);
    setNeedsCommit();
}

void PlatformCALayerWin::setBorderColor(const Color& value)
{
    intern(this)->setBorderColor(value);
    setNeedsCommit();
}

float PlatformCALayerWin::opacity() const
{
    return CACFLayerGetOpacity(m_layer.get());
}

void PlatformCALayerWin::setOpacity(float value)
{
    CACFLayerSetOpacity(m_layer.get(), value);
    setNeedsCommit();
}

void PlatformCALayerWin::setFilters(const FilterOperations&)
{
}

void PlatformCALayerWin::copyFiltersFrom(const PlatformCALayer&)
{
}

void PlatformCALayerWin::setName(const String& value)
{
    CACFLayerSetName(m_layer.get(), value.createCFString().get());
    setNeedsCommit();
}

void PlatformCALayerWin::setSpeed(float value)
{
    CACFLayerSetSpeed(m_layer.get(), value);
    setNeedsCommit();
}

void PlatformCALayerWin::setTimeOffset(CFTimeInterval value)
{
    CACFLayerSetTimeOffset(m_layer.get(), value);
    setNeedsCommit();
}

void PlatformCALayerWin::setEdgeAntialiasingMask(unsigned mask)
{
    CACFLayerSetEdgeAntialiasingMask(m_layer.get(), mask);
    setNeedsCommit();
}

float PlatformCALayerWin::contentsScale() const
{
    return intern(this)->contentsScale();
}

void PlatformCALayerWin::setContentsScale(float scaleFactor)
{
    intern(this)->setContentsScale(scaleFactor);
    setNeedsCommit();
}

float PlatformCALayerWin::cornerRadius() const
{
    return 0; // FIXME: implement.
}

void PlatformCALayerWin::setCornerRadius(float value)
{
    // FIXME: implement.
}

FloatRoundedRect PlatformCALayerWin::shapeRoundedRect() const
{
    // FIXME: implement.
    return FloatRoundedRect();
}

void PlatformCALayerWin::setShapeRoundedRect(const FloatRoundedRect&)
{
    // FIXME: implement.
}

WindRule PlatformCALayerWin::shapeWindRule() const
{
    // FIXME: implement.
    return WindRule::NonZero;
}

void PlatformCALayerWin::setShapeWindRule(WindRule)
{
    // FIXME: implement.
}

Path PlatformCALayerWin::shapePath() const
{
    // FIXME: implement.
    return Path();
}

void PlatformCALayerWin::setShapePath(const Path&)
{
    // FIXME: implement.
}

static void printIndent(StringBuilder& builder, int indent)
{
    for ( ; indent > 0; --indent)
        builder.append("  ");
}

static void printTransform(StringBuilder& builder, const CATransform3D& transform)
{
    builder.append('[');
    builder.append(FormattedNumber::fixedPrecision(transform.m11));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(transform.m12));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(transform.m13));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(transform.m14));
    builder.append("; ");
    builder.append(FormattedNumber::fixedPrecision(transform.m21));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(transform.m22));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(transform.m23));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(transform.m24));
    builder.append("; ");
    builder.append(FormattedNumber::fixedPrecision(transform.m31));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(transform.m32));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(transform.m33));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(transform.m34));
    builder.append("; ");
    builder.append(FormattedNumber::fixedPrecision(transform.m41));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(transform.m42));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(transform.m43));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(transform.m44));
    builder.append(']');
}

static void printColor(StringBuilder& builder, int indent, const String& label, CGColorRef color)
{
    Color layerColor(color);
    if (!layerColor.isValid())
        return;

    builder.append('\n');
    printIndent(builder, indent);
    builder.append('(', label, ' ', serializationForRenderTreeAsText(layerColor), ')');
}

static void printLayer(StringBuilder& builder, const PlatformCALayer* layer, int indent)
{
    FloatPoint3D layerPosition = layer->position();
    FloatPoint3D layerAnchorPoint = layer->anchorPoint();
    FloatRect layerBounds = layer->bounds();
    builder.append('\n');
    printIndent(builder, indent);

    char* layerTypeName = nullptr;
    switch (layer->layerType()) {
    case PlatformCALayer::LayerTypeLayer: layerTypeName = "layer"; break;
    case PlatformCALayer::LayerTypeWebLayer: layerTypeName = "web-layer"; break;
    case PlatformCALayer::LayerTypeSimpleLayer: layerTypeName = "simple-layer"; break;
    case PlatformCALayer::LayerTypeTransformLayer: layerTypeName = "transform-layer"; break;
    case PlatformCALayer::LayerTypeTiledBackingLayer: layerTypeName = "tiled-backing-layer"; break;
    case PlatformCALayer::LayerTypePageTiledBackingLayer: layerTypeName = "page-tiled-backing-layer"; break;
    case PlatformCALayer::LayerTypeTiledBackingTileLayer: layerTypeName = "tiled-backing-tile-layer"; break;
    case PlatformCALayer::LayerTypeRootLayer: layerTypeName = "root-layer"; break;
    case PlatformCALayer::LayerTypeAVPlayerLayer: layerTypeName = "avplayer-layer"; break;
    case PlatformCALayer::LayerTypeContentsProvidedLayer: layerTypeName = "contents-provided-layer"; break;
    case PlatformCALayer::LayerTypeBackdropLayer: layerTypeName = "backdrop-layer"; break;
    case PlatformCALayer::LayerTypeShapeLayer: layerTypeName = "shape-layer"; break;
    case PlatformCALayer::LayerTypeLightSystemBackdropLayer: layerTypeName = "light-system-backdrop-layer"; break;
    case PlatformCALayer::LayerTypeDarkSystemBackdropLayer: layerTypeName = "dark-system-backdrop-layer"; break;
    case PlatformCALayer::LayerTypeScrollContainerLayer: layerTypeName = "scroll-container-layer"; break;
    case PlatformCALayer::LayerTypeCustom: layerTypeName = "custom-layer"; break;
    }

    builder.append("(");
    builder.append(layerTypeName);
    builder.append(" [");
    builder.append(FormattedNumber::fixedPrecision(layerPosition.x()));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(layerPosition.y()));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(layerPosition.z()));
    builder.append("] [");
    builder.append(FormattedNumber::fixedPrecision(layerBounds.x()));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(layerBounds.y()));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(layerBounds.width()));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(layerBounds.height()));
    builder.append("] [");
    builder.append(FormattedNumber::fixedPrecision(layerAnchorPoint.x()));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(layerAnchorPoint.y()));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(layerAnchorPoint.z()));
    builder.append("] superlayer=");
    builder.appendNumber(reinterpret_cast<unsigned long long>(layer->superlayer()));

    // Print name if needed
    String layerName = CACFLayerGetName(layer->platformLayer());
    if (!layerName.isEmpty()) {
        builder.append('\n');
        printIndent(builder, indent + 1);
        builder.append("(name \"");
        builder.append(layerName);
        builder.append("\")");
    }

    // Print borderWidth if needed
    if (CGFloat borderWidth = CACFLayerGetBorderWidth(layer->platformLayer())) {
        builder.append('\n');
        printIndent(builder, indent + 1);
        builder.append("(borderWidth ");
        builder.append(FormattedNumber::fixedPrecision(borderWidth));
        builder.append(')');
    }

    // Print backgroundColor if needed
    printColor(builder, indent + 1, "backgroundColor", CACFLayerGetBackgroundColor(layer->platformLayer()));

    // Print borderColor if needed
    printColor(builder, indent + 1, "borderColor", CACFLayerGetBorderColor(layer->platformLayer()));

    // Print masksToBounds if needed
    if (bool layerMasksToBounds = layer->masksToBounds()) {
        builder.append('\n');
        printIndent(builder, indent + 1);
        builder.append("(masksToBounds true)");
    }

    if (bool geometryFlipped = layer->geometryFlipped()) {
        builder.append('\n');
        printIndent(builder, indent + 1);
        builder.append("(geometryFlipped true)");
    }

    // Print opacity if needed
    float layerOpacity = layer->opacity();
    if (layerOpacity != 1) {
        builder.append('\n');
        printIndent(builder, indent + 1);
        builder.append("(opacity ");
        builder.append(FormattedNumber::fixedPrecision(layerOpacity));
        builder.append(')');
    }

    // Print sublayerTransform if needed
    TransformationMatrix layerTransform = layer->sublayerTransform();
    if (!layerTransform.isIdentity()) {
        builder.append('\n');
        printIndent(builder, indent + 1);
        builder.append("(sublayerTransform ");
        printTransform(builder, layerTransform);
        builder.append(')');
    }

    // Print transform if needed
    layerTransform = layer->transform();
    if (!layerTransform.isIdentity()) {
        builder.append('\n');
        printIndent(builder, indent + 1);
        builder.append("(transform ");
        printTransform(builder, layerTransform);
        builder.append(')');
    }

    // Print contents if needed
    if (CFTypeRef layerContents = layer->contents()) {
        if (CFGetTypeID(layerContents) == CGImageGetTypeID()) {
            CGImageRef imageContents = static_cast<CGImageRef>(const_cast<void*>(layerContents));
            builder.append('\n');
            printIndent(builder, indent + 1);
            builder.append("(contents (image [");
            builder.appendNumber(CGImageGetWidth(imageContents));
            builder.append(' ');
            builder.appendNumber(CGImageGetHeight(imageContents));
            builder.append("]))");
        }

        if (CFGetTypeID(layerContents) == CABackingStoreGetTypeID()) {
            CABackingStoreRef backingStore = static_cast<CABackingStoreRef>(const_cast<void*>(layerContents));
            CGImageRef imageContents = CABackingStoreGetCGImage(backingStore);
            builder.append('\n');
            printIndent(builder, indent + 1);
            builder.append("(contents (backing-store [");
            builder.appendNumber(CGImageGetWidth(imageContents));
            builder.append(' ');
            builder.appendNumber(CGImageGetHeight(imageContents));
            builder.append("]))");
        }
    }

    // Print sublayers if needed
    int n = intern(layer)->sublayerCount();
    if (n > 0) {
        builder.append('\n');
        printIndent(builder, indent + 1);
        builder.append("(sublayers");

        PlatformCALayerList sublayers;
        intern(layer)->getSublayers(sublayers);
        ASSERT(n == sublayers.size());
        for (int i = 0; i < n; ++i)
            printLayer(builder, sublayers[i].get(), indent + 2);

        builder.append(')');
    }

    builder.append(')');
}

String PlatformCALayerWin::layerTreeAsString() const
{
    // Print heading info
    CGRect rootBounds = bounds();

    StringBuilder builder;
    builder.append("\n\n** Render tree at time ");
    builder.append(FormattedNumber::fixedPrecision(MonotonicTime::now().secondsSinceEpoch().seconds()));
    builder.append(" (bounds ");
    builder.append(FormattedNumber::fixedPrecision(rootBounds.origin.x));
    builder.append(", ");
    builder.append(FormattedNumber::fixedPrecision(rootBounds.origin.y));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(rootBounds.size.width));
    builder.append('x');
    builder.append(FormattedNumber::fixedPrecision(rootBounds.size.height));
    builder.append(") **\n\n");

    // Print layer tree from the root
    printLayer(builder, this, 0);

    return builder.toString();
}

Ref<PlatformCALayer> PlatformCALayerWin::createCompatibleLayer(PlatformCALayer::LayerType layerType, PlatformCALayerClient* client) const
{
    return PlatformCALayerWin::create(layerType, client);
}

TiledBacking* PlatformCALayerWin::tiledBacking()
{
    if (!usesTiledBackingLayer())
        return nullptr;

    return reinterpret_cast<WebTiledBackingLayerWin*>(intern(this))->tiledBacking();
}

void PlatformCALayerWin::drawTextAtPoint(CGContextRef context, CGFloat x, CGFloat y, CGSize scale, CGFloat fontSize, const char* message, size_t length, CGFloat, Color) const
{
    String text(message, length);

    FontCascadeDescription desc;

    NONCLIENTMETRICS metrics;
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0);
    desc.setOneFamily(metrics.lfSmCaptionFont.lfFaceName);

    desc.setComputedSize(scale.width * fontSize);

    FontCascade font = FontCascade(WTFMove(desc), 0, 0);
    font.update(nullptr);

    GraphicsContext cg(context);
    cg.setFillColor(Color::black);
    cg.drawText(font, TextRun(text), IntPoint(x, y));
}

#endif
