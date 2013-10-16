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
#import <wtf/text/CString.h>
#import <wtf/text/StringBuilder.h>

using namespace WebCore;

namespace WebKit {

RemoteLayerTreeTransaction::LayerProperties::LayerProperties()
    : changedProperties(NoChange)
{
}

void RemoteLayerTreeTransaction::LayerProperties::encode(CoreIPC::ArgumentEncoder& encoder) const
{
    encoder << changedProperties;

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
}

bool RemoteLayerTreeTransaction::LayerProperties::decode(CoreIPC::ArgumentDecoder& decoder, LayerProperties& result)
{
    if (!decoder.decode(result.changedProperties))
        return false;

    if (result.changedProperties & NameChanged) {
        if (!decoder.decode(result.name))
            return false;
    }

    if (result.changedProperties & ChildrenChanged) {
        if (!decoder.decode(result.children))
            return false;

        for (auto layerID: result.children) {
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
    encoder << m_changedLayerProperties;
    encoder << m_destroyedLayerIDs;
}

bool RemoteLayerTreeTransaction::decode(CoreIPC::ArgumentDecoder& decoder, RemoteLayerTreeTransaction& result)
{
    if (!decoder.decode(result.m_rootLayerID))
        return false;
    if (!result.m_rootLayerID)
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

    for (auto layerID: layerIDs) {
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
