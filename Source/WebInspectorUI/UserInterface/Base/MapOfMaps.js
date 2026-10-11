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

class MapOfMaps
{
    constructor()
    {
        this._map = new Map;
    }

    // Public

    get size()
    {
        return this._map.size;
    }

    has(key1, key2)
    {
        let valueMap = this._map.get(key1);
        if (!valueMap)
            return false;
        return arguments.length === 1 || valueMap.has(key2);
    }

    get(key1, key2)
    {
        let valueMap = this._map.get(key1);
        if (arguments.length === 1)
            return valueMap;
        return valueMap?.get(key2);
    }

    getOrInsert(key1, key2, value)
    {
        return this._map.getOrInsert(key1, new Map).getOrInsert(key2, value);
    }

    set(key1, key2, value)
    {
        this._map.getOrInsert(key1, new Map).set(key2, value);
        return this;
    }

    delete(key1, key2)
    {
        // Allow an entire key to be removed by not passing a second key.
        if (arguments.length === 1)
            return this._map.delete(key1);

        let valueMap = this._map.get(key1);
        if (!valueMap)
            return false;

        let deleted = valueMap.delete(key2);

        if (!valueMap.size)
            this._map.delete(key1);

        return deleted;
    }

    take(key1, key2)
    {
        // Allow an entire key to be removed by not passing a second key.
        if (arguments.length === 1)
            return this._map.take(key1);

        let valueMap = this._map.get(key1);
        if (!valueMap)
            return undefined;

        let result = valueMap.take(key2);
        if (!valueMap.size)
            this._map.delete(key1);
        return result;
    }

    clear()
    {
        this._map.clear();
    }

    keys()
    {
        return this._map.keys();
    }
}
