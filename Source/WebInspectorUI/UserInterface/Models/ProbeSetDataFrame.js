/*
 * Copyright (C) 2013 University of Washington. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.ProbeSetDataFrame = function(index)
{
    this._count = 0;
    this._index = index;
    this._separator = false;
};

WebInspector.ProbeSetDataFrame.compare = function(a, b) {
    console.assert(a instanceof WebInspector.ProbeSetDataFrame, a);
    console.assert(b instanceof WebInspector.ProbeSetDataFrame, b);

    return a.index - b.index;
}

WebInspector.ProbeSetDataFrame.MissingValue = "?";

WebInspector.ProbeSetDataFrame.prototype = {
    constructor: WebInspector.ProbeSetDataFrame,

    // Public

    get key()
    {
        return String(this._index);
    },

    get count()
    {
        return this._count;
    },

    get index()
    {
        return this._index;
    },

    get isSeparator()
    {
        return this._separator;
    },

    // The last data frame before a main frame navigation is marked as a "separator" frame.
    set isSeparator(value)
    {
        this._separator = !!value;
    },

    addSampleForProbe: function(probe, sample)
    {
        this[probe.id] = sample;
        this._count++;
    },

    missingKeys: function(probeSet)
    {
        return probeSet.probes.filter(function(probe) {
            return !this.hasOwnProperty(probe.id);
        }, this);
    },

    isComplete: function(probeSet)
    {
        return !this.missingKeys(probeSet).length;
    },

    fillMissingValues: function(probeSet)
    {
        for (var key of this.missingKeys(probeSet))
            this[key] = WebInspector.ProbeSetDataFrame.MissingValue;
    }
};
