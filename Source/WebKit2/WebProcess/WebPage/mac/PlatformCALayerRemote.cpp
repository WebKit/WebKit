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

#import "PlatformCALayerRemote.h"

#import "RemoteLayerTreeContext.h"
#import <WebCore/AnimationUtilities.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/GraphicsLayerCA.h>
#import <WebCore/LengthFunctions.h>
#import <WebCore/PlatformCAFilters.h>
#import <WebCore/TiledBacking.h>
#import <wtf/CurrentTime.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;
using namespace WebKit;

static RemoteLayerTreeTransaction::LayerID generateLayerID()
{
    static RemoteLayerTreeTransaction::LayerID layerID;
    return ++layerID;
}

PassRefPtr<PlatformCALayer> PlatformCALayerRemote::create(LayerType layerType, PlatformCALayerClient* owner, RemoteLayerTreeContext* context)
{
    return adoptRef(new PlatformCALayerRemote(layerType, owner, context));
}

PlatformCALayerRemote::PlatformCALayerRemote(LayerType layerType, PlatformCALayerClient* owner, RemoteLayerTreeContext* context)
    : PlatformCALayer(layerType, owner)
    , m_layerID(generateLayerID())
    , m_context(context)
{
    m_context->layerWasCreated(this, layerType);
}

PassRefPtr<PlatformCALayer> PlatformCALayerRemote::clone(PlatformCALayerClient* owner) const
{
    return nullptr;
}

PlatformCALayerRemote::~PlatformCALayerRemote()
{
    m_context->layerWillBeDestroyed(this);
}

void PlatformCALayerRemote::recursiveBuildTransaction(RemoteLayerTreeTransaction& transaction)
{
    if (m_properties.changedProperties != RemoteLayerTreeTransaction::NoChange) {
        if (m_properties.changedProperties & RemoteLayerTreeTransaction::ChildrenChanged) {
            m_properties.children.clear();
            for (auto layer : m_children)
                m_properties.children.append(static_cast<PlatformCALayerRemote*>(layer.get())->layerID());
        }

        transaction.layerPropertiesChanged(this, m_properties);
        m_properties.changedProperties = RemoteLayerTreeTransaction::NoChange;
    }

    for (size_t i = 0; i < m_children.size(); ++i) {
        PlatformCALayerRemote* child = static_cast<PlatformCALayerRemote*>(m_children[i].get());
        child->recursiveBuildTransaction(transaction);
    }
}

void PlatformCALayerRemote::animationStarted(CFTimeInterval beginTime)
{
}

void PlatformCALayerRemote::setNeedsDisplay(const FloatRect* dirtyRect)
{
}

void PlatformCALayerRemote::setContentsChanged()
{
}

PlatformCALayer* PlatformCALayerRemote::superlayer() const
{
    return nullptr;
}

void PlatformCALayerRemote::removeFromSuperlayer()
{
    ASSERT_NOT_REACHED();
}

void PlatformCALayerRemote::setSublayers(const PlatformCALayerList& list)
{
    m_children = list;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::removeAllSublayers()
{
    m_children.clear();
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::appendSublayer(PlatformCALayer* layer)
{
    m_children.append(layer);
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::insertSublayer(PlatformCALayer* layer, size_t index)
{
    m_children.insert(index, layer);
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::replaceSublayer(PlatformCALayer* reference, PlatformCALayer* layer)
{
    ASSERT_NOT_REACHED();
}

void PlatformCALayerRemote::adoptSublayers(PlatformCALayer* source)
{
    ASSERT_NOT_REACHED();
}

void PlatformCALayerRemote::addAnimationForKey(const String& key, PlatformCAAnimation* animation)
{
}

void PlatformCALayerRemote::removeAnimationForKey(const String& key)
{
}

PassRefPtr<PlatformCAAnimation> PlatformCALayerRemote::animationForKey(const String& key)
{
    return nullptr;
}

void PlatformCALayerRemote::setMask(PlatformCALayer* layer)
{
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
    m_properties.size = value.size();
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::SizeChanged);
}

FloatPoint3D PlatformCALayerRemote::position() const
{
    return m_properties.position;
}

void PlatformCALayerRemote::setPosition(const FloatPoint3D& value)
{
    m_properties.position = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::PositionChanged);
}

FloatPoint3D PlatformCALayerRemote::anchorPoint() const
{
    return m_properties.anchorPoint;
}

void PlatformCALayerRemote::setAnchorPoint(const FloatPoint3D& value)
{
    m_properties.anchorPoint = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::AnchorPointChanged);
}

TransformationMatrix PlatformCALayerRemote::transform() const
{
    return m_properties.transform;
}

void PlatformCALayerRemote::setTransform(const TransformationMatrix& value)
{
    m_properties.transform = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::TransformChanged);
}

TransformationMatrix PlatformCALayerRemote::sublayerTransform() const
{
    return m_properties.sublayerTransform;
}

void PlatformCALayerRemote::setSublayerTransform(const TransformationMatrix& value)
{
    m_properties.sublayerTransform = value;
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
    m_properties.masksToBounds = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::MasksToBoundsChanged);
}

bool PlatformCALayerRemote::acceleratesDrawing() const
{
    return false;
}

void PlatformCALayerRemote::setAcceleratesDrawing(bool acceleratesDrawing)
{
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
}

void PlatformCALayerRemote::setMinificationFilter(FilterType value)
{
}

void PlatformCALayerRemote::setMagnificationFilter(FilterType value)
{
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
    m_properties.borderWidth = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::BorderWidthChanged);
}

void PlatformCALayerRemote::setBorderColor(const Color& value)
{
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
}

void PlatformCALayerRemote::copyFiltersFrom(const PlatformCALayer* sourceLayer)
{
}

bool PlatformCALayerRemote::filtersCanBeComposited(const FilterOperations& filters)
{
    return false; // This will probably work the same as Mac eventually?
}
#endif

void PlatformCALayerRemote::setName(const String& value)
{
    m_properties.name = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::NameChanged);
}

void PlatformCALayerRemote::setFrame(const FloatRect& value)
{
}

void PlatformCALayerRemote::setSpeed(float value)
{
}

void PlatformCALayerRemote::setTimeOffset(CFTimeInterval value)
{
}

float PlatformCALayerRemote::contentsScale() const
{
    return 0;
}

void PlatformCALayerRemote::setContentsScale(float value)
{
}

TiledBacking* PlatformCALayerRemote::tiledBacking()
{
    return nullptr;
}

void PlatformCALayerRemote::synchronouslyDisplayTilesInRect(const FloatRect& rect)
{
}

AVPlayerLayer* PlatformCALayerRemote::playerLayer() const
{
    return nullptr;
}

#endif // USE(ACCELERATED_COMPOSITING)
