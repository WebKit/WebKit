/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.TypeDescription = class TypeDescription extends WebInspector.Object
{
    constructor(leastCommonAncestor, typeSet, structures, valid, truncated)
    {
        super();

        console.assert(!leastCommonAncestor || typeof leastCommonAncestor === "string");
        console.assert(!typeSet || typeSet instanceof WebInspector.TypeSet);
        console.assert(!structures || structures.every((x) => x instanceof WebInspector.StructureDescription));

        this._leastCommonAncestor = leastCommonAncestor || null;
        this._typeSet = typeSet || null;
        this._structures = structures || null;
        this._valid = valid || false;
        this._truncated = truncated || false;
    }

    // Static

    // Runtime.TypeDescription.
    static fromPayload(payload)
    {
        var typeSet = undefined;
        if (payload.typeSet)
            typeSet = WebInspector.TypeSet.fromPayload(payload.typeSet);

        var structures = undefined;
        if (payload.structures)
            structures = payload.structures.map(WebInspector.StructureDescription.fromPayload);

        return new WebInspector.TypeDescription(payload.leastCommonAncestor, typeSet, structures, payload.isValid, payload.isTruncated);
    }

    // Public

    get leastCommonAncestor() { return this._leastCommonAncestor; }
    get typeSet() { return this._typeSet; }
    get structures() { return this._structures; }
    get valid() { return this._valid; }
    get truncated() { return this._truncated; }
};
