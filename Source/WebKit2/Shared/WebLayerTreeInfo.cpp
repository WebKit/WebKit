/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "WebLayerTreeInfo.h"

#include "Arguments.h"
#include "SharedMemory.h"
#include "WebCoreArgumentCoders.h"

using namespace CoreIPC;

namespace WebKit {

void WebLayerInfo::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    // We have to divide it to several lines, because CoreIPC::In/Out takes a maximum of 10 arguments.
    encoder->encode(CoreIPC::In(id, name, parent, children, flags, replica, mask, imageBackingStoreID));
    encoder->encode(CoreIPC::In(pos, size, transform, opacity, anchorPoint, childrenTransform, contentsRect, animations));
}

bool WebLayerInfo::decode(CoreIPC::ArgumentDecoder* decoder, WebLayerInfo& info)
{
    // We have to divide it to several lines, because CoreIPC::In/Out takes a maximum of 10 arguments.
    if (!decoder->decode(CoreIPC::Out(info.id, info.name, info.parent, info.children, info.flags, info.replica, info.mask, info.imageBackingStoreID)))
        return false;
    if (!decoder->decode(CoreIPC::Out(info.pos, info.size, info.transform, info.opacity, info.anchorPoint, info.childrenTransform, info.contentsRect, info.animations)))
        return false;

    return true;
}

void WebLayerTreeInfo::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(CoreIPC::In(layers, rootLayerID, deletedLayerIDs, contentScale));
}

bool WebLayerTreeInfo::decode(CoreIPC::ArgumentDecoder* decoder, WebLayerTreeInfo& info)
{
    return decoder->decode(CoreIPC::Out(info.layers, info.rootLayerID, info.deletedLayerIDs, info.contentScale));
}

void WebLayerUpdateInfo::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(CoreIPC::In(layerID, rect, bitmapHandle));
}

bool WebLayerUpdateInfo::decode(CoreIPC::ArgumentDecoder* decoder, WebLayerUpdateInfo& info)
{
    return decoder->decode(CoreIPC::Out(info.layerID, info.rect, info.bitmapHandle));
}

void WebLayerAnimation::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encodeEnum(operation);
    encoder->encode(keyframeList);
    encoder->encode(CoreIPC::In(name, startTime, boxSize, animation, keyframeList));
}

bool WebLayerAnimation::decode(CoreIPC::ArgumentDecoder* decoder, WebLayerAnimation& info)
{
    if (!decoder->decodeEnum(info.operation))
        return false;
    if (!decoder->decode(info.keyframeList))
        return false;
    if (!decoder->decode(CoreIPC::Out(info.name, info.startTime, info.boxSize, info.animation)))
        return false;

    return true;
}

}
#endif
