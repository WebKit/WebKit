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

class ListMultimap
{
    constructor()
    {
        this._insertionOrderedEntries = new LinkedList;
        this._keyMap = new Map;
    }

    get size()
    {
        return this._insertionOrderedEntries.length;
    }

    add(key, value)
    {
        let nodeMap = this._keyMap.get(key);
        if (!nodeMap) {
            nodeMap = new Map;
            this._keyMap.set(key, nodeMap);
        }

        let node = nodeMap.get(value);
        if (!node) {
            node = this._insertionOrderedEntries.push([key, value]);
            nodeMap.set(value, node);
        }

        return this;
    }

    delete(key, value)
    {
        let nodeMap = this._keyMap.get(key);
        if (!nodeMap)
            return false;

        let node = nodeMap.get(value);
        if (!node)
            return false;

        nodeMap.delete(value);
        this._insertionOrderedEntries.remove(node);
        return true;
    }

    deleteAll(key)
    {
        let nodeMap = this._keyMap.get(key);
        if (!nodeMap)
            return false;

        let list = this._insertionOrderedEntries;
        let didDelete = false;
        nodeMap.forEach(function(node) {
            list.remove(node);
            didDelete = true;
        });

        this._keyMap.delete(key);
        return didDelete;
    }

    has(key, value)
    {
        let nodeMap = this._keyMap.get(key);
        if (!nodeMap)
            return false;

        return nodeMap.has(value);
    }

    clear()
    {
        this._keyMap = new Map;
        this._insertionOrderedEntries = new LinkedList;
    }

    forEach(callback)
    {
        this._insertionOrderedEntries.forEach(callback);
    }

    toArray()
    {
        return this._insertionOrderedEntries.toArray();
    }

    toJSON()
    {
        return this.toArray();
    }
}
