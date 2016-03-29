/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

WebInspector.HeapSnapshotProxy = class HeapSnapshotProxy extends WebInspector.Object
{
    constructor(snapshotObjectId, identifier, title, totalSize, totalObjectCount, categories)
    {
        super();

        this._proxyObjectId = snapshotObjectId;

        this._identifier = identifier;
        this._title = title;
        this._totalSize = totalSize;
        this._totalObjectCount = totalObjectCount;
        this._categories = Map.fromObject(categories);
    }

    // Static

    static deserialize(objectId, serializedSnapshot)
    {
        let {identifier, title, totalSize, totalObjectCount, categories} = serializedSnapshot;
        return new WebInspector.HeapSnapshotProxy(objectId, identifier, title, totalSize, totalObjectCount, categories);
    }

    // Public

    get proxyObjectId() { return this._proxyObjectId; }
    get identifier() { return this._identifier; }
    get title() { return this._title; }
    get totalSize() { return this._totalSize; }
    get totalObjectCount() { return this._totalObjectCount; }
    get categories() { return this._categories; }

    allocationBucketCounts(bucketSizes, callback)
    {
        WebInspector.HeapSnapshotWorkerProxy.singleton().callMethod(this._proxyObjectId, "allocationBucketCounts", bucketSizes, callback);
    }

    instancesWithClassName(className, callback)
    {
        WebInspector.HeapSnapshotWorkerProxy.singleton().callMethod(this._proxyObjectId, "instancesWithClassName", className, (serializedNodes) => {
            callback(serializedNodes.map(WebInspector.HeapSnapshotNodeProxy.deserialize.bind(null, this._proxyObjectId)));
        });
    }

    nodeWithIdentifier(nodeIdentifier, callback)
    {
        WebInspector.HeapSnapshotWorkerProxy.singleton().callMethod(this._proxyObjectId, "nodeWithIdentifier", nodeIdentifier, (serializedNode) => {
            callback(WebInspector.HeapSnapshotNodeProxy.deserialize(this._proxyObjectId, serializedNode));
        });
    }
};
