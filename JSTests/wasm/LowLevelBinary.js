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

import * as assert from './assert.js';
import * as WASM from './WASM.js';

const _initialAllocationSize = 1024;
const _growAllocationSize = allocated => allocated * 2;

export const varuint32Min = 0;
export const varint7Min = -0b1000000;
export const varint7Max = 0b111111;
export const varuint7Max = 0b1111111;
export const varuint32Max = ((((1 << 31) >>> 0) - 1) * 2) + 1;
export const varint32Min = -((1 << 31) >>> 0);
export const varint32Max = ((1 << 31) - 1) >>> 0;
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
    trim() { this._buf = this._buf.slice(0, this._used); }

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
    uint24(v) {
        if ((v & 0xFFFFFF) >>> 0 !== v)
            throw new RangeError(`Invalid uint24 ${v}`);
        this._maybeGrow(3);
        this._push8(v);
        this._push8(v >>> 8);
        this._push8(v >>> 16);
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
    float(v) {
        if (isNaN(v))
            throw new RangeError("unimplemented, NaNs");
        // Unfortunately, we cannot just view the actual buffer as a Float32Array since it needs to be 4 byte aligned
        let buffer = new ArrayBuffer(4);
        let floatView = new Float32Array(buffer);
        let int8View = new Uint8Array(buffer);
        floatView[0] = v;
        for (let byte of int8View)
            this._push8(byte);
    }

    double(v) {
        if (isNaN(v))
            throw new RangeError("unimplemented, NaNs");
        // Unfortunately, we cannot just view the actual buffer as a Float64Array since it needs to be 4 byte aligned
        let buffer = new ArrayBuffer(8);
        let floatView = new Float64Array(buffer);
        let int8View = new Uint8Array(buffer);
        floatView[0] = v;
        for (let byte of int8View)
            this._push8(byte);
    }

    varuint32(v) {
        assert.isNumber(v);
        if (v < varuint32Min || varuint32Max < v)
            throw new RangeError(`Invalid varuint32 ${v} range is [${varuint32Min}, ${varuint32Max}]`);
        while (v >= 0x80) {
            this.uint8(0x80 | (v & 0x7F));
            v >>>= 7;
        }
        this.uint8(v);
    }
    varint32(v) {
        assert.isNumber(v);
        if (v < varint32Min || varint32Max < v)
            throw new RangeError(`Invalid varint32 ${v} range is [${varint32Min}, ${varint32Max}]`);
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
    varuint64(v) {
        assert.isNumber(v);
        if (v < varuint32Min || varuint32Max < v)
            throw new RangeError(`unimplemented: varuint64 larger than 32-bit`);
        this.varuint32(v); // FIXME implement 64-bit var{u}int https://bugs.webkit.org/show_bug.cgi?id=163420
    }
    varint64(v) {
        assert.isNumber(v);
        if (v < varint32Min || varint32Max < v)
            throw new RangeError(`unimplemented: varint64 larger than 32-bit`);
        this.varint32(v); // FIXME implement 64-bit var{u}int https://bugs.webkit.org/show_bug.cgi?id=163420
    }
    varuint1(v) {
        if (v !== 0 && v !== 1)
            throw new RangeError(`Invalid varuint1 ${v} range is [0, 1]`);
        this.varuint32(v);
    }
    varint7(v) {
        if (v < varint7Min || varint7Max < v)
            throw new RangeError(`Invalid varint7 ${v} range is [${varint7Min}, ${varint7Max}]`);
        this.varint32(v);
    }
    varuint7(v) {
        if (v < varuint32Min || varuint7Max < v)
            throw new RangeError(`Invalid varuint7 ${v} range is [${varuint32Min}, ${varuint7Max}]`);
        this.varuint32(v);
    }
    block_type(v) {
        if (!WASM.isValidBlockType(v))
            throw new Error(`Invalid block type ${v}`);
        this.varint7(WASM.typeValue[v]);
    }
    string(str) {
        let patch = this.newPatchable("varuint32");
        for (const char of str) {
            // Encode UTF-8 2003 code points.
            const code = char.codePointAt();
            if (code <= 0x007F) {
                const utf8 = code;
                patch.uint8(utf8);
            } else if (code <= 0x07FF) {
                const utf8 = 0x80C0 | ((code & 0x7C0) >> 6) | ((code & 0x3F) << 8);
                patch.uint16(utf8);
            } else if (code <= 0xFFFF) {
                const utf8 = 0x8080E0 | ((code & 0xF000) >> 12) | ((code & 0xFC0) << 2) | ((code & 0x3F) << 16);
                patch.uint24(utf8);
            } else if (code <= 0x10FFFF) {
                const utf8 = (0x808080F0 | ((code & 0x1C0000) >> 18) | ((code & 0x3F000) >> 4) | ((code & 0xFC0) << 10) | ((code & 0x3F) << 24)) >>> 0;
                patch.uint32(utf8);
            } else
                throw new Error(`Unexpectedly large UTF-8 character code point '${char}' 0x${code.toString(16)}`);
        }
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
    getUint24(at) {
        _getterRangeCheck(this, at, 3);
        return this._buf[at] | (this._buf[at + 1] << 8) | (this._buf[at + 2] << 16);
    }
    getUint32(at) {
        _getterRangeCheck(this, at, 4);
        return (this._buf[at] | (this._buf[at + 1] << 8) | (this._buf[at + 2] << 16) | (this._buf[at + 3] << 24)) >>> 0;
    }
    getVaruint32(at) {
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
        if ((shift == 35) && ((byte & 0xF0) != 0)) throw new Error(`Unexpected non-significant varuint32 bits in last byte 0x${byte.toString(16)}`);
        return { value: v, next: at };
    }
    getVarint32(at) {
        let v = 0;
        let shift = 0;
        let byte = 0;
        do {
            byte = this.getUint8(at++);
            v = (v | ((byte & 0x7F) << shift) >>> 0) >>> 0;
            shift += 7;
        } while ((byte & 0x80) !== 0);
        if (shift - 7 > 32) throw new RangeError(`Shifting too much at ${at}`);
        if ((shift == 35) && (((byte << 26) >> 30) != ((byte << 25) >> 31))) throw new Error(`Unexpected non-significant varint32 bits in last byte 0x${byte.toString(16)}`);
        if ((byte & 0x40) === 0x40) {
            const sext = shift < 32 ? 32 - shift : 0;
            v = (v << sext) >> sext;
        }
        return { value: v, next: at };
    }
    getVaruint1(at) {
        const res = this.getVaruint32(at);
        if (res.value !== 0 && res.value !== 1) throw new Error(`Expected a varuint1, got value ${res.value}`);
        return res;
    }
    getVaruint7(at) {
        const res = this.getVaruint32(at);
        if (res.value > varuint7Max) throw new Error(`Expected a varuint7, got value ${res.value}`);
        return res;
    }
    getString(at) {
        const size = this.getVaruint32(at);
        const last = size.next + size.value;
        let i = size.next;
        let str = "";
        while (i < last) {
            // Decode UTF-8 2003 code points.
            const peek = this.getUint8(i);
            let code;
            if ((peek & 0x80) === 0x0) {
                const utf8 = this.getUint8(i);
                assert.eq(utf8 & 0x80, 0x00);
                i += 1;
                code = utf8;
            } else if ((peek & 0xE0) === 0xC0) {
                const utf8 = this.getUint16(i);
                assert.eq(utf8 & 0xC0E0, 0x80C0);
                i += 2;
                code = ((utf8 & 0x1F) << 6) | ((utf8 & 0x3F00) >> 8);
            } else if ((peek & 0xF0) === 0xE0) {
                const utf8 = this.getUint24(i);
                assert.eq(utf8 & 0xC0C0F0, 0x8080E0);
                i += 3;
                code = ((utf8 & 0xF) << 12) | ((utf8 & 0x3F00) >> 2) | ((utf8 & 0x3F0000) >> 16);
            } else if ((peek & 0xF8) === 0xF0) {
                const utf8 = this.getUint32(i);
                assert.eq((utf8 & 0xC0C0C0F8) | 0, 0x808080F0 | 0);
                i += 4;
                code = ((utf8 & 0x7) << 18) | ((utf8 & 0x3F00) << 4) | ((utf8 & 0x3F0000) >> 10) | ((utf8 & 0x3F000000) >> 24);
            } else
                throw new Error(`Unexpectedly large UTF-8 initial byte 0x${peek.toString(16)}`);
            str += String.fromCodePoint(code);
        }
        if (i !== last)
            throw new Error(`String decoding read up to ${i}, expected ${last}, UTF-8 decoding was too greedy`);
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
