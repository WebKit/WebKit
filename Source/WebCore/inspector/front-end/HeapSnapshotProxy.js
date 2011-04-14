/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyrightdd
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.HeapSnapshotProxy = function()
{   
    this._snapshot = null;
}

WebInspector.HeapSnapshotProxy.prototype = {
    _invokeGetter: function(getterName, callback)
    {
        function returnResult()
        {
            callback(this._snapshot[getterName]);
        }
        setTimeout(returnResult.bind(this), 0);
    },

    aggregates: function(withNodeIndexes, callback)
    {
        function returnResult()
        {
            callback(this._snapshot.aggregates(withNodeIndexes));
        }
        setTimeout(returnResult.bind(this), 0);
    },

    _extractEdgeData: function(edge)
    {
        return {name: edge.name, node: this._extractNodeData(edge.node), nodeIndex: edge.nodeIndex, type: edge.type};
    },

    _extractNodeData: function(node)
    {
        return {id: node.id, name: node.name, nodeIndex: node.nodeIndex, retainedSize: node.retainedSize, selfSize: node.selfSize, type: node.type};
    },

    createDiff: function(className)
    {
        return new WebInspector.HeapSnapshotsDiffProxy(new WebInspector.HeapSnapshotsDiff(this._snapshot, className));
    },

    createEdgesProvider: function(nodeIndex, filter)
    {
        function createProvider()
        {
            if (filter)
                filter = filter.bind(this._snapshot);
            return new WebInspector.HeapSnapshotEdgesProvider(this._snapshot, nodeIndex, filter);
        }
        return new WebInspector.HeapSnapshotProviderProxy(createProvider.bind(this), this._extractEdgeData.bind(this));
    },

    createNodesProvider: function(filter)
    {
        function createProvider()
        {
            if (filter)
                filter = filter.bind(this._snapshot);
            return new WebInspector.HeapSnapshotNodesProvider(this._snapshot, filter);
        }
        return new WebInspector.HeapSnapshotProviderProxy(createProvider.bind(this), this._extractNodeData.bind(this));
    },

    createPathFinder: function(targetNodeIndex)
    {
        return new WebInspector.HeapSnapshotPathFinderProxy(new WebInspector.HeapSnapshotPathFinder(this._snapshot, targetNodeIndex));
    },

    dispose: function()
    {
        this._snapshot.dispose();
    },

    finishLoading: function(callback)
    {
        if (this._snapshot || !this._isLoading)
            return false;
        function parse() {
            var rawSnapshot = JSON.parse(this._json);
            var loadCallbacks = this._onLoadCallbacks;
            loadCallbacks.splice(0, 0, callback);
            delete this._onLoadCallback;
            delete this._json;
            delete this._isLoading;
            this._snapshot = new WebInspector.HeapSnapshot(rawSnapshot);
            this._nodeCount = this._snapshot.nodeCount;
            this._rootNodeIndex = this._snapshot._rootNodeIndex;
            this._totalSize = this._snapshot.totalSize;
            for (var i = 0; i < loadCallbacks.length; ++i)
                loadCallbacks[i]();
        }
        setTimeout(parse.bind(this), 0);
        return true;
    },

    get loaded()
    {
        return !!this._snapshot;
    },

    get nodeCount()
    {
        return this._nodeCount;
    },

    nodeFieldValuesByIndex: function(fieldName, indexes, callback)
    {
        function returnResult()
        {
            callback(this._snapshot.nodeFieldValuesByIndex(fieldName, indexes));
        }
        setTimeout(returnResult.bind(this), 0);
    },

    nodeIds: function(callback)
    {
        this._invokeGetter("nodeIds", callback);
    },

    pushJSONChunk: function(chunk)
    {
        if (this.loaded || !this._isLoading)
            return;
        this._json += chunk;
    },

    pushBaseIds: function(snapshotId, className, nodeIds)
    {
        this._snapshot.updateBaseNodeIds(snapshotId, className, nodeIds);
    },

    get rootNodeIndex()
    {
        return this._rootNodeIndex;
    },

    startLoading: function(callback)
    {
        if (this._snapshot) {
            function asyncInvoke()
            {
                callback();
            }
            setTimeout(callback, 0);
            return false;
        } else if (this._isLoading) {
            this._onLoadCallbacks.push(callback);
            return false;
        } else {
            this._isLoading = true;
            this._onLoadCallbacks = [callback];
            this._json = "";
            return true;
        }
    },

    get totalSize()
    {
        return this._totalSize;
    },

    get uid()
    {
        return this._snapshot.uid;
    }
};

WebInspector.HeapSnapshotProviderProxy = function(createProvider, extractData)
{
    this._provider = createProvider();
    this._extractData = extractData;
}

WebInspector.HeapSnapshotProviderProxy.prototype = {
    getNextItems: function(count, callback)
    {
        function returnResult()
        {
            var result = new Array(count);
            for (var i = 0 ; i < count && this._provider.hasNext(); ++i, this._provider.next())
                result[i] = this._extractData(this._provider.item);
            result.length = i;
            callback(result, this._provider.hasNext(), this._provider.length);
        }
        setTimeout(returnResult.bind(this), 0);
    },

    isEmpty: function(callback)
    {
        function returnResult()
        {
            callback(this._provider.isEmpty);
        }
        setTimeout(returnResult.bind(this), 0);
    },

    sortAndRewind: function(comparator, callback)
    {
        function returnResult()
        {
            var result = this._provider.sort(comparator);
            if (result)
                this._provider.first();
            callback(result);
        }
        setTimeout(returnResult.bind(this), 0);
    }
};

WebInspector.HeapSnapshotPathFinderProxy = function(pathFinder)
{
    this._pathFinder = pathFinder;
}

WebInspector.HeapSnapshotPathFinderProxy.prototype = {
    findNext: function(callback)
    {
        function returnResult()
        {
            callback(this._pathFinder.findNext());
        }
        setTimeout(returnResult.bind(this), 0);
    },

    updateRoots: function(filter)
    {
        function asyncInvoke()
        {
            this._pathFinder.updateRoots(filter);
        }
        setTimeout(asyncInvoke.bind(this), 0);
    }
};

WebInspector.HeapSnapshotsDiffProxy = function(diff)
{
    this._diff = diff;
}

WebInspector.HeapSnapshotsDiffProxy.prototype = {
    calculate: function(callback)
    {
        function returnResult()
        {
            callback(this._diff.calculate());
        }
        setTimeout(returnResult.bind(this), 0);
    },

    pushBaseIds: function(baseSnapshotId, baseIds)
    {
        this._diff.baseIds = baseIds;
    },

    pushBaseSelfSizes: function(baseSelfSizes)
    {
        this._diff.baseSelfSizes = baseSelfSizes;
    }
};
