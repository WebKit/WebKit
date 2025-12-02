/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeCommitBundle.h"

#if !defined(NDEBUG) || !LOG_DISABLED

#import <wtf/text/TextStream.h>

namespace WebKit {

String PageData::description() const
{
    TextStream ts;

    ts.startGroup();
    ts << "PageData"_s;

    ts.dumpProperty("renderTreeSize"_s, renderTreeSize);

    ts.endGroup();

    return ts.release();
}

String MainFrameData::description() const
{
    TextStream ts;

    ts.startGroup();
    ts << "MainFrameData"_s;

    ts.dumpProperty("baseLayoutViewportSize"_s, WebCore::FloatSize(baseLayoutViewportSize));

    if (minStableLayoutViewportOrigin != WebCore::LayoutPoint::zero())
        ts.dumpProperty("minStableLayoutViewportOrigin"_s, WebCore::FloatPoint(minStableLayoutViewportOrigin));
    ts.dumpProperty("maxStableLayoutViewportOrigin"_s, WebCore::FloatPoint(maxStableLayoutViewportOrigin));

    if (pageScaleFactor != 1)
        ts.dumpProperty("pageScaleFactor"_s, pageScaleFactor);

#if PLATFORM(MAC)
    ts.dumpProperty("pageScalingLayer"_s, pageScalingLayerID);
    ts.dumpProperty("scrolledContentsLayerID"_s, scrolledContentsLayerID);
    ts.dumpProperty("mainFrameClipLayerID"_s, mainFrameClipLayerID);
#endif

    ts.dumpProperty("minimumScaleFactor"_s, minimumScaleFactor);
    ts.dumpProperty("maximumScaleFactor"_s, maximumScaleFactor);
    ts.dumpProperty("initialScaleFactor"_s, initialScaleFactor);
    ts.dumpProperty("viewportMetaTagWidth"_s, viewportMetaTagWidth);
    ts.dumpProperty("viewportMetaTagWidthWasExplicit"_s, viewportMetaTagWidthWasExplicit);
    ts.dumpProperty("viewportMetaTagCameFromImageDocument"_s, viewportMetaTagCameFromImageDocument);
    ts.dumpProperty("allowsUserScaling"_s, allowsUserScaling);
    ts.dumpProperty("avoidsUnsafeArea"_s, avoidsUnsafeArea);
    ts.dumpProperty("isInStableState"_s, isInStableState);

    if (editorState) {
        TextStream::GroupScope scope(ts);
        ts << "EditorState"_s;
        ts << *editorState;
    }

    ts.endGroup();

    return ts.release();
}

String RemoteLayerTreeCommitBundle::description() const
{
    TextStream ts;

    ts.startGroup();
    ts << "RemoteLayerTreeCommitBundle"_s;

    ts.dumpProperty("transactionID"_s, transactionID);
    ts.dumpProperty("startTime"_s, startTime.secondsSinceEpoch().milliseconds());

    ts.endGroup();

    return ts.release();
}

} // namespace WebKit

#endif // !defined(NDEBUG) || !LOG_DISABLED
