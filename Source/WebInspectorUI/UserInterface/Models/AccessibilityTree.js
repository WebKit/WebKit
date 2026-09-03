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

WI.AccessibilityTree = class AccessibilityTree extends WI.Object {
    constructor(frame)
    {
        super();
        console.assert(frame instanceof WI.Frame);

        this._frame = frame;
        this._rootDOMNode = null;
        this._requestIdentifier = 0;

        WI.domManager.addEventListener(WI.DOMManager.Event.DocumentUpdated, this._documentUpdated, this);
    }

    // Public

    get frame() { return this._frame; }

    invalidate()
    {
        this._rootDOMNode = null;
        this._pendingRootDOMNodeRequests = null;

        if (this._invalidateTimeoutIdentifier)
            return;

        this.dispatchEventToListeners(WI.AccessibilityTree.Event.RootDOMNodeInvalidated);
        
    }

    requestRootDOMNode(callback)
    {
        console.assert(typeof callback === "function");
        if (typeof callback !== "function")
            return;

        if (this._rootDOMNode) {
            callback(this._rootDOMNode);
            return;
        }

        if (this._pendingRootDOMNodeRequests) {
            this._pendingRootDOMNodeRequests.push(callback);
            return;
        }

        this._pendingRootDOMNodeRequests = [callback];
        this._requestRootDOMNode();
    }

    // Private

    _requestRootDOMNode()
    {
        console.assert(this._frame.isMainFrame() || this._frame.pageExecutionContext);
        console.assert(this._pendingRootDOMNodeRequests.length);

        // Bump the request identifier. This prevents pending callbacks for previous requests from completing.
        var requestIdentifier = ++this._requestIdentifier;

        function rootObjectAvailable(error, result)
        {
            // Check to see if we have been invalidated (if the callbacks were cleared).
            if (!this._pendingRootDOMNodeRequests || requestIdentifier !== this._requestIdentifier)
                return;

            if (error) {
                console.error(JSON.stringify(error));

                this._rootDOMNode = null;
                dispatchCallbacks.call(this);
                return;
            }

            // Convert the RemoteObject to a DOMNode by asking the backend to push it to us.
            var remoteObject = WI.RemoteObject.fromPayload(result);
            remoteObject.pushNodeToFrontend(rootDOMNodeAvailable.bind(this, remoteObject));
        }

        function rootDOMNodeAvailable(remoteObject, nodeId)
        {
            remoteObject.release();

            // Check to see if we have been invalidated (if the callbacks were cleared).
            if (!this._pendingRootDOMNodeRequests || requestIdentifier !== this._requestIdentifier)
                return;

            if (!nodeId) {
                this._rootDOMNode = null;
                dispatchCallbacks.call(this);
                return;
            }

            this._rootDOMNode = WI.domManager.nodeForId(nodeId);

            console.assert(this._rootDOMNode);
            if (!this._rootDOMNode) {
                dispatchCallbacks.call(this);
                return;
            }

            // Request the child nodes since the root node is often not shown in the UI,
            // and the child nodes will be needed immediately.
            this._rootDOMNode.getChildNodes(dispatchCallbacks.bind(this));
        }

        function mainDocumentAvailable(document)
        {
            this._rootDOMNode = document;

            dispatchCallbacks.call(this);
        }

        function dispatchCallbacks()
        {
            if (!this._pendingRootDOMNodeRequests || requestIdentifier !== this._requestIdentifier)
                return;

            for (var i = 0; i < this._pendingRootDOMNodeRequests.length; ++i)
                this._pendingRootDOMNodeRequests[i](this._rootDOMNode);
            this._pendingRootDOMNodeRequests = null;
        }

        if (this._frame.isMainFrame())
            WI.domManager.requestDocument(mainDocumentAvailable.bind(this));
        else {
            let target = WI.assumingMainTarget();
            var contextId = this._frame.pageExecutionContext.id;
            target.RuntimeAgent.evaluate.invoke({expression: appendWebInspectorSourceURL("document"), objectGroup: "", includeCommandLineAPI: false, doNotPauseOnExceptionsAndMuteConsole: true, contextId, returnByValue: false, generatePreview: false}, rootObjectAvailable.bind(this));
        }
    }

    _documentUpdated(event)
    {
        this.invalidate();
    }
};

WI.AccessibilityTree.Event = {
    RootDOMNodeInvalidated: "dom-tree-root-dom-node-invalidated",
};
