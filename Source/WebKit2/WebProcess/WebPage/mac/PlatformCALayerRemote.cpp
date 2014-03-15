/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "PlatformCALayerRemote.h"

#import "PlatformCALayerRemoteCustom.h"
#import "PlatformCALayerRemoteTiledBacking.h"
#import "RemoteLayerBackingStore.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreePropertyApplier.h"
#import <WebCore/AnimationUtilities.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/GraphicsLayerCA.h>
#import <WebCore/LengthFunctions.h>
#import <WebCore/PlatformCAFilters.h>
#import <WebCore/PlatformCALayerMac.h>
#import <WebCore/TiledBacking.h>
#import <wtf/CurrentTime.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;
using namespace WebKit;

PassRefPtr<PlatformCALayerRemote> PlatformCALayerRemote::create(LayerType layerType, PlatformCALayerClient* owner, RemoteLayerTreeContext* context)
{
    RefPtr<PlatformCALayerRemote> layer;

    if (layerType == LayerTypeTiledBackingLayer ||  layerType == LayerTypePageTiledBackingLayer)
        layer = adoptRef(new PlatformCALayerRemoteTiledBacking(layerType, owner, context));
    else
        layer = adoptRef(new PlatformCALayerRemote(layerType, owner, context));

    context->layerWasCreated(layer.get(), layerType);

    return layer.release();
}

PassRefPtr<PlatformCALayerRemote> PlatformCALayerRemote::create(PlatformLayer *platformLayer, PlatformCALayerClient* owner, RemoteLayerTreeContext* context)
{
    RefPtr<PlatformCALayerRemote> layer = adoptRef(new PlatformCALayerRemoteCustom(static_cast<PlatformLayer*>(platformLayer), owner, context));

    context->layerWasCreated(layer.get(), LayerTypeCustom);

    return layer.release();
}

PassRefPtr<PlatformCALayerRemote> PlatformCALayerRemote::create(const PlatformCALayerRemote& other, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext* context)
{
    RefPtr<PlatformCALayerRemote> layer = adoptRef(new PlatformCALayerRemote(other, owner, context));

    context->layerWasCreated(layer.get(), LayerTypeCustom);

    return layer.release();
}

PlatformCALayerRemote::PlatformCALayerRemote(LayerType layerType, PlatformCALayerClient* owner, RemoteLayerTreeContext* context)
    : PlatformCALayer(layerType, owner)
    , m_superlayer(nullptr)
    , m_maskLayer(nullptr)
    , m_acceleratesDrawing(false)
    , m_context(context)
{
}

PlatformCALayerRemote::PlatformCALayerRemote(const PlatformCALayerRemote& other, PlatformCALayerClient* owner, RemoteLayerTreeContext* context)
    : PlatformCALayer(other.layerType(), owner)
    , m_properties(other.m_properties)
    , m_superlayer(nullptr)
    , m_maskLayer(nullptr)
    , m_acceleratesDrawing(other.acceleratesDrawing())
    , m_context(context)
{
}

PassRefPtr<PlatformCALayer> PlatformCALayerRemote::clone(PlatformCALayerClient* client) const
{
    RefPtr<PlatformCALayerRemote> clone = PlatformCALayerRemote::create(*this, client, m_context);

    clone->m_properties.notePropertiesChanged(static_cast<RemoteLayerTreeTransaction::LayerChange>(m_properties.everChangedProperties & ~RemoteLayerTreeTransaction::BackingStoreChanged));

    return clone.release();
}

PlatformCALayerRemote::~PlatformCALayerRemote()
{
    for (const auto& layer : m_children)
        toPlatformCALayerRemote(layer.get())->m_superlayer = nullptr;
    m_context->layerWillBeDestroyed(this);
}

void PlatformCALayerRemote::recursiveBuildTransaction(RemoteLayerTreeTransaction& transaction)
{
    if (m_properties.backingStore && m_properties.backingStore->display())
        m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::BackingStoreChanged);

    if (m_properties.changedProperties != RemoteLayerTreeTransaction::NoChange) {
        if (m_properties.changedProperties & RemoteLayerTreeTransaction::ChildrenChanged) {
            m_properties.children.clear();
            for (const auto& layer : m_children)
                m_properties.children.append(layer->layerID());
        }

        if (m_layerType == LayerTypeCustom) {
            RemoteLayerTreePropertyApplier::applyProperties(platformLayer(), m_properties, RemoteLayerTreePropertyApplier::RelatedLayerMap());
            m_properties.changedProperties = RemoteLayerTreeTransaction::NoChange;
            return;
        }

        transaction.layerPropertiesChanged(this, m_properties);
        m_properties.changedProperties = RemoteLayerTreeTransaction::NoChange;
    }

    for (size_t i = 0; i < m_children.size(); ++i) {
        PlatformCALayerRemote* child = toPlatformCALayerRemote(m_children[i].get());
        ASSERT(child->superlayer() == this);
        child->recursiveBuildTransaction(transaction);
    }

    if (m_maskLayer)
        m_maskLayer->recursiveBuildTransaction(transaction);
}

void PlatformCALayerRemote::animationStarted(CFTimeInterval beginTime)
{
}

void PlatformCALayerRemote::ensureBackingStore()
{
    if (!m_properties.backingStore)
        m_properties.backingStore = std::make_unique<RemoteLayerBackingStore>();

    updateBackingStore();
}

void PlatformCALayerRemote::updateBackingStore()
{
    if (!m_properties.backingStore)
        return;

    m_properties.backingStore->ensureBackingStore(this, expandedIntSize(m_properties.size), m_properties.contentsScale, m_acceleratesDrawing);
}

void PlatformCALayerRemote::setNeedsDisplay(const FloatRect* rect)
{
    ensureBackingStore();

    if (!rect) {
        m_properties.backingStore->setNeedsDisplay();
        return;
    }

    // FIXME: Need to map this through contentsRect/etc.
    m_properties.backingStore->setNeedsDisplay(enclosingIntRect(*rect));
}

void PlatformCALayerRemote::setContentsChanged()
{
}

PlatformCALayer* PlatformCALayerRemote::superlayer() const
{
    return m_superlayer;
}

void PlatformCALayerRemote::removeFromSuperlayer()
{
    if (!m_superlayer)
        return;

    m_superlayer->removeSublayer(this);
}

void PlatformCALayerRemote::removeSublayer(PlatformCALayerRemote* layer)
{
    size_t childIndex = m_children.find(layer);
    if (childIndex != notFound)
        m_children.remove(childIndex);
    layer->m_superlayer = nullptr;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::setSublayers(const PlatformCALayerList& list)
{
    removeAllSublayers();
    m_children = list;

    for (const auto& layer : list) {
        layer->removeFromSuperlayer();
        toPlatformCALayerRemote(layer.get())->m_superlayer = this;
    }

    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::removeAllSublayers()
{
    PlatformCALayerList layersToRemove = m_children;
    for (const auto& layer : layersToRemove)
        layer->removeFromSuperlayer();
    ASSERT(m_children.isEmpty());
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::appendSublayer(PlatformCALayer* layer)
{
    RefPtr<PlatformCALayer> layerProtector(layer);

    layer->removeFromSuperlayer();
    m_children.append(layer);
    toPlatformCALayerRemote(layer)->m_superlayer = this;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::insertSublayer(PlatformCALayer* layer, size_t index)
{
    RefPtr<PlatformCALayer> layerProtector(layer);

    layer->removeFromSuperlayer();
    m_children.insert(index, layer);
    toPlatformCALayerRemote(layer)->m_superlayer = this;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::replaceSublayer(PlatformCALayer* reference, PlatformCALayer* layer)
{
    ASSERT(reference->superlayer() == this);
    RefPtr<PlatformCALayer> layerProtector(layer);

    layer->removeFromSuperlayer();
    size_t referenceIndex = m_children.find(reference);
    if (referenceIndex != notFound) {
        m_children[referenceIndex]->removeFromSuperlayer();
        m_children.insert(referenceIndex, layer);
        toPlatformCALayerRemote(layer)->m_superlayer = this;
    }

    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::adoptSublayers(PlatformCALayer* source)
{
    setSublayers(toPlatformCALayerRemote(source)->m_children);
}

void PlatformCALayerRemote::addAnimationForKey(const String& key, PlatformCAAnimation* animation)
{
    ASSERT_NOT_REACHED();
}

void PlatformCALayerRemote::removeAnimationForKey(const String& key)
{
    ASSERT_NOT_REACHED();
}

PassRefPtr<PlatformCAAnimation> PlatformCALayerRemote::animationForKey(const String& key)
{
    ASSERT_NOT_REACHED();

    return nullptr;
}

void PlatformCALayerRemote::setMask(PlatformCALayer* layer)
{
    if (layer) {
        m_maskLayer = toPlatformCALayerRemote(layer);
        m_properties.maskLayerID = m_maskLayer->layerID();
    } else {
        m_maskLayer = nullptr;
        m_properties.maskLayerID = 0;
    }

    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::MaskLayerChanged);
}

bool PlatformCALayerRemote::isOpaque() const
{
    return m_properties.opaque;
}

void PlatformCALayerRemote::setOpaque(bool value)
{
    m_properties.opaque = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::OpaqueChanged);
}

FloatRect PlatformCALayerRemote::bounds() const
{
    return FloatRect(FloatPoint(), m_properties.size);
}

void PlatformCALayerRemote::setBounds(const FloatRect& value)
{
    if (value.size() == m_properties.size)
        return;

    m_properties.size = value.size();
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::SizeChanged);
    
    if (requiresCustomAppearanceUpdateOnBoundsChange())
        m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::CustomAppearanceChanged);

    updateBackingStore();
}

FloatPoint3D PlatformCALayerRemote::position() const
{
    return m_properties.position;
}

void PlatformCALayerRemote::setPosition(const FloatPoint3D& value)
{
    if (value == m_properties.position)
        return;

    m_properties.position = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::PositionChanged);
}

FloatPoint3D PlatformCALayerRemote::anchorPoint() const
{
    return m_properties.anchorPoint;
}

void PlatformCALayerRemote::setAnchorPoint(const FloatPoint3D& value)
{
    if (value == m_properties.anchorPoint)
        return;

    m_properties.anchorPoint = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::AnchorPointChanged);
}

TransformationMatrix PlatformCALayerRemote::transform() const
{
    return m_properties.transform ? *m_properties.transform : TransformationMatrix();
}

void PlatformCALayerRemote::setTransform(const TransformationMatrix& value)
{
    m_properties.transform = std::make_unique<TransformationMatrix>(value);
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::TransformChanged);
}

TransformationMatrix PlatformCALayerRemote::sublayerTransform() const
{
    return m_properties.sublayerTransform ? *m_properties.sublayerTransform : TransformationMatrix();
}

void PlatformCALayerRemote::setSublayerTransform(const TransformationMatrix& value)
{
    m_properties.sublayerTransform = std::make_unique<TransformationMatrix>(value);
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::SublayerTransformChanged);
}

void PlatformCALayerRemote::setHidden(bool value)
{
    m_properties.hidden = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::HiddenChanged);
}

void PlatformCALayerRemote::setGeometryFlipped(bool value)
{
    m_properties.geometryFlipped = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::GeometryFlippedChanged);
}

bool PlatformCALayerRemote::isDoubleSided() const
{
    return m_properties.doubleSided;
}

void PlatformCALayerRemote::setDoubleSided(bool value)
{
    m_properties.doubleSided = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::DoubleSidedChanged);
}

bool PlatformCALayerRemote::masksToBounds() const
{
    return m_properties.masksToBounds;
}

void PlatformCALayerRemote::setMasksToBounds(bool value)
{
    if (value == m_properties.masksToBounds)
        return;

    m_properties.masksToBounds = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::MasksToBoundsChanged);
}

bool PlatformCALayerRemote::acceleratesDrawing() const
{
    return m_acceleratesDrawing;
}

void PlatformCALayerRemote::setAcceleratesDrawing(bool acceleratesDrawing)
{
    m_acceleratesDrawing = acceleratesDrawing;
    updateBackingStore();
}

CFTypeRef PlatformCALayerRemote::contents() const
{
    return nullptr;
}

void PlatformCALayerRemote::setContents(CFTypeRef value)
{
}

void PlatformCALayerRemote::setContentsRect(const FloatRect& value)
{
    m_properties.contentsRect = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ContentsRectChanged);
}

void PlatformCALayerRemote::setMinificationFilter(FilterType value)
{
    m_properties.minificationFilter = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::MinificationFilterChanged);
}

void PlatformCALayerRemote::setMagnificationFilter(FilterType value)
{
    m_properties.magnificationFilter = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::MagnificationFilterChanged);
}

Color PlatformCALayerRemote::backgroundColor() const
{
    return m_properties.backgroundColor;
}

void PlatformCALayerRemote::setBackgroundColor(const Color& value)
{
    m_properties.backgroundColor = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::BackgroundColorChanged);
}

void PlatformCALayerRemote::setBorderWidth(float value)
{
    if (value == m_properties.borderWidth)
        return;

    m_properties.borderWidth = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::BorderWidthChanged);
}

void PlatformCALayerRemote::setBorderColor(const Color& value)
{
    if (value == m_properties.borderColor)
        return;

    m_properties.borderColor = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::BorderColorChanged);
}

float PlatformCALayerRemote::opacity() const
{
    return m_properties.opacity;
}

void PlatformCALayerRemote::setOpacity(float value)
{
    m_properties.opacity = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::OpacityChanged);
}

#if ENABLE(CSS_FILTERS)
void PlatformCALayerRemote::setFilters(const FilterOperations& filters)
{
    m_properties.filters = std::make_unique<FilterOperations>(filters);
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::FiltersChanged);
}

void PlatformCALayerRemote::copyFiltersFrom(const PlatformCALayer* sourceLayer)
{
    ASSERT_NOT_REACHED();
}

bool PlatformCALayerRemote::filtersCanBeComposited(const FilterOperations& filters)
{
    return PlatformCALayerMac::filtersCanBeComposited(filters);
}
#endif

void PlatformCALayerRemote::setName(const String& value)
{
    m_properties.name = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::NameChanged);
}

void PlatformCALayerRemote::setSpeed(float value)
{
    m_properties.speed = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::SpeedChanged);
}

void PlatformCALayerRemote::setTimeOffset(CFTimeInterval value)
{
    m_properties.timeOffset = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::TimeOffsetChanged);
}

float PlatformCALayerRemote::contentsScale() const
{
    return m_properties.contentsScale;
}

void PlatformCALayerRemote::setContentsScale(float value)
{
    m_properties.contentsScale = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ContentsScaleChanged);

    updateBackingStore();
}

void PlatformCALayerRemote::setEdgeAntialiasingMask(unsigned value)
{
    m_properties.edgeAntialiasingMask = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::EdgeAntialiasingMaskChanged);
}

bool PlatformCALayerRemote::requiresCustomAppearanceUpdateOnBoundsChange() const
{
    return m_properties.customAppearance == GraphicsLayer::ScrollingShadow;
}

GraphicsLayer::CustomAppearance PlatformCALayerRemote::customAppearance() const
{
    return m_properties.customAppearance;
}

void PlatformCALayerRemote::updateCustomAppearance(GraphicsLayer::CustomAppearance customAppearance)
{
    m_properties.customAppearance = customAppearance;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::CustomAppearanceChanged);
}

PassRefPtr<PlatformCALayer> PlatformCALayerRemote::createCompatibleLayer(PlatformCALayer::LayerType layerType, PlatformCALayerClient* client) const
{
    return PlatformCALayerRemote::create(layerType, client, m_context);
}

void PlatformCALayerRemote::enumerateRectsBeingDrawn(CGContextRef context, void (^block)(CGRect))
{
    m_properties.backingStore->enumerateRectsBeingDrawn(context, block);
}

uint32_t PlatformCALayerRemote::hostingContextID()
{
    ASSERT_NOT_REACHED();
    return 0;
}
