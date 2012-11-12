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

#include "config.h"
#include "RemoteLayerTreeTransaction.h"

#include "RemoteGraphicsLayer.h"
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

RemoteLayerTreeTransaction::LayerProperties::LayerProperties()
    : changedProperties(NoChange)
{
}

RemoteLayerTreeTransaction::RemoteLayerTreeTransaction()
{
}

RemoteLayerTreeTransaction::~RemoteLayerTreeTransaction()
{
}

void RemoteLayerTreeTransaction::layerPropertiesChanged(const RemoteGraphicsLayer* graphicsLayer, unsigned changedProperties)
{
    LayerProperties& layerProperties = m_changedLayerProperties.add(graphicsLayer->layerID(), LayerProperties()).iterator->value;

    layerProperties.changedProperties |= changedProperties;

    if (changedProperties & NameChanged)
        layerProperties.name = graphicsLayer->name();
}

#ifndef NDEBUG

static void writeIndent(StringBuilder& builder, int indent)
{
    for (int i = 0; i < indent; ++i)
        builder.append(' ');
}

static void dumpChangedLayers(StringBuilder& builder, const HashMap<uint64_t, RemoteLayerTreeTransaction::LayerProperties>& changedLayerProperties)
{
    if (changedLayerProperties.isEmpty())
        return;

    writeIndent(builder, 1);
    builder.append("(changed-layers\n");

    // Dump the layer properties sorted by layer ID.
    Vector<uint64_t> layerIDs;
    copyKeysToVector(changedLayerProperties, layerIDs);
    std::sort(layerIDs.begin(), layerIDs.end());

    for (uint64_t layerID: layerIDs) {
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

        builder.append(")\n");
    }
}

void RemoteLayerTreeTransaction::dump() const
{
    StringBuilder builder;

    builder.append("(\n");
    dumpChangedLayers(builder, m_changedLayerProperties);
    builder.append(")\n");

    fprintf(stderr, "%s", builder.toString().utf8().data());
}

#endif // NDEBUG

} // namespace WebKit
