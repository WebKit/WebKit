/*
 * Copyright (C) 2026 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

class IterableWeakMap
{
    constructor(entries = [])
    {
        this._valueForWrapper = new Map;
        this._wrapperForKey = new WeakMap;

        for (let [key, value] of entries)
            this.set(key, value);
    }

    // Public

    get size()
    {
        let size = 0;
        for (let wrapper of this._valueForWrapper.keys()) {
            if (wrapper.deref())
                ++size;
        }
        return size;
    }

    has(key)
    {
        let result = this._wrapperForKey.has(key);
        console.assert(Array.from(this._valueForWrapper.keys()).some((wrapper) => wrapper.deref() === key) === result, this, key);
        return result;
    }

    get(key)
    {
        let wrapper = this._wrapperForKey.get(key);
        if (!wrapper)
            return undefined;

        return this._valueForWrapper.get(wrapper);
    }

    set(key, value)
    {
        console.assert(typeof key === "object", key);
        console.assert(key !== null, key);

        let wrapper = this._wrapperForKey.get(key);
        if (wrapper) {
            this._valueForWrapper.set(wrapper, value);
            return;
        }

        wrapper = new WeakRef(key);
        this._wrapperForKey.set(key, wrapper);
        this._valueForWrapper.set(wrapper, value);
        this._finalizationRegistry.register(key, {weakThis: new WeakRef(this), wrapper}, wrapper);
    }

    delete(key)
    {
        let wrapper = this._wrapperForKey.get(key);
        if (!wrapper)
            return false;

        this._take(key, wrapper);
        return true;
    }

    take(key)
    {
        let wrapper = this._wrapperForKey.get(key);
        if (!wrapper)
            return undefined;

        return this._take(key, wrapper);
    }

    clear()
    {
        for (let wrapper of this._valueForWrapper.keys()) {
            let key = wrapper.deref();
            if (key)
                this._wrapperForKey.delete(key);
            this._finalizationRegistry.unregister(wrapper);
        }
        this._valueForWrapper.clear();
    }

    *keys()
    {
        for (let entry of this.entries())
            yield entry[0];
    }

    *values()
    {
        for (let entry of this.entries())
            yield entry[1];
    }

    *entries()
    {
        for (let [wrapper, value] of this._valueForWrapper) {
            let key = wrapper.deref();
            console.assert(!key === !this._wrapperForKey.has(key), this, key);
            if (key)
                yield [key, value];
        }
    }

    [Symbol.iterator]()
    {
        return this.entries();
    }

    copy()
    {
        return new IterableWeakMap(this.toJSON());
    }

    toJSON()
    {
        return Array.from(this);
    }

    // Private

    _take(key, wrapper)
    {
        let value = this._valueForWrapper.get(wrapper);

        let keyDeleted = this._wrapperForKey.delete(key);
        console.assert(keyDeleted, this, key);

        let wrapperDeleted = this._valueForWrapper.delete(wrapper);
        console.assert(wrapperDeleted, this, key);

        this._finalizationRegistry.unregister(wrapper);

        console.assert(wrapper.deref() === key, this, key);
        return value;
    }

    get _finalizationRegistry()
    {
        return IterableWeakMap._finalizationRegistry ??= new FinalizationRegistry(function(heldValue) {
            heldValue.weakThis.deref()?._valueForWrapper.delete(heldValue.wrapper);
        });
    }
}
