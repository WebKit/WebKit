/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WI.CSSObserver = class CSSObserver extends InspectorBackend.Dispatcher
{
    // Events defined by the "CSS" domain.

    mediaQueryResultChanged()
    {
        WI.cssManager.mediaQueryResultChanged();
    }

    styleSheetChanged(styleSheetId)
    {
        WI.cssManager.styleSheetChanged(styleSheetId);
    }

    styleSheetAdded(styleSheetInfo)
    {
        WI.cssManager.styleSheetAdded(styleSheetInfo);
    }

    styleSheetRemoved(id)
    {
        WI.cssManager.styleSheetRemoved(id);
    }

    nodeLayoutContextTypeChanged(nodeId, layoutContextType)
    {
        WI.domManager.nodeLayoutContextTypeChanged(nodeId, layoutContextType);
    }

    namedFlowCreated(namedFlow)
    {
        // COMPATIBILITY (iOS 11.3): Removed.
    }

    namedFlowRemoved(documentNodeId, flowName)
    {
        // COMPATIBILITY (iOS 11.3): Removed.
    }

    regionOversetChanged(namedFlow)
    {
        // COMPATIBILITY (iOS 11.3): Removed.
    }

    registeredNamedFlowContentElement(documentNodeId, flowName, contentNodeId, nextContentElementNodeId)
    {
        // COMPATIBILITY (iOS 11.3): Removed.
    }

    unregisteredNamedFlowContentElement(documentNodeId, flowName, contentNodeId)
    {
        // COMPATIBILITY (iOS 11.3): Removed.
    }
};
