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

WebInspector.CallingContextTree = class CallingContextTree extends WebInspector.Object
{
    constructor()
    {
        super();

        this._root = new WebInspector.CCTNode(-1, -1, -1, "<root>", null);
        this._totalNumberOfSamples = 0;
    }

    // Public

    get totalNumberOfSamples() { return this._totalNumberOfSamples; }
    
    updateTreeWithStackTrace({timestamp, stackFrames})
    {
        this._totalNumberOfSamples++;
        let node = this._root;
        node.addTimestamp(timestamp);
        for (let i = stackFrames.length; i--; ) {
            let stackFrame = stackFrames[i];
            node = node.findOrMakeChild(stackFrame);
            node.addTimestamp(timestamp);
        }
    }

    toCPUProfilePayload(startTime, endTime)
    {
        let cpuProfile = {};
        let roots = [];
        let numSamplesInTimeRange = this._root.filteredTimestamps(startTime, endTime).length;

        this._root.forEachChild((child) => {
            if (child.hasStackTraceInTimeRange(startTime, endTime))
                roots.push(child.toCPUProfileNode(numSamplesInTimeRange, startTime, endTime)); 
        });

        cpuProfile.rootNodes = roots;
        return cpuProfile;
    }

    forEachNode(callback)
    {
        this._root.forEachNode(callback);
    }

    // Testing.

    static __test_makeTreeFromProtocolMessageObject(messageObject)
    {
        let tree = new WebInspector.CallingContextTree;
        let stackTraces = messageObject.params.samples.stackTraces;
        for (let i = 0; i < stackTraces.length; i++)
            tree.updateTreeWithStackTrace(stackTraces[i]);
        return tree;
    }

    __test_matchesStackTrace(stackTrace)
    {
        // StackTrace should have top frame first in the array and bottom frame last.
        // We don't look for a match that traces down the tree from the root; instead,
        // we match by looking at all the leafs, and matching while walking up the tree
        // towards the root. If we successfully make the walk, we've got a match that
        // suffices for a particular test. A successful match doesn't mean we actually
        // walk all the way up to the root; it just means we didn't fail while walking
        // in the direction of the root.
        let leaves = this.__test_buildLeafLinkedLists();

        outer:
        for (let node of leaves) {
            for (let stackNode of stackTrace) {
                for (let propertyName of Object.getOwnPropertyNames(stackNode)) {
                    if (stackNode[propertyName] !== node[propertyName])
                        continue outer;
                }
                node = node.parent;
            }
            return true;
        }
        return false;
    }

    __test_buildLeafLinkedLists()
    {
        let result = [];
        let parent = null;
        this._root.__test_buildLeafLinkedLists(parent, result);
        return result;
    }
};

WebInspector.CCTNode = class CCTNode extends WebInspector.Object
{
    constructor(sourceID, line, column, name, url)
    {
        super();

        this._children = {};
        this._sourceID = sourceID;
        this._line = line;
        this._column = column;
        this._name = name;
        this._url = url;
        this._timestamps = [];
        this._uid = WebInspector.CCTNode.__uid++;
    }

    // Static and Private

    static _hash(stackFrame)
    {
        return stackFrame.name + ":" + stackFrame.sourceID + ":" + stackFrame.line + ":" + stackFrame.column;
    }

    // Public

    get sourceID() { return this._sourceID; }
    get line() { return this._line; }
    get column() { return this._column; }
    get name() { return this._name; }
    get uid() { return this._uid; }
    get url() { return this._url; }

    hasStackTraceInTimeRange(startTime, endTime)
    {
        console.assert(startTime <= endTime);
        if (startTime > endTime)
            return false;

        let timestamps = this._timestamps;
        let length = timestamps.length;
        if (!length)
            return false;

        let index = timestamps.lowerBound(startTime);
        if (index === length)
            return false;
        console.assert(startTime <= timestamps[index]);

        let hasTimestampInRange = timestamps[index] <= endTime;
        return hasTimestampInRange;
    }

    filteredTimestamps(startTime, endTime)
    {
        let index = this._timestamps.lowerBound(startTime); // The left-most (smallest) item that is >= startTime.
        let result = [];
        for (; index < this._timestamps.length; index++) {
            let timestamp = this._timestamps[index];
            console.assert(startTime <= timestamp);
            if (!(timestamp <= endTime))
                break;
            result.push(timestamp);
        }
        return result;
    }

    hasChildren()
    {
        return !!Object.getOwnPropertyNames(this._children).length;
    }

    findOrMakeChild(stackFrame)
    {
        let hash = WebInspector.CCTNode._hash(stackFrame);
        let node = this._children[hash];
        if (node)
            return node;
        node = new WebInspector.CCTNode(stackFrame.sourceID, stackFrame.line, stackFrame.column, stackFrame.name, stackFrame.url);
        this._children[hash] = node;
        return node;
    }

    addTimestamp(timestamp)
    {
        console.assert(!this._timestamps.length || this._timestamps.lastValue <= timestamp, "Expected timestamps to be added in sorted, increasing, order.");
        this._timestamps.push(timestamp);
    }

    forEachChild(callback)
    {
        for (let propertyName of Object.getOwnPropertyNames(this._children))
            callback(this._children[propertyName]);
    }

    forEachNode(callback)
    {
        callback(this);
        this.forEachChild(function(child) {
            child.forEachNode(callback);
        });
    }

    toCPUProfileNode(numSamples, startTime, endTime)
    {
        let children = [];
        this.forEachChild((child) => {
            if (child.hasStackTraceInTimeRange(startTime, endTime))
                children.push(child.toCPUProfileNode(numSamples, startTime, endTime));
        });
        let cpuProfileNode = {
            id: this._uid,
            functionName: this._name,
            url: this._url,
            lineNumber: this._line,
            columnNumber: this._column,
            children: children
        };

        let timestamps = [];
        let frameStartTime = Number.MAX_VALUE;
        let frameEndTime = Number.MIN_VALUE;
        for (let i = 0; i < this._timestamps.length; i++) {
            let timestamp = this._timestamps[i];
            if (startTime <= timestamp && timestamp <= endTime) {
                timestamps.push(timestamp);
                frameStartTime = Math.min(frameStartTime, timestamp);
                frameEndTime = Math.max(frameEndTime, timestamp);
            }
        }

        cpuProfileNode.callInfo = {
            callCount: timestamps.length, // Totally not callCount, but oh well, this makes life easier because of field names.
            startTime: frameStartTime,
            endTime: frameEndTime,
            totalTime: (timestamps.length / numSamples) * (endTime - startTime)
        };

        return cpuProfileNode;
    }

    // Testing.

    __test_buildLeafLinkedLists(parent, result)
    {
        let linkedListNode = {
            name: this._name,
            url: this._url,
            parent: parent
        };
        if (this.hasChildren()) {
            this.forEachChild((child) => {
                child.__test_buildLeafLinkedLists(linkedListNode, result);
            });
        } else {
            // We're a leaf.
            result.push(linkedListNode);
        }
    }
};

WebInspector.CCTNode.__uid = 0;

