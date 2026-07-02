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

WI.DOMObserver = class DOMObserver extends InspectorBackend.Dispatcher
{
    // Events defined by the "DOM" domain.

    documentUpdated()
    {
        if (this._target instanceof WI.FrameTarget) {
            WI.domManager._frameTargetDocumentUpdated(this._target);
            return;
        }
        WI.domManager._documentUpdated();
    }

    inspect(nodeId)
    {
        if (this._target instanceof WI.FrameTarget)
            return;
        WI.domManager.inspectElement(nodeId);
    }

    setChildNodes(parentId, nodes)
    {
        if (this._target instanceof WI.FrameTarget) {
            WI.domManager._frameTargetSetChildNodes(this._target, parentId, nodes);
            return;
        }
        WI.domManager._setChildNodes(parentId, nodes);
    }

    attributeModified(nodeId, name, value)
    {
        if (this._target instanceof WI.FrameTarget) {
            WI.domManager._frameTargetAttributeModified(this._target, nodeId, name, value);
            return;
        }
        WI.domManager._attributeModified(nodeId, name, value);
    }

    attributeRemoved(nodeId, name)
    {
        if (this._target instanceof WI.FrameTarget) {
            WI.domManager._frameTargetAttributeRemoved(this._target, nodeId, name);
            return;
        }
        WI.domManager._attributeRemoved(nodeId, name);
    }

    inlineStyleInvalidated(nodeIds)
    {
        if (this._target instanceof WI.FrameTarget) {
            WI.domManager._frameTargetInlineStyleInvalidated(this._target, nodeIds);
            return;
        }
        WI.domManager._inlineStyleInvalidated(nodeIds);
    }

    characterDataModified(nodeId, characterData)
    {
        if (this._target instanceof WI.FrameTarget) {
            WI.domManager._frameTargetCharacterDataModified(this._target, nodeId, characterData);
            return;
        }
        WI.domManager._characterDataModified(nodeId, characterData);
    }

    childNodeCountUpdated(nodeId, childNodeCount)
    {
        if (this._target instanceof WI.FrameTarget) {
            WI.domManager._frameTargetChildNodeCountUpdated(this._target, nodeId, childNodeCount);
            return;
        }
        WI.domManager._childNodeCountUpdated(nodeId, childNodeCount);
    }

    childNodeInserted(parentNodeId, previousNodeId, node)
    {
        if (this._target instanceof WI.FrameTarget) {
            WI.domManager._frameTargetChildNodeInserted(this._target, parentNodeId, previousNodeId, node);
            return;
        }
        WI.domManager._childNodeInserted(parentNodeId, previousNodeId, node);
    }

    childNodeRemoved(parentNodeId, nodeId)
    {
        if (this._target instanceof WI.FrameTarget) {
            WI.domManager._frameTargetChildNodeRemoved(this._target, parentNodeId, nodeId);
            return;
        }
        WI.domManager._childNodeRemoved(parentNodeId, nodeId);
    }

    willDestroyDOMNode(nodeId)
    {
        if (this._target instanceof WI.FrameTarget) {
            WI.domManager._frameTargetWillDestroyDOMNode(this._target, nodeId);
            return;
        }
        WI.domManager.willDestroyDOMNode(nodeId);
    }

    shadowRootPushed(hostId, root)
    {
        if (this._target instanceof WI.FrameTarget)
            return; // FIXME: <https://webkit.org/b/298980> Route to frame-target handler.
        WI.domManager._childNodeInserted(hostId, 0, root);
    }

    shadowRootPopped(hostId, rootId)
    {
        if (this._target instanceof WI.FrameTarget)
            return; // FIXME: <https://webkit.org/b/298980> Route to frame-target handler.
        WI.domManager._childNodeRemoved(hostId, rootId);
    }

    customElementStateChanged(nodeId, customElementState)
    {
        if (this._target instanceof WI.FrameTarget)
            return; // FIXME: <https://webkit.org/b/298980> Route to frame-target handler.
        WI.domManager._customElementStateChanged(nodeId, customElementState);
    }

    pseudoElementAdded(parentNodeId, pseudoElement)
    {
        if (this._target instanceof WI.FrameTarget)
            return; // FIXME: <https://webkit.org/b/298980> Route to frame-target handler.
        WI.domManager._pseudoElementAdded(parentNodeId, pseudoElement);
    }

    pseudoElementRemoved(parentNodeId, pseudoElementId)
    {
        if (this._target instanceof WI.FrameTarget)
            return; // FIXME: <https://webkit.org/b/298980> Route to frame-target handler.
        WI.domManager._pseudoElementRemoved(parentNodeId, pseudoElementId);
    }

    didAddEventListener(nodeId)
    {
        if (this._target instanceof WI.FrameTarget)
            return;
        WI.domManager.didAddEventListener(nodeId);
    }

    willRemoveEventListener(nodeId)
    {
        if (this._target instanceof WI.FrameTarget)
            return;
        WI.domManager.willRemoveEventListener(nodeId);
    }

    didFireEvent(nodeId, eventName, timestamp, data)
    {
        if (this._target instanceof WI.FrameTarget)
            return;
        WI.domManager.didFireEvent(nodeId, eventName, timestamp, data);
    }

    powerEfficientPlaybackStateChanged(nodeId, timestamp, isPowerEfficient)
    {
        if (this._target instanceof WI.FrameTarget)
            return;
        WI.domManager.powerEfficientPlaybackStateChanged(nodeId, timestamp, isPowerEfficient);
    }
};
