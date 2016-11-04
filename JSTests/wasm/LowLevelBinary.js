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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

const _initialAllocationSize = 1024;
const _growAllocationSize = allocated => allocated * 2;

export const varuintMin = 0;
export const varint7Min = -0b1000000;
export const varint7Max = 0b111111;
export const varuint7Max = 0b1111111;
export const varuintMax = ((((1 << 31) >>> 0) - 1) * 2) + 1;
export const varintMin = -((1 << 31) >>> 0);
export const varintMax = ((1 << 31) - 1) >>> 0;
export const varBitsMax = 5;

const _getterRangeCheck = (llb, at, size) => {
    if (0 > at || at + size > llb._used)
        throw new RangeError(`[${at}, ${at + size}) is out of buffer range [0, ${llb._used})`);
};

const _hexdump = (buf, size) => {
    let s = "";
    const width = 16;
    const base = 16;
    for (let row = 0; row * width < size; ++row) {
        const address = (row * width).toString(base);
        s += "0".repeat(8 - address.length) + address;
        let chars = "";
        for (let col = 0; col !== width; ++col) {
            const idx = row * width + col;
            if (idx < size) {
                const byte = buf[idx];
                const bytestr = byte.toString(base);
                s += " " + (bytestr.length === 1 ? "0" + bytestr : bytestr);
                chars += 0x20 <= byte && byte < 0x7F ? String.fromCharCode(byte) : "·";
            } else {
                s += "   ";
                chars += " ";
            }
        }
        s+= "  |" + chars + "|\n";
    }
    return s;
};

export default class LowLevelBinary {
    constructor() {
        this._buf = new Uint8Array(_initialAllocationSize);
        this._used = 0;
    }

    newPatchable(type) { return new PatchableLowLevelBinary(type, this); }

    // Utilities.
    get() { return this._buf; }
    hexdump() { return _hexdump(this._buf, this._used); }
    _maybeGrow(bytes) {
        const allocated = this._buf.length;
        if (allocated - this._used < bytes) {
            let buf = new Uint8Array(_growAllocationSize(allocated));
            buf.set(this._buf);
            this._buf = buf;
        }
    }
    _push8(v) { this._buf[this._used] = v & 0xFF; this._used += 1; }

    // Data types.
    uint8(v) {
        if ((v & 0xFF) >>> 0 !== v)
            throw new RangeError(`Invalid uint8 ${v}`);
        this._maybeGrow(1);
        this._push8(v);
    }
    uint16(v) {
        if ((v & 0xFFFF) >>> 0 !== v)
            throw new RangeError(`Invalid uint16 ${v}`);
        this._maybeGrow(2);
        this._push8(v);
        this._push8(v >>> 8);
    }
    uint32(v) {
        if ((v & 0xFFFFFFFF) >>> 0 !== v)
            throw new RangeError(`Invalid uint32 ${v}`);
        this._maybeGrow(4);
        this._push8(v);
        this._push8(v >>> 8);
        this._push8(v >>> 16);
        this._push8(v >>> 24);
    }
    varuint(v) {
        if (v < varuintMin || varuintMax < v)
            throw new RangeError(`Invalid varuint ${v} range is [${varuintMin}, ${varuintMax}]`);
        while (v >= 0x80) {
            this.uint8(0x80 | (v & 0x7F));
            v >>>= 7;
        }
        this.uint8(v);
    }
    varint(v) {
        if (v < varintMin || varintMax < v)
            throw new RangeError(`Invalid varint ${v} range is [${varintMin}, ${varintMax}]`);
        do {
            const b = v & 0x7F;
            v >>= 7;
            if ((v === 0 && ((b & 0x40) === 0)) || (v === -1 && ((b & 0x40) === 0x40))) {
                this.uint8(b & 0x7F);
                break;
            }
            this.uint8(0x80 | b);
        } while (true);
    }
    varuint1(v) {
        if (v !== 0 && v !== 1)
            throw new RangeError(`Invalid varuint1 ${v} range is [0, 1]`);
        this.varuint(v);
    }
    varint7(v) {
        if (v < varint7Min || varint7Max < v)
            throw new RangeError(`Invalid varint7 ${v} range is [${varint7Min}, ${varint7Max}]`);
        this.varint(v);
    }
    varuint7(v) {
        if (v < varuintMin || varuint7Max < v)
            throw new RangeError(`Invalid varuint7 ${v} range is [${varuintMin}, ${varuint7Max}]`);
        this.varuint(v);
    }
    string(str) {
        let patch = this.newPatchable("varuint");
        for (const char of str)
            patch.uint16(char.charCodeAt());
        patch.apply();
    }

    // Getters.
    getSize() { return this._used; }
    getUint8(at) {
        _getterRangeCheck(this, at, 1);
        return this._buf[at];
    }
    getUint16(at) {
        _getterRangeCheck(this, at, 2);
        return this._buf[at] | (this._buf[at + 1] << 8);
    }
    getUint32(at) {
        _getterRangeCheck(this, at, 4);
        return (this._buf[at] | (this._buf[at + 1] << 8) | (this._buf[at + 2] << 16) | (this._buf[at + 3] << 24)) >>> 0;
    }
    getVaruint(at) {
        let v = 0;
        let shift = 0;
        let byte = 0;
        do {
            byte = this.getUint8(at++);
            ++read;
            v = (v | ((byte & 0x7F) << shift) >>> 0) >>> 0;
            shift += 7;
        } while ((byte & 0x80) !== 0);
        if (shift - 7 > 32) throw new RangeError(`Shifting too much at ${at}`);
        if ((shift == 35) && ((byte & 0xF0) != 0)) throw new Error(`Unexpected non-significant varuint bits in last byte 0x${byte.toString(16)}`);
        return { value: v, next: at };
    }
    getVarint(at) {
        let v = 0;
        let shift = 0;
        let byte = 0;
        do {
            byte = this.getUint8(at++);
            v = (v | ((byte & 0x7F) << shift) >>> 0) >>> 0;
            shift += 7;
        } while ((byte & 0x80) !== 0);
        if (shift - 7 > 32) throw new RangeError(`Shifting too much at ${at}`);
        if ((shift == 35) && (((byte << 26) >> 30) != ((byte << 25) >> 31))) throw new Error(`Unexpected non-significant varint bits in last byte 0x${byte.toString(16)}`);
        if ((byte & 0x40) === 0x40) {
            const sext = shift < 32 ? 32 - shift : 0;
            v = (v << sext) >> sext;
        }
        return { value: v, next: at };
    }
    getVaruint7(at) {
        const res = this.getVaruint(at);
        if (res.value > varuint7Max) throw new Error(`Expected a varuint7, got value ${res.value}`);
        return res;
    }
    getString(at) {
        const size = this.getVaruint(at);
        let str = "";
        for (let i = size.next; i !== size.next + size.value; i += 2)
            str += String.fromCharCode(this.getUint16(i));
        return str;
    }
};

class PatchableLowLevelBinary extends LowLevelBinary {
    constructor(type, lowLevelBinary) {
        super();
        this.type = type;
        this.target = lowLevelBinary;
        this._buffered_bytes = 0;
    }
    _push8(v) { ++this._buffered_bytes; super._push8(v); }
    apply() {
        this.target[this.type](this._buffered_bytes);
        for (let i = 0; i < this._buffered_bytes; ++i)
            this.target.uint8(this._buf[i]);
    }
};
