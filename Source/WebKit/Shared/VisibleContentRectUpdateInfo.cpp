/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "VisibleContentRectUpdateInfo.h"

#include "WebCoreArgumentCoders.h"
#include <WebCore/LengthBox.h>
#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

void VisibleContentRectUpdateInfo::encode(IPC::Encoder& encoder) const
{
    encoder << m_exposedContentRect;
    encoder << m_unobscuredContentRect;
    encoder << m_contentInsets;
    encoder << m_unobscuredContentRectRespectingInputViewBounds;
    encoder << m_unobscuredRectInScrollViewCoordinates;
    encoder << m_customFixedPositionRect;
    encoder << m_obscuredInsets;
    encoder << m_unobscuredSafeAreaInsets;
    encoder << m_lastLayerTreeTransactionID;
    encoder << m_scale;
    encoder << m_timestamp;
    encoder << m_horizontalVelocity;
    encoder << m_verticalVelocity;
    encoder << m_scaleChangeRate;
    encoder << m_inStableState;
    encoder << m_isFirstUpdateForNewViewSize;
    encoder << m_isChangingObscuredInsetsInteractively;
    encoder << m_allowShrinkToFit;
    encoder << m_enclosedInScrollableAncestorView;
}

bool VisibleContentRectUpdateInfo::decode(IPC::Decoder& decoder, VisibleContentRectUpdateInfo& result)
{
    if (!decoder.decode(result.m_exposedContentRect))
        return false;
    if (!decoder.decode(result.m_unobscuredContentRect))
        return false;
    if (!decoder.decode(result.m_contentInsets))
        return false;
    if (!decoder.decode(result.m_unobscuredContentRectRespectingInputViewBounds))
        return false;
    if (!decoder.decode(result.m_unobscuredRectInScrollViewCoordinates))
        return false;
    if (!decoder.decode(result.m_customFixedPositionRect))
        return false;
    if (!decoder.decode(result.m_obscuredInsets))
        return false;
    if (!decoder.decode(result.m_unobscuredSafeAreaInsets))
        return false;
    if (!decoder.decode(result.m_lastLayerTreeTransactionID))
        return false;
    if (!decoder.decode(result.m_scale))
        return false;
    if (!decoder.decode(result.m_timestamp))
        return false;
    if (!decoder.decode(result.m_horizontalVelocity))
        return false;
    if (!decoder.decode(result.m_verticalVelocity))
        return false;
    if (!decoder.decode(result.m_scaleChangeRate))
        return false;
    if (!decoder.decode(result.m_inStableState))
        return false;
    if (!decoder.decode(result.m_isFirstUpdateForNewViewSize))
        return false;
    if (!decoder.decode(result.m_isChangingObscuredInsetsInteractively))
        return false;
    if (!decoder.decode(result.m_allowShrinkToFit))
        return false;
    if (!decoder.decode(result.m_enclosedInScrollableAncestorView))
        return false;

    return true;
}

String VisibleContentRectUpdateInfo::dump() const
{
    TextStream stream;
    stream << *this;
    return stream.release();
}

TextStream& operator<<(TextStream& ts, const VisibleContentRectUpdateInfo& info)
{
    TextStream::GroupScope scope(ts);
    
    ts << "VisibleContentRectUpdateInfo";

    ts.dumpProperty("lastLayerTreeTransactionID", info.lastLayerTreeTransactionID());

    ts.dumpProperty("exposedContentRect", info.exposedContentRect());
    ts.dumpProperty("unobscuredContentRect", info.unobscuredContentRect());
    ts.dumpProperty("contentInsets", info.contentInsets());
    ts.dumpProperty("unobscuredContentRectRespectingInputViewBounds", info.unobscuredContentRectRespectingInputViewBounds());
    ts.dumpProperty("unobscuredRectInScrollViewCoordinates", info.unobscuredRectInScrollViewCoordinates());
    ts.dumpProperty("customFixedPositionRect", info.customFixedPositionRect());
    ts.dumpProperty("obscuredInsets", info.obscuredInsets());
    ts.dumpProperty("unobscuredSafeAreaInsets", info.unobscuredSafeAreaInsets());

    ts.dumpProperty("scale", info.scale());
    ts.dumpProperty("inStableState", info.inStableState());
    ts.dumpProperty("isFirstUpdateForNewViewSize", info.isFirstUpdateForNewViewSize());
    if (info.isChangingObscuredInsetsInteractively())
        ts.dumpProperty("isChangingObscuredInsetsInteractively", info.isChangingObscuredInsetsInteractively());
    if (info.enclosedInScrollableAncestorView())
        ts.dumpProperty("enclosedInScrollableAncestorView", info.enclosedInScrollableAncestorView());

    ts.dumpProperty("timestamp", info.timestamp().secondsSinceEpoch().value());
    ts.dumpProperty("allowShrinkToFit", info.allowShrinkToFit());
    if (info.horizontalVelocity())
        ts.dumpProperty("horizontalVelocity", info.horizontalVelocity());
    if (info.verticalVelocity())
        ts.dumpProperty("verticalVelocity", info.verticalVelocity());
    if (info.scaleChangeRate())
        ts.dumpProperty("scaleChangeRate", info.scaleChangeRate());

    return ts;
}

} // namespace WebKit
