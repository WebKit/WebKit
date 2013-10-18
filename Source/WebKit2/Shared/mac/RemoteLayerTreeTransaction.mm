/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RemoteLayerTreeTransaction.h"

#import "ArgumentCoders.h"
#import "MessageDecoder.h"
#import "MessageEncoder.h"
#import "PlatformCALayerRemote.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/TextStream.h>
#import <wtf/text/CString.h>
#import <wtf/text/StringBuilder.h>

using namespace WebCore;

namespace WebKit {

RemoteLayerTreeTransaction::LayerCreationProperties::LayerCreationProperties()
{
}

void RemoteLayerTreeTransaction::LayerCreationProperties::encode(CoreIPC::ArgumentEncoder& encoder) const
{
    encoder << layerID;
    encoder.encodeEnum(type);
}

bool RemoteLayerTreeTransaction::LayerCreationProperties::decode(CoreIPC::ArgumentDecoder& decoder, LayerCreationProperties& result)
{
    if (!decoder.decode(result.layerID))
        return false;

    if (!decoder.decodeEnum(result.type))
        return false;

    return true;
}

RemoteLayerTreeTransaction::LayerProperties::LayerProperties()
    : changedProperties(NoChange)
{
}

void RemoteLayerTreeTransaction::LayerProperties::encode(CoreIPC::ArgumentEncoder& encoder) const
{
    encoder.encodeEnum(changedProperties);

    if (changedProperties & NameChanged)
        encoder << name;

    if (changedProperties & ChildrenChanged)
        encoder << children;

    if (changedProperties & PositionChanged)
        encoder << position;

    if (changedProperties & SizeChanged)
        encoder << size;

    if (changedProperties & BackgroundColorChanged)
        encoder << backgroundColor;

    if (changedProperties & AnchorPointChanged)
        encoder << anchorPoint;

    if (changedProperties & BorderWidthChanged)
        encoder << borderWidth;

    if (changedProperties & BorderColorChanged)
        encoder << borderColor;

    if (changedProperties & OpacityChanged)
        encoder << opacity;

    if (changedProperties & TransformChanged)
        encoder << transform;

    if (changedProperties & SublayerTransformChanged)
        encoder << sublayerTransform;

    if (changedProperties & HiddenChanged)
        encoder << hidden;

    if (changedProperties & GeometryFlippedChanged)
        encoder << geometryFlipped;

    if (changedProperties & DoubleSidedChanged)
        encoder << doubleSided;

    if (changedProperties & MasksToBoundsChanged)
        encoder << masksToBounds;

    if (changedProperties & OpaqueChanged)
        encoder << opaque;

    if (changedProperties & MaskLayerChanged)
        encoder << maskLayer;

    if (changedProperties & ContentsRectChanged)
        encoder << contentsRect;

    if (changedProperties & ContentsScaleChanged)
        encoder << contentsScale;

    if (changedProperties & MinificationFilterChanged)
        encoder.encodeEnum(minificationFilter);

    if (changedProperties & MagnificationFilterChanged)
        encoder.encodeEnum(magnificationFilter);

    if (changedProperties & SpeedChanged)
        encoder << speed;

    if (changedProperties & TimeOffsetChanged)
        encoder << timeOffset;

    if (changedProperties & BackingStoreChanged)
        encoder << backingStore;
}

bool RemoteLayerTreeTransaction::LayerProperties::decode(CoreIPC::ArgumentDecoder& decoder, LayerProperties& result)
{
    if (!decoder.decodeEnum(result.changedProperties))
        return false;

    if (result.changedProperties & NameChanged) {
        if (!decoder.decode(result.name))
            return false;
    }

    if (result.changedProperties & ChildrenChanged) {
        if (!decoder.decode(result.children))
            return false;

        for (auto layerID : result.children) {
            if (!layerID)
                return false;
        }
    }

    if (result.changedProperties & PositionChanged) {
        if (!decoder.decode(result.position))
            return false;
    }

    if (result.changedProperties & SizeChanged) {
        if (!decoder.decode(result.size))
            return false;
    }

    if (result.changedProperties & BackgroundColorChanged) {
        if (!decoder.decode(result.backgroundColor))
            return false;
    }

    if (result.changedProperties & AnchorPointChanged) {
        if (!decoder.decode(result.anchorPoint))
            return false;
    }

    if (result.changedProperties & BorderWidthChanged) {
        if (!decoder.decode(result.borderWidth))
            return false;
    }

    if (result.changedProperties & BorderColorChanged) {
        if (!decoder.decode(result.borderColor))
            return false;
    }

    if (result.changedProperties & OpacityChanged) {
        if (!decoder.decode(result.opacity))
            return false;
    }

    if (result.changedProperties & TransformChanged) {
        if (!decoder.decode(result.transform))
            return false;
    }

    if (result.changedProperties & SublayerTransformChanged) {
        if (!decoder.decode(result.sublayerTransform))
            return false;
    }

    if (result.changedProperties & HiddenChanged) {
        if (!decoder.decode(result.hidden))
            return false;
    }

    if (result.changedProperties & GeometryFlippedChanged) {
        if (!decoder.decode(result.geometryFlipped))
            return false;
    }

    if (result.changedProperties & DoubleSidedChanged) {
        if (!decoder.decode(result.doubleSided))
            return false;
    }

    if (result.changedProperties & MasksToBoundsChanged) {
        if (!decoder.decode(result.masksToBounds))
            return false;
    }

    if (result.changedProperties & OpaqueChanged) {
        if (!decoder.decode(result.opaque))
            return false;
    }

    if (result.changedProperties & MaskLayerChanged) {
        if (!decoder.decode(result.maskLayer))
            return false;
    }

    if (result.changedProperties & ContentsRectChanged) {
        if (!decoder.decode(result.contentsRect))
            return false;
    }

    if (result.changedProperties & ContentsScaleChanged) {
        if (!decoder.decode(result.contentsScale))
            return false;
    }

    if (result.changedProperties & MinificationFilterChanged) {
        if (!decoder.decodeEnum(result.minificationFilter))
            return false;
    }

    if (result.changedProperties & MagnificationFilterChanged) {
        if (!decoder.decodeEnum(result.magnificationFilter))
            return false;
    }

    if (result.changedProperties & SpeedChanged) {
        if (!decoder.decode(result.speed))
            return false;
    }

    if (result.changedProperties & TimeOffsetChanged) {
        if (!decoder.decode(result.timeOffset))
            return false;
    }

    if (result.changedProperties & BackingStoreChanged) {
        if (!decoder.decode(result.backingStore))
            return false;
    }

    return true;
}

RemoteLayerTreeTransaction::RemoteLayerTreeTransaction()
{
}

RemoteLayerTreeTransaction::~RemoteLayerTreeTransaction()
{
}

void RemoteLayerTreeTransaction::encode(CoreIPC::ArgumentEncoder& encoder) const
{
    encoder << m_rootLayerID;
    encoder << m_createdLayers;
    encoder << m_changedLayerProperties;
    encoder << m_destroyedLayerIDs;
}

bool RemoteLayerTreeTransaction::decode(CoreIPC::ArgumentDecoder& decoder, RemoteLayerTreeTransaction& result)
{
    if (!decoder.decode(result.m_rootLayerID))
        return false;
    if (!result.m_rootLayerID)
        return false;

    if (!decoder.decode(result.m_createdLayers))
        return false;

    if (!decoder.decode(result.m_changedLayerProperties))
        return false;

    if (!decoder.decode(result.m_destroyedLayerIDs))
        return false;
    for (LayerID layerID : result.m_destroyedLayerIDs) {
        if (!layerID)
            return false;
    }

    return true;
}

void RemoteLayerTreeTransaction::setRootLayerID(LayerID rootLayerID)
{
    ASSERT_ARG(rootLayerID, rootLayerID);

    m_rootLayerID = rootLayerID;
}

void RemoteLayerTreeTransaction::layerPropertiesChanged(PlatformCALayerRemote* remoteLayer, RemoteLayerTreeTransaction::LayerProperties& properties)
{
    m_changedLayerProperties.set(remoteLayer->layerID(), properties);
}

void RemoteLayerTreeTransaction::setCreatedLayers(Vector<LayerCreationProperties> createdLayers)
{
    m_createdLayers = std::move(createdLayers);
}

void RemoteLayerTreeTransaction::setDestroyedLayerIDs(Vector<LayerID> destroyedLayerIDs)
{
    m_destroyedLayerIDs = std::move(destroyedLayerIDs);
}

#ifndef NDEBUG

static void writeIndent(StringBuilder& builder, int indent)
{
    for (int i = 0; i < indent; ++i)
        builder.append(' ');
}

static void dumpProperty(StringBuilder& builder, String name, const TransformationMatrix& transform)
{
    if (transform.isIdentity())
        return;

    builder.append('\n');
    writeIndent(builder, 3);
    builder.append("(");
    builder.append(name);
    builder.append("\n");

    TextStream ts;
    ts << "    [" << transform.m11() << " " << transform.m12() << " " << transform.m13() << " " << transform.m14() << "]\n";
    ts << "    [" << transform.m21() << " " << transform.m22() << " " << transform.m23() << " " << transform.m24() << "]\n";
    ts << "    [" << transform.m31() << " " << transform.m32() << " " << transform.m33() << " " << transform.m34() << "]\n";
    ts << "    [" << transform.m41() << " " << transform.m42() << " " << transform.m43() << " " << transform.m44() << "])";

    builder.append(ts.release());
}

static void dumpProperty(StringBuilder& builder, String name, const PlatformCALayer::FilterType filterType)
{
    builder.append('\n');
    writeIndent(builder, 3);
    builder.append('(');
    builder.append(name);
    builder.append(' ');

    switch (filterType) {
        case PlatformCALayer::Linear:
            builder.append("linear");
            break;
        case PlatformCALayer::Nearest:
            builder.append("nearest");
            break;
        case PlatformCALayer::Trilinear:
            builder.append("trilinear");
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
    }

    builder.append(")");
}

static void dumpChangedLayers(StringBuilder& builder, const HashMap<RemoteLayerTreeTransaction::LayerID, RemoteLayerTreeTransaction::LayerProperties>& changedLayerProperties)
{
    if (changedLayerProperties.isEmpty())
        return;

    writeIndent(builder, 1);
    builder.append("(changed-layers\n");

    // Dump the layer properties sorted by layer ID.
    Vector<RemoteLayerTreeTransaction::LayerID> layerIDs;
    copyKeysToVector(changedLayerProperties, layerIDs);
    std::sort(layerIDs.begin(), layerIDs.end());

    for (auto layerID : layerIDs) {
        const RemoteLayerTreeTransaction::LayerProperties& layerProperties = changedLayerProperties.get(layerID);

        writeIndent(builder, 2);
        builder.append("(layer ");
        builder.appendNumber(layerID);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::NameChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(name \"");
            builder.append(layerProperties.name);
            builder.append("\")");
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ChildrenChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(children (");
            for (size_t i = 0; i < layerProperties.children.size(); ++i) {
                if (i)
                    builder.append(' ');
                builder.appendNumber(layerProperties.children[i]);
            }

            builder.append(")");
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::PositionChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(position ");
            builder.appendNumber(layerProperties.position.x());
            builder.append(' ');
            builder.appendNumber(layerProperties.position.y());
            builder.append(' ');
            builder.appendNumber(layerProperties.position.z());
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::SizeChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(size ");
            builder.appendNumber(layerProperties.size.width());
            builder.append(' ');
            builder.appendNumber(layerProperties.size.height());
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::AnchorPointChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(anchorPoint ");
            builder.appendNumber(layerProperties.anchorPoint.x());
            builder.append(' ');
            builder.appendNumber(layerProperties.anchorPoint.y());
            builder.append(' ');
            builder.appendNumber(layerProperties.anchorPoint.z());
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BackgroundColorChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(backgroundColor ");
            builder.append(layerProperties.backgroundColor.serialized());
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BorderColorChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(borderColor ");
            builder.append(layerProperties.borderColor.serialized());
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BorderWidthChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(borderWidth ");
            builder.appendNumber(layerProperties.borderWidth);
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::OpacityChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(opacity ");
            builder.appendNumber(layerProperties.opacity);
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::TransformChanged)
            dumpProperty(builder, "transform", layerProperties.transform);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::SublayerTransformChanged)
            dumpProperty(builder, "sublayerTransform", layerProperties.sublayerTransform);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::HiddenChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(hidden ");
            builder.append(layerProperties.hidden ? "true" : "false");
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::GeometryFlippedChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(geometryFlipped ");
            builder.append(layerProperties.geometryFlipped ? "true" : "false");
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::DoubleSidedChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(doubleSided ");
            builder.append(layerProperties.doubleSided ? "true" : "false");
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::MasksToBoundsChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(masksToBounds ");
            builder.append(layerProperties.masksToBounds ? "true" : "false");
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::OpaqueChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(opaque ");
            builder.append(layerProperties.opaque ? "true" : "false");
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::MaskLayerChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(maskLayer ");
            builder.appendNumber(layerProperties.maskLayer);
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ContentsRectChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(contentsRect ");
            builder.appendNumber(layerProperties.contentsRect.x());
            builder.append(' ');
            builder.appendNumber(layerProperties.contentsRect.y());
            builder.append(' ');
            builder.appendNumber(layerProperties.contentsRect.width());
            builder.append(' ');
            builder.appendNumber(layerProperties.contentsRect.height());
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ContentsScaleChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(contentsScale ");
            builder.appendNumber(layerProperties.contentsScale);
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::MinificationFilterChanged)
            dumpProperty(builder, "minificationFilter", layerProperties.minificationFilter);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::MagnificationFilterChanged)
            dumpProperty(builder, "magnificationFilter", layerProperties.magnificationFilter);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::SpeedChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(speed ");
            builder.appendNumber(layerProperties.speed);
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::TimeOffsetChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(timeOffset ");
            builder.appendNumber(layerProperties.timeOffset);
            builder.append(')');
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BackingStoreChanged) {
            builder.append('\n');
            writeIndent(builder, 3);
            builder.append("(backingStore ");
            builder.appendNumber(layerProperties.backingStore.bitmap()->size().width());
            builder.append(" ");
            builder.appendNumber(layerProperties.backingStore.bitmap()->size().height());
            builder.append(')');
        }

        builder.append(")\n");
    }
}

void RemoteLayerTreeTransaction::dump() const
{
    StringBuilder builder;

    builder.append("(\n");

    writeIndent(builder, 1);
    builder.append("(root-layer ");
    builder.appendNumber(m_rootLayerID);
    builder.append(")\n");

    if (!m_createdLayers.isEmpty()) {
        writeIndent(builder, 1);
        builder.append("(created-layers\n");
        for (const auto& createdLayer : m_createdLayers) {
            writeIndent(builder, 2);
            builder.append("(");
            switch (createdLayer.type) {
            case PlatformCALayer::LayerTypeLayer:
            case PlatformCALayer::LayerTypeWebLayer:
                builder.append("layer");
                break;
            case PlatformCALayer::LayerTypeTransformLayer:
                builder.append("transform-layer");
                break;
            case PlatformCALayer::LayerTypeWebTiledLayer:
                builder.append("tiled-layer");
                break;
            case PlatformCALayer::LayerTypeTiledBackingLayer:
                builder.append("tiled-backing-layer");
                break;
            case PlatformCALayer::LayerTypePageTiledBackingLayer:
                builder.append("page-tiled-backing-layer");
                break;
            case PlatformCALayer::LayerTypeRootLayer:
                builder.append("root-layer");
                break;
            case PlatformCALayer::LayerTypeAVPlayerLayer:
                builder.append("av-player-layer");
                break;
            case PlatformCALayer::LayerTypeCustom:
                builder.append("custom-layer");
                break;
            }
            builder.append(' ');
            builder.appendNumber(createdLayer.layerID);
            builder.append(")\n");
        }
        builder.append(")\n");
    }

    dumpChangedLayers(builder, m_changedLayerProperties);

    if (!m_destroyedLayerIDs.isEmpty()) {
        writeIndent(builder, 1);
        builder.append("(destroyed-layers ");
        for (size_t i = 0; i < m_destroyedLayerIDs.size(); ++i) {
            if (i)
                builder.append(' ');
            builder.appendNumber(m_destroyedLayerIDs[i]);
        }
        builder.append(")\n");
    }
    builder.append(")\n");

    fprintf(stderr, "%s", builder.toString().utf8().data());
}

#endif // NDEBUG

} // namespace WebKit
