/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
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

WI.IndexSet = class IndexSet
{
    constructor(values)
    {
        console.assert(!values || Array.isArray(values));

        this._indexes = [];

        if (values) {
            for (let value of values.slice().sort((a, b) => a - b)) {
                if (value === this._indexes.lastValue)
                    continue;
                if (this._validateIndex(value))
                    this._indexes.push(value);
            }
        }
    }

    // Public

    get size() { return this._indexes.length; }

    get firstIndex()
    {
        return this._indexes.length ? this._indexes[0] : NaN;
    }

    get lastIndex()
    {
        return this._indexes.length ? this._indexes.lastValue : NaN;
    }

    add(value)
    {
        if (!this._validateIndex(value))
            return;

        let index = this._indexes.lowerBound(value);
        if (this._indexes[index] === value)
            return;

        this._indexes.insertAtIndex(value, index);
    }

    delete(value)
    {
        if (!this._validateIndex(value))
            return false;

        if (!this.size)
            return false;

        let index = this._indexes.lowerBound(value);
        if (index === this._indexes.length)
            return false;
        this._indexes.splice(index, 1);
        return true;
    }

    has(value)
    {
        if (!this.size)
            return false;

        let index = this._indexes.lowerBound(value);
        return this._indexes[index] === value;
    }

    clear()
    {
        this._indexes = [];
    }

    copy()
    {
        let indexSet = new WI.IndexSet;
        indexSet._indexes = this._indexes.slice();
        return indexSet;
    }

    indexGreaterThan(value)
    {
        const following = true;
        return this._indexClosestTo(value, following);
    }

    indexLessThan(value)
    {
        const following = false;
        return this._indexClosestTo(value, following);
    }

    [Symbol.iterator]()
    {
        return this._indexes[Symbol.iterator]();
    }

    // Private

    _indexClosestTo(value, following)
    {
        if (!this.size)
            return NaN;

        if (this.size === 1) {
            if (following)
                return this.firstIndex > value ? this.firstIndex : NaN;
            return this.firstIndex < value ? this.firstIndex : NaN;
        }

        let offset = following ? 1 : -1;
        let position = this._indexes.lowerBound(value + offset);

        let closestIndex = this._indexes[position];
        if (closestIndex === undefined)
            return NaN;

        if (value === closestIndex)
            return NaN;

        if (!following && closestIndex > value)
            return NaN;
        return closestIndex;
    }

    _validateIndex(value)
    {
        console.assert(Number.isInteger(value) && value >= 0, "Index must be a non-negative integer.");
        return Number.isInteger(value) && value >= 0;
    }
};
