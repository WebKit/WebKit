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

WebInspector.HeapSnapshotDiff = class HeapSnapshotDiff extends WebInspector.Object
{
    constructor(snapshot1, snapshot2)
    {
        super();

        console.assert(snapshot1 instanceof WebInspector.HeapSnapshot);
        console.assert(snapshot2 instanceof WebInspector.HeapSnapshot);

        this._snapshot1 = snapshot1;
        this._snapshot2 = snapshot2;

        let known = new Map;
        for (let instance of snapshot1.instances)
            known.set(instance.id, instance);

        let added = [];
        for (let instance of snapshot2.instances) {
            if (known.has(instance.id))
                known.delete(instance.id);
            else
                added.push(instance);
        }

        let removed = [...known.values()];

        this._addedInstances = added;
        this._removedInstances = removed;
    }

    // Public

    get snapshot1() { return this._snapshot1; }
    get snapshot2() { return this._snapshot2; }
    get addedInstances() { return this._addedInstances; }
    get removedInstances() { return this._removedInstances; }

    get sizeDifference()
    {
        return this._snapshot2.totalSize - this._snapshot1.totalSize;
    }

    get growth()
    {
        return this._addedInstances.reduce((sum, x) => sum += x.size, 0);
    }

    snapshotForDiff()
    {
        // FIXME: This only includes the newly added instances. Should we do anything with the removed instances?
        return new WebInspector.HeapSnapshot(null, this._addedInstances, this._snapshot2.nodeMap);
    }
};
