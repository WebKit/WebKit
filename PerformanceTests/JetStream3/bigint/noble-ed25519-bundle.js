// The MIT License (MIT)

// Copyright (c) 2019 Paul Miller (https://paulmillr.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the “Software”), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

(function(){function r(e,n,t){function o(i,f){if(!n[i]){if(!e[i]){var c="function"==typeof require&&require;if(!f&&c)return c(i,!0);if(u)return u(i,!0);var a=new Error("Cannot find module '"+i+"'");throw a.code="MODULE_NOT_FOUND",a}var p=n[i]={exports:{}};e[i][0].call(p.exports,function(r){var n=e[i][1][r];return o(n||r)},p,p.exports,r,e,n,t)}return n[i].exports}for(var u="function"==typeof require&&require,i=0;i<t.length;i++)o(t[i]);return o}return r})()({1:[function(require,module,exports){
"use strict";
/*! noble-ed25519 - MIT License (c) 2019 Paul Miller (paulmillr.com) */
Object.defineProperty(exports, "__esModule", { value: true });
exports.utils = exports.curve25519 = exports.getSharedSecret = exports.sync = exports.verify = exports.sign = exports.getPublicKey = exports.Signature = exports.Point = exports.RistrettoPoint = exports.ExtendedPoint = exports.CURVE = void 0;
const nodeCrypto = require("crypto");
const _0n = BigInt(0);
const _1n = BigInt(1);
const _2n = BigInt(2);
const CU_O = BigInt('7237005577332262213973186563042994240857116359379907606001950938285454250989');
const CURVE = Object.freeze({
    a: BigInt(-1),
    d: BigInt('37095705934669439343138083508754565189542113879843219016388785533085940283555'),
    P: BigInt('57896044618658097711785492504343953926634992332820282019728792003956564819949'),
    l: CU_O,
    n: CU_O,
    h: BigInt(8),
    Gx: BigInt('15112221349535400772501151409588531511454012693041857206046113283949847762202'),
    Gy: BigInt('46316835694926478169428394003475163141307993866256225615783033603165251855960'),
});
exports.CURVE = CURVE;
const POW_2_256 = BigInt('0x10000000000000000000000000000000000000000000000000000000000000000');
const SQRT_M1 = BigInt('19681161376707505956807079304988542015446066515923890162744021073123829784752');
const SQRT_D = BigInt('6853475219497561581579357271197624642482790079785650197046958215289687604742');
const SQRT_AD_MINUS_ONE = BigInt('25063068953384623474111414158702152701244531502492656460079210482610430750235');
const INVSQRT_A_MINUS_D = BigInt('54469307008909316920995813868745141605393597292927456921205312896311721017578');
const ONE_MINUS_D_SQ = BigInt('1159843021668779879193775521855586647937357759715417654439879720876111806838');
const D_MINUS_ONE_SQ = BigInt('40440834346308536858101042469323190826248399146238708352240133220865137265952');
class ExtendedPoint {
    constructor(x, y, z, t) {
        this.x = x;
        this.y = y;
        this.z = z;
        this.t = t;
    }
    static fromAffine(p) {
        if (!(p instanceof Point)) {
            throw new TypeError('ExtendedPoint#fromAffine: expected Point');
        }
        if (p.equals(Point.ZERO))
            return ExtendedPoint.ZERO;
        return new ExtendedPoint(p.x, p.y, _1n, mod(p.x * p.y));
    }
    static toAffineBatch(points) {
        const toInv = invertBatch(points.map((p) => p.z));
        return points.map((p, i) => p.toAffine(toInv[i]));
    }
    static normalizeZ(points) {
        return this.toAffineBatch(points).map(this.fromAffine);
    }
    equals(other) {
        assertExtPoint(other);
        const { x: X1, y: Y1, z: Z1 } = this;
        const { x: X2, y: Y2, z: Z2 } = other;
        const X1Z2 = mod(X1 * Z2);
        const X2Z1 = mod(X2 * Z1);
        const Y1Z2 = mod(Y1 * Z2);
        const Y2Z1 = mod(Y2 * Z1);
        return X1Z2 === X2Z1 && Y1Z2 === Y2Z1;
    }
    negate() {
        return new ExtendedPoint(mod(-this.x), this.y, this.z, mod(-this.t));
    }
    double() {
        const { x: X1, y: Y1, z: Z1 } = this;
        const { a } = CURVE;
        const A = mod(X1 * X1);
        const B = mod(Y1 * Y1);
        const C = mod(_2n * mod(Z1 * Z1));
        const D = mod(a * A);
        const x1y1 = X1 + Y1;
        const E = mod(mod(x1y1 * x1y1) - A - B);
        const G = D + B;
        const F = G - C;
        const H = D - B;
        const X3 = mod(E * F);
        const Y3 = mod(G * H);
        const T3 = mod(E * H);
        const Z3 = mod(F * G);
        return new ExtendedPoint(X3, Y3, Z3, T3);
    }
    add(other) {
        assertExtPoint(other);
        const { x: X1, y: Y1, z: Z1, t: T1 } = this;
        const { x: X2, y: Y2, z: Z2, t: T2 } = other;
        const A = mod((Y1 - X1) * (Y2 + X2));
        const B = mod((Y1 + X1) * (Y2 - X2));
        const F = mod(B - A);
        if (F === _0n)
            return this.double();
        const C = mod(Z1 * _2n * T2);
        const D = mod(T1 * _2n * Z2);
        const E = D + C;
        const G = B + A;
        const H = D - C;
        const X3 = mod(E * F);
        const Y3 = mod(G * H);
        const T3 = mod(E * H);
        const Z3 = mod(F * G);
        return new ExtendedPoint(X3, Y3, Z3, T3);
    }
    subtract(other) {
        return this.add(other.negate());
    }
    precomputeWindow(W) {
        const windows = 1 + 256 / W;
        const points = [];
        let p = this;
        let base = p;
        for (let window = 0; window < windows; window++) {
            base = p;
            points.push(base);
            for (let i = 1; i < 2 ** (W - 1); i++) {
                base = base.add(p);
                points.push(base);
            }
            p = base.double();
        }
        return points;
    }
    wNAF(n, affinePoint) {
        if (!affinePoint && this.equals(ExtendedPoint.BASE))
            affinePoint = Point.BASE;
        const W = (affinePoint && affinePoint._WINDOW_SIZE) || 1;
        if (256 % W) {
            throw new Error('Point#wNAF: Invalid precomputation window, must be power of 2');
        }
        let precomputes = affinePoint && pointPrecomputes.get(affinePoint);
        if (!precomputes) {
            precomputes = this.precomputeWindow(W);
            if (affinePoint && W !== 1) {
                precomputes = ExtendedPoint.normalizeZ(precomputes);
                pointPrecomputes.set(affinePoint, precomputes);
            }
        }
        let p = ExtendedPoint.ZERO;
        let f = ExtendedPoint.ZERO;
        const windows = 1 + 256 / W;
        const windowSize = 2 ** (W - 1);
        const mask = BigInt(2 ** W - 1);
        const maxNumber = 2 ** W;
        const shiftBy = BigInt(W);
        for (let window = 0; window < windows; window++) {
            const offset = window * windowSize;
            let wbits = Number(n & mask);
            n >>= shiftBy;
            if (wbits > windowSize) {
                wbits -= maxNumber;
                n += _1n;
            }
            if (wbits === 0) {
                let pr = precomputes[offset];
                if (window % 2)
                    pr = pr.negate();
                f = f.add(pr);
            }
            else {
                let cached = precomputes[offset + Math.abs(wbits) - 1];
                if (wbits < 0)
                    cached = cached.negate();
                p = p.add(cached);
            }
        }
        return ExtendedPoint.normalizeZ([p, f])[0];
    }
    multiply(scalar, affinePoint) {
        return this.wNAF(normalizeScalar(scalar, CURVE.l), affinePoint);
    }
    multiplyUnsafe(scalar) {
        let n = normalizeScalar(scalar, CURVE.l, false);
        const G = ExtendedPoint.BASE;
        const P0 = ExtendedPoint.ZERO;
        if (n === _0n)
            return P0;
        if (this.equals(P0) || n === _1n)
            return this;
        if (this.equals(G))
            return this.wNAF(n);
        let p = P0;
        let d = this;
        while (n > _0n) {
            if (n & _1n)
                p = p.add(d);
            d = d.double();
            n >>= _1n;
        }
        return p;
    }
    isSmallOrder() {
        return this.multiplyUnsafe(CURVE.h).equals(ExtendedPoint.ZERO);
    }
    isTorsionFree() {
        return this.multiplyUnsafe(CURVE.l).equals(ExtendedPoint.ZERO);
    }
    toAffine(invZ = invert(this.z)) {
        const { x, y, z } = this;
        const ax = mod(x * invZ);
        const ay = mod(y * invZ);
        const zz = mod(z * invZ);
        if (zz !== _1n)
            throw new Error('invZ was invalid');
        return new Point(ax, ay);
    }
    fromRistrettoBytes() {
        legacyRist();
    }
    toRistrettoBytes() {
        legacyRist();
    }
    fromRistrettoHash() {
        legacyRist();
    }
}
exports.ExtendedPoint = ExtendedPoint;
ExtendedPoint.BASE = new ExtendedPoint(CURVE.Gx, CURVE.Gy, _1n, mod(CURVE.Gx * CURVE.Gy));
ExtendedPoint.ZERO = new ExtendedPoint(_0n, _1n, _1n, _0n);
function assertExtPoint(other) {
    if (!(other instanceof ExtendedPoint))
        throw new TypeError('ExtendedPoint expected');
}
function assertRstPoint(other) {
    if (!(other instanceof RistrettoPoint))
        throw new TypeError('RistrettoPoint expected');
}
function legacyRist() {
    throw new Error('Legacy method: switch to RistrettoPoint');
}
class RistrettoPoint {
    constructor(ep) {
        this.ep = ep;
    }
    static calcElligatorRistrettoMap(r0) {
        const { d } = CURVE;
        const r = mod(SQRT_M1 * r0 * r0);
        const Ns = mod((r + _1n) * ONE_MINUS_D_SQ);
        let c = BigInt(-1);
        const D = mod((c - d * r) * mod(r + d));
        let { isValid: Ns_D_is_sq, value: s } = uvRatio(Ns, D);
        let s_ = mod(s * r0);
        if (!edIsNegative(s_))
            s_ = mod(-s_);
        if (!Ns_D_is_sq)
            s = s_;
        if (!Ns_D_is_sq)
            c = r;
        const Nt = mod(c * (r - _1n) * D_MINUS_ONE_SQ - D);
        const s2 = s * s;
        const W0 = mod((s + s) * D);
        const W1 = mod(Nt * SQRT_AD_MINUS_ONE);
        const W2 = mod(_1n - s2);
        const W3 = mod(_1n + s2);
        return new ExtendedPoint(mod(W0 * W3), mod(W2 * W1), mod(W1 * W3), mod(W0 * W2));
    }
    static hashToCurve(hex) {
        hex = ensureBytes(hex, 64);
        const r1 = bytes255ToNumberLE(hex.slice(0, 32));
        const R1 = this.calcElligatorRistrettoMap(r1);
        const r2 = bytes255ToNumberLE(hex.slice(32, 64));
        const R2 = this.calcElligatorRistrettoMap(r2);
        return new RistrettoPoint(R1.add(R2));
    }
    static fromHex(hex) {
        hex = ensureBytes(hex, 32);
        const { a, d } = CURVE;
        const emsg = 'RistrettoPoint.fromHex: the hex is not valid encoding of RistrettoPoint';
        const s = bytes255ToNumberLE(hex);
        if (!equalBytes(numberTo32BytesLE(s), hex) || edIsNegative(s))
            throw new Error(emsg);
        const s2 = mod(s * s);
        const u1 = mod(_1n + a * s2);
        const u2 = mod(_1n - a * s2);
        const u1_2 = mod(u1 * u1);
        const u2_2 = mod(u2 * u2);
        const v = mod(a * d * u1_2 - u2_2);
        const { isValid, value: I } = invertSqrt(mod(v * u2_2));
        const Dx = mod(I * u2);
        const Dy = mod(I * Dx * v);
        let x = mod((s + s) * Dx);
        if (edIsNegative(x))
            x = mod(-x);
        const y = mod(u1 * Dy);
        const t = mod(x * y);
        if (!isValid || edIsNegative(t) || y === _0n)
            throw new Error(emsg);
        return new RistrettoPoint(new ExtendedPoint(x, y, _1n, t));
    }
    toRawBytes() {
        let { x, y, z, t } = this.ep;
        const u1 = mod(mod(z + y) * mod(z - y));
        const u2 = mod(x * y);
        const u2sq = mod(u2 * u2);
        const { value: invsqrt } = invertSqrt(mod(u1 * u2sq));
        const D1 = mod(invsqrt * u1);
        const D2 = mod(invsqrt * u2);
        const zInv = mod(D1 * D2 * t);
        let D;
        if (edIsNegative(t * zInv)) {
            let _x = mod(y * SQRT_M1);
            let _y = mod(x * SQRT_M1);
            x = _x;
            y = _y;
            D = mod(D1 * INVSQRT_A_MINUS_D);
        }
        else {
            D = D2;
        }
        if (edIsNegative(x * zInv))
            y = mod(-y);
        let s = mod((z - y) * D);
        if (edIsNegative(s))
            s = mod(-s);
        return numberTo32BytesLE(s);
    }
    toHex() {
        return bytesToHex(this.toRawBytes());
    }
    toString() {
        return this.toHex();
    }
    equals(other) {
        assertRstPoint(other);
        const a = this.ep;
        const b = other.ep;
        const one = mod(a.x * b.y) === mod(a.y * b.x);
        const two = mod(a.y * b.y) === mod(a.x * b.x);
        return one || two;
    }
    add(other) {
        assertRstPoint(other);
        return new RistrettoPoint(this.ep.add(other.ep));
    }
    subtract(other) {
        assertRstPoint(other);
        return new RistrettoPoint(this.ep.subtract(other.ep));
    }
    multiply(scalar) {
        return new RistrettoPoint(this.ep.multiply(scalar));
    }
    multiplyUnsafe(scalar) {
        return new RistrettoPoint(this.ep.multiplyUnsafe(scalar));
    }
}
exports.RistrettoPoint = RistrettoPoint;
RistrettoPoint.BASE = new RistrettoPoint(ExtendedPoint.BASE);
RistrettoPoint.ZERO = new RistrettoPoint(ExtendedPoint.ZERO);
const pointPrecomputes = new WeakMap();
class Point {
    constructor(x, y) {
        this.x = x;
        this.y = y;
    }
    _setWindowSize(windowSize) {
        this._WINDOW_SIZE = windowSize;
        pointPrecomputes.delete(this);
    }
    static fromHex(hex, strict = true) {
        const { d, P } = CURVE;
        hex = ensureBytes(hex, 32);
        const normed = hex.slice();
        normed[31] = hex[31] & ~0x80;
        const y = bytesToNumberLE(normed);
        if (strict && y >= P)
            throw new Error('Expected 0 < hex < P');
        if (!strict && y >= POW_2_256)
            throw new Error('Expected 0 < hex < 2**256');
        const y2 = mod(y * y);
        const u = mod(y2 - _1n);
        const v = mod(d * y2 + _1n);
        let { isValid, value: x } = uvRatio(u, v);
        if (!isValid)
            throw new Error('Point.fromHex: invalid y coordinate');
        const isXOdd = (x & _1n) === _1n;
        const isLastByteOdd = (hex[31] & 0x80) !== 0;
        if (isLastByteOdd !== isXOdd) {
            x = mod(-x);
        }
        return new Point(x, y);
    }
    static async fromPrivateKey(privateKey) {
        return (await getExtendedPublicKey(privateKey)).point;
    }
    toRawBytes() {
        const bytes = numberTo32BytesLE(this.y);
        bytes[31] |= this.x & _1n ? 0x80 : 0;
        return bytes;
    }
    toHex() {
        return bytesToHex(this.toRawBytes());
    }
    toX25519() {
        const { y } = this;
        const u = mod((_1n + y) * invert(_1n - y));
        return numberTo32BytesLE(u);
    }
    isTorsionFree() {
        return ExtendedPoint.fromAffine(this).isTorsionFree();
    }
    equals(other) {
        return this.x === other.x && this.y === other.y;
    }
    negate() {
        return new Point(mod(-this.x), this.y);
    }
    add(other) {
        return ExtendedPoint.fromAffine(this).add(ExtendedPoint.fromAffine(other)).toAffine();
    }
    subtract(other) {
        return this.add(other.negate());
    }
    multiply(scalar) {
        return ExtendedPoint.fromAffine(this).multiply(scalar, this).toAffine();
    }
}
exports.Point = Point;
Point.BASE = new Point(CURVE.Gx, CURVE.Gy);
Point.ZERO = new Point(_0n, _1n);
class Signature {
    constructor(r, s) {
        this.r = r;
        this.s = s;
        this.assertValidity();
    }
    static fromHex(hex) {
        const bytes = ensureBytes(hex, 64);
        const r = Point.fromHex(bytes.slice(0, 32), false);
        const s = bytesToNumberLE(bytes.slice(32, 64));
        return new Signature(r, s);
    }
    assertValidity() {
        const { r, s } = this;
        if (!(r instanceof Point))
            throw new Error('Expected Point instance');
        normalizeScalar(s, CURVE.l, false);
        return this;
    }
    toRawBytes() {
        const u8 = new Uint8Array(64);
        u8.set(this.r.toRawBytes());
        u8.set(numberTo32BytesLE(this.s), 32);
        return u8;
    }
    toHex() {
        return bytesToHex(this.toRawBytes());
    }
}
exports.Signature = Signature;
function concatBytes(...arrays) {
    if (!arrays.every((a) => a instanceof Uint8Array))
        throw new Error('Expected Uint8Array list');
    if (arrays.length === 1)
        return arrays[0];
    const length = arrays.reduce((a, arr) => a + arr.length, 0);
    const result = new Uint8Array(length);
    for (let i = 0, pad = 0; i < arrays.length; i++) {
        const arr = arrays[i];
        result.set(arr, pad);
        pad += arr.length;
    }
    return result;
}
const hexes = Array.from({ length: 256 }, (v, i) => i.toString(16).padStart(2, '0'));
function bytesToHex(uint8a) {
    if (!(uint8a instanceof Uint8Array))
        throw new Error('Uint8Array expected');
    let hex = '';
    for (let i = 0; i < uint8a.length; i++) {
        hex += hexes[uint8a[i]];
    }
    return hex;
}
function hexToBytes(hex) {
    if (typeof hex !== 'string') {
        throw new TypeError('hexToBytes: expected string, got ' + typeof hex);
    }
    if (hex.length % 2)
        throw new Error('hexToBytes: received invalid unpadded hex');
    const array = new Uint8Array(hex.length / 2);
    for (let i = 0; i < array.length; i++) {
        const j = i * 2;
        const hexByte = hex.slice(j, j + 2);
        const byte = Number.parseInt(hexByte, 16);
        if (Number.isNaN(byte) || byte < 0)
            throw new Error('Invalid byte sequence');
        array[i] = byte;
    }
    return array;
}
function numberTo32BytesBE(num) {
    const length = 32;
    const hex = num.toString(16).padStart(length * 2, '0');
    return hexToBytes(hex);
}
function numberTo32BytesLE(num) {
    return numberTo32BytesBE(num).reverse();
}
function edIsNegative(num) {
    return (mod(num) & _1n) === _1n;
}
function bytesToNumberLE(uint8a) {
    if (!(uint8a instanceof Uint8Array))
        throw new Error('Expected Uint8Array');
    return BigInt('0x' + bytesToHex(Uint8Array.from(uint8a).reverse()));
}
const MAX_255B = BigInt('0x7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff');
function bytes255ToNumberLE(bytes) {
    return mod(bytesToNumberLE(bytes) & MAX_255B);
}
function mod(a, b = CURVE.P) {
    const res = a % b;
    return res >= _0n ? res : b + res;
}
function invert(number, modulo = CURVE.P) {
    if (number === _0n || modulo <= _0n) {
        throw new Error(`invert: expected positive integers, got n=${number} mod=${modulo}`);
    }
    let a = mod(number, modulo);
    let b = modulo;
    let x = _0n, y = _1n, u = _1n, v = _0n;
    while (a !== _0n) {
        const q = b / a;
        const r = b % a;
        const m = x - u * q;
        const n = y - v * q;
        b = a, a = r, x = u, y = v, u = m, v = n;
    }
    const gcd = b;
    if (gcd !== _1n)
        throw new Error('invert: does not exist');
    return mod(x, modulo);
}
function invertBatch(nums, p = CURVE.P) {
    const tmp = new Array(nums.length);
    const lastMultiplied = nums.reduce((acc, num, i) => {
        if (num === _0n)
            return acc;
        tmp[i] = acc;
        return mod(acc * num, p);
    }, _1n);
    const inverted = invert(lastMultiplied, p);
    nums.reduceRight((acc, num, i) => {
        if (num === _0n)
            return acc;
        tmp[i] = mod(acc * tmp[i], p);
        return mod(acc * num, p);
    }, inverted);
    return tmp;
}
function pow2(x, power) {
    const { P } = CURVE;
    let res = x;
    while (power-- > _0n) {
        res *= res;
        res %= P;
    }
    return res;
}
function pow_2_252_3(x) {
    const { P } = CURVE;
    const _5n = BigInt(5);
    const _10n = BigInt(10);
    const _20n = BigInt(20);
    const _40n = BigInt(40);
    const _80n = BigInt(80);
    const x2 = (x * x) % P;
    const b2 = (x2 * x) % P;
    const b4 = (pow2(b2, _2n) * b2) % P;
    const b5 = (pow2(b4, _1n) * x) % P;
    const b10 = (pow2(b5, _5n) * b5) % P;
    const b20 = (pow2(b10, _10n) * b10) % P;
    const b40 = (pow2(b20, _20n) * b20) % P;
    const b80 = (pow2(b40, _40n) * b40) % P;
    const b160 = (pow2(b80, _80n) * b80) % P;
    const b240 = (pow2(b160, _80n) * b80) % P;
    const b250 = (pow2(b240, _10n) * b10) % P;
    const pow_p_5_8 = (pow2(b250, _2n) * x) % P;
    return { pow_p_5_8, b2 };
}
function uvRatio(u, v) {
    const v3 = mod(v * v * v);
    const v7 = mod(v3 * v3 * v);
    const pow = pow_2_252_3(u * v7).pow_p_5_8;
    let x = mod(u * v3 * pow);
    const vx2 = mod(v * x * x);
    const root1 = x;
    const root2 = mod(x * SQRT_M1);
    const useRoot1 = vx2 === u;
    const useRoot2 = vx2 === mod(-u);
    const noRoot = vx2 === mod(-u * SQRT_M1);
    if (useRoot1)
        x = root1;
    if (useRoot2 || noRoot)
        x = root2;
    if (edIsNegative(x))
        x = mod(-x);
    return { isValid: useRoot1 || useRoot2, value: x };
}
function invertSqrt(number) {
    return uvRatio(_1n, number);
}
function modlLE(hash) {
    return mod(bytesToNumberLE(hash), CURVE.l);
}
function equalBytes(b1, b2) {
    if (b1.length !== b2.length) {
        return false;
    }
    for (let i = 0; i < b1.length; i++) {
        if (b1[i] !== b2[i]) {
            return false;
        }
    }
    return true;
}
function ensureBytes(hex, expectedLength) {
    const bytes = hex instanceof Uint8Array ? Uint8Array.from(hex) : hexToBytes(hex);
    if (typeof expectedLength === 'number' && bytes.length !== expectedLength)
        throw new Error(`Expected ${expectedLength} bytes`);
    return bytes;
}
function normalizeScalar(num, max, strict = true) {
    if (!max)
        throw new TypeError('Specify max value');
    if (typeof num === 'number' && Number.isSafeInteger(num))
        num = BigInt(num);
    if (typeof num === 'bigint' && num < max) {
        if (strict) {
            if (_0n < num)
                return num;
        }
        else {
            if (_0n <= num)
                return num;
        }
    }
    throw new TypeError('Expected valid scalar: 0 < scalar < max');
}
function adjustBytes25519(bytes) {
    bytes[0] &= 248;
    bytes[31] &= 127;
    bytes[31] |= 64;
    return bytes;
}
function decodeScalar25519(n) {
    return bytesToNumberLE(adjustBytes25519(ensureBytes(n, 32)));
}
function checkPrivateKey(key) {
    key =
        typeof key === 'bigint' || typeof key === 'number'
            ? numberTo32BytesBE(normalizeScalar(key, POW_2_256))
            : ensureBytes(key);
    if (key.length !== 32)
        throw new Error(`Expected 32 bytes`);
    return key;
}
function getKeyFromHash(hashed) {
    const head = adjustBytes25519(hashed.slice(0, 32));
    const prefix = hashed.slice(32, 64);
    const scalar = modlLE(head);
    const point = Point.BASE.multiply(scalar);
    const pointBytes = point.toRawBytes();
    return { head, prefix, scalar, point, pointBytes };
}
let _sha512Sync;
function sha512s(...m) {
    if (typeof _sha512Sync !== 'function')
        throw new Error('utils.sha512Sync must be set to use sync methods');
    return _sha512Sync(...m);
}
async function getExtendedPublicKey(key) {
    return getKeyFromHash(await exports.utils.sha512(checkPrivateKey(key)));
}
function getExtendedPublicKeySync(key) {
    return getKeyFromHash(sha512s(checkPrivateKey(key)));
}
async function getPublicKey(privateKey) {
    return (await getExtendedPublicKey(privateKey)).pointBytes;
}
exports.getPublicKey = getPublicKey;
function getPublicKeySync(privateKey) {
    return getExtendedPublicKeySync(privateKey).pointBytes;
}
async function sign(message, privateKey) {
    message = ensureBytes(message);
    const { prefix, scalar, pointBytes } = await getExtendedPublicKey(privateKey);
    const r = modlLE(await exports.utils.sha512(prefix, message));
    const R = Point.BASE.multiply(r);
    const k = modlLE(await exports.utils.sha512(R.toRawBytes(), pointBytes, message));
    const s = mod(r + k * scalar, CURVE.l);
    return new Signature(R, s).toRawBytes();
}
exports.sign = sign;
function signSync(message, privateKey) {
    message = ensureBytes(message);
    const { prefix, scalar, pointBytes } = getExtendedPublicKeySync(privateKey);
    const r = modlLE(sha512s(prefix, message));
    const R = Point.BASE.multiply(r);
    const k = modlLE(sha512s(R.toRawBytes(), pointBytes, message));
    const s = mod(r + k * scalar, CURVE.l);
    return new Signature(R, s).toRawBytes();
}
function prepareVerification(sig, message, publicKey) {
    message = ensureBytes(message);
    if (!(publicKey instanceof Point))
        publicKey = Point.fromHex(publicKey, false);
    const { r, s } = sig instanceof Signature ? sig.assertValidity() : Signature.fromHex(sig);
    const SB = ExtendedPoint.BASE.multiplyUnsafe(s);
    return { r, s, SB, pub: publicKey, msg: message };
}
function finishVerification(publicKey, r, SB, hashed) {
    const k = modlLE(hashed);
    const kA = ExtendedPoint.fromAffine(publicKey).multiplyUnsafe(k);
    const RkA = ExtendedPoint.fromAffine(r).add(kA);
    return RkA.subtract(SB).multiplyUnsafe(CURVE.h).equals(ExtendedPoint.ZERO);
}
async function verify(sig, message, publicKey) {
    const { r, SB, msg, pub } = prepareVerification(sig, message, publicKey);
    const hashed = await exports.utils.sha512(r.toRawBytes(), pub.toRawBytes(), msg);
    return finishVerification(pub, r, SB, hashed);
}
exports.verify = verify;
function verifySync(sig, message, publicKey) {
    const { r, SB, msg, pub } = prepareVerification(sig, message, publicKey);
    const hashed = sha512s(r.toRawBytes(), pub.toRawBytes(), msg);
    return finishVerification(pub, r, SB, hashed);
}
exports.sync = {
    getExtendedPublicKey: getExtendedPublicKeySync,
    getPublicKey: getPublicKeySync,
    sign: signSync,
    verify: verifySync,
};
async function getSharedSecret(privateKey, publicKey) {
    const { head } = await getExtendedPublicKey(privateKey);
    const u = Point.fromHex(publicKey).toX25519();
    return exports.curve25519.scalarMult(head, u);
}
exports.getSharedSecret = getSharedSecret;
Point.BASE._setWindowSize(8);
function cswap(swap, x_2, x_3) {
    const dummy = mod(swap * (x_2 - x_3));
    x_2 = mod(x_2 - dummy);
    x_3 = mod(x_3 + dummy);
    return [x_2, x_3];
}
function montgomeryLadder(pointU, scalar) {
    const { P } = CURVE;
    const u = normalizeScalar(pointU, P);
    const k = normalizeScalar(scalar, P);
    const a24 = BigInt(121665);
    const x_1 = u;
    let x_2 = _1n;
    let z_2 = _0n;
    let x_3 = u;
    let z_3 = _1n;
    let swap = _0n;
    let sw;
    for (let t = BigInt(255 - 1); t >= _0n; t--) {
        const k_t = (k >> t) & _1n;
        swap ^= k_t;
        sw = cswap(swap, x_2, x_3);
        x_2 = sw[0];
        x_3 = sw[1];
        sw = cswap(swap, z_2, z_3);
        z_2 = sw[0];
        z_3 = sw[1];
        swap = k_t;
        const A = x_2 + z_2;
        const AA = mod(A * A);
        const B = x_2 - z_2;
        const BB = mod(B * B);
        const E = AA - BB;
        const C = x_3 + z_3;
        const D = x_3 - z_3;
        const DA = mod(D * A);
        const CB = mod(C * B);
        const dacb = DA + CB;
        const da_cb = DA - CB;
        x_3 = mod(dacb * dacb);
        z_3 = mod(x_1 * mod(da_cb * da_cb));
        x_2 = mod(AA * BB);
        z_2 = mod(E * (AA + mod(a24 * E)));
    }
    sw = cswap(swap, x_2, x_3);
    x_2 = sw[0];
    x_3 = sw[1];
    sw = cswap(swap, z_2, z_3);
    z_2 = sw[0];
    z_3 = sw[1];
    const { pow_p_5_8, b2 } = pow_2_252_3(z_2);
    const xp2 = mod(pow2(pow_p_5_8, BigInt(3)) * b2);
    return mod(x_2 * xp2);
}
function encodeUCoordinate(u) {
    return numberTo32BytesLE(mod(u, CURVE.P));
}
function decodeUCoordinate(uEnc) {
    const u = ensureBytes(uEnc, 32);
    u[31] &= 127;
    return bytesToNumberLE(u);
}
exports.curve25519 = {
    BASE_POINT_U: '0900000000000000000000000000000000000000000000000000000000000000',
    scalarMult(privateKey, publicKey) {
        const u = decodeUCoordinate(publicKey);
        const p = decodeScalar25519(privateKey);
        const pu = montgomeryLadder(u, p);
        if (pu === _0n)
            throw new Error('Invalid private or public key received');
        return encodeUCoordinate(pu);
    },
    scalarMultBase(privateKey) {
        return exports.curve25519.scalarMult(privateKey, exports.curve25519.BASE_POINT_U);
    },
};
const crypto = {
    node: undefined,
    web: typeof self === 'object' && 'crypto' in self ? self.crypto : undefined,
};
exports.utils = {
    bytesToHex,
    hexToBytes,
    concatBytes,
    getExtendedPublicKey,
    mod,
    invert,
    TORSION_SUBGROUP: [
        '0100000000000000000000000000000000000000000000000000000000000000',
        'c7176a703d4dd84fba3c0b760d10670f2a2053fa2c39ccc64ec7fd7792ac037a',
        '0000000000000000000000000000000000000000000000000000000000000080',
        '26e8958fc2b227b045c3f489f2ef98f0d5dfac05d3c63339b13802886d53fc05',
        'ecffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f',
        '26e8958fc2b227b045c3f489f2ef98f0d5dfac05d3c63339b13802886d53fc85',
        '0000000000000000000000000000000000000000000000000000000000000000',
        'c7176a703d4dd84fba3c0b760d10670f2a2053fa2c39ccc64ec7fd7792ac03fa',
    ],
    hashToPrivateScalar: (hash) => {
        hash = ensureBytes(hash);
        if (hash.length < 40 || hash.length > 1024)
            throw new Error('Expected 40-1024 bytes of private key as per FIPS 186');
        return mod(bytesToNumberLE(hash), CURVE.l - _1n) + _1n;
    },
    randomBytes: (bytesLength = 32) => {
        if (crypto.web) {
            return crypto.web.getRandomValues(new Uint8Array(bytesLength));
        }
        else if (crypto.node) {
            const { randomBytes } = crypto.node;
            return new Uint8Array(randomBytes(bytesLength).buffer);
        }
        else {
            throw new Error("The environment doesn't have randomBytes function");
        }
    },
    randomPrivateKey: () => {
        return exports.utils.randomBytes(32);
    },
    sha512: async (...messages) => {
        const message = concatBytes(...messages);
        if (crypto.web) {
            const buffer = await crypto.web.subtle.digest('SHA-512', message);
            return new Uint8Array(buffer);
        }
        else if (crypto.node) {
            return Uint8Array.from(crypto.node.createHash('sha512').update(message).digest());
        }
        else {
            throw new Error("The environment doesn't have sha512 function");
        }
    },
    precompute(windowSize = 8, point = Point.BASE) {
        const cached = point.equals(Point.BASE) ? point : new Point(point.x, point.y);
        cached._setWindowSize(windowSize);
        cached.multiply(_2n);
        return cached;
    },
    sha512Sync: undefined,
};
Object.defineProperties(exports.utils, {
    sha512Sync: {
        configurable: false,
        get() {
            return _sha512Sync;
        },
        set(val) {
            if (!_sha512Sync)
                _sha512Sync = val;
        },
    },
});

},{"crypto":9}],2:[function(require,module,exports){
"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.output = exports.exists = exports.hash = exports.bytes = exports.bool = exports.number = void 0;
function number(n) {
    if (!Number.isSafeInteger(n) || n < 0)
        throw new Error(`Wrong positive integer: ${n}`);
}
exports.number = number;
function bool(b) {
    if (typeof b !== 'boolean')
        throw new Error(`Expected boolean, not ${b}`);
}
exports.bool = bool;
function bytes(b, ...lengths) {
    if (!(b instanceof Uint8Array))
        throw new TypeError('Expected Uint8Array');
    if (lengths.length > 0 && !lengths.includes(b.length))
        throw new TypeError(`Expected Uint8Array of length ${lengths}, not of length=${b.length}`);
}
exports.bytes = bytes;
function hash(hash) {
    if (typeof hash !== 'function' || typeof hash.create !== 'function')
        throw new Error('Hash should be wrapped by utils.wrapConstructor');
    number(hash.outputLen);
    number(hash.blockLen);
}
exports.hash = hash;
function exists(instance, checkFinished = true) {
    if (instance.destroyed)
        throw new Error('Hash instance has been destroyed');
    if (checkFinished && instance.finished)
        throw new Error('Hash#digest() has already been called');
}
exports.exists = exists;
function output(out, instance) {
    bytes(out);
    const min = instance.outputLen;
    if (out.length < min) {
        throw new Error(`digestInto() expects output buffer of length at least ${min}`);
    }
}
exports.output = output;
const assert = {
    number,
    bool,
    bytes,
    hash,
    exists,
    output,
};
exports.default = assert;

},{}],3:[function(require,module,exports){
"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.SHA2 = void 0;
const _assert_js_1 = require("./_assert.js");
const utils_js_1 = require("./utils.js");
// Polyfill for Safari 14
function setBigUint64(view, byteOffset, value, isLE) {
    if (typeof view.setBigUint64 === 'function')
        return view.setBigUint64(byteOffset, value, isLE);
    const _32n = BigInt(32);
    const _u32_max = BigInt(0xffffffff);
    const wh = Number((value >> _32n) & _u32_max);
    const wl = Number(value & _u32_max);
    const h = isLE ? 4 : 0;
    const l = isLE ? 0 : 4;
    view.setUint32(byteOffset + h, wh, isLE);
    view.setUint32(byteOffset + l, wl, isLE);
}
// Base SHA2 class (RFC 6234)
class SHA2 extends utils_js_1.Hash {
    constructor(blockLen, outputLen, padOffset, isLE) {
        super();
        this.blockLen = blockLen;
        this.outputLen = outputLen;
        this.padOffset = padOffset;
        this.isLE = isLE;
        this.finished = false;
        this.length = 0;
        this.pos = 0;
        this.destroyed = false;
        this.buffer = new Uint8Array(blockLen);
        this.view = (0, utils_js_1.createView)(this.buffer);
    }
    update(data) {
        _assert_js_1.default.exists(this);
        const { view, buffer, blockLen } = this;
        data = (0, utils_js_1.toBytes)(data);
        const len = data.length;
        for (let pos = 0; pos < len;) {
            const take = Math.min(blockLen - this.pos, len - pos);
            // Fast path: we have at least one block in input, cast it to view and process
            if (take === blockLen) {
                const dataView = (0, utils_js_1.createView)(data);
                for (; blockLen <= len - pos; pos += blockLen)
                    this.process(dataView, pos);
                continue;
            }
            buffer.set(data.subarray(pos, pos + take), this.pos);
            this.pos += take;
            pos += take;
            if (this.pos === blockLen) {
                this.process(view, 0);
                this.pos = 0;
            }
        }
        this.length += data.length;
        this.roundClean();
        return this;
    }
    digestInto(out) {
        _assert_js_1.default.exists(this);
        _assert_js_1.default.output(out, this);
        this.finished = true;
        // Padding
        // We can avoid allocation of buffer for padding completely if it
        // was previously not allocated here. But it won't change performance.
        const { buffer, view, blockLen, isLE } = this;
        let { pos } = this;
        // append the bit '1' to the message
        buffer[pos++] = 0b10000000;
        this.buffer.subarray(pos).fill(0);
        // we have less than padOffset left in buffer, so we cannot put length in current block, need process it and pad again
        if (this.padOffset > blockLen - pos) {
            this.process(view, 0);
            pos = 0;
        }
        // Pad until full block byte with zeros
        for (let i = pos; i < blockLen; i++)
            buffer[i] = 0;
        // Note: sha512 requires length to be 128bit integer, but length in JS will overflow before that
        // You need to write around 2 exabytes (u64_max / 8 / (1024**6)) for this to happen.
        // So we just write lowest 64 bits of that value.
        setBigUint64(view, blockLen - 8, BigInt(this.length * 8), isLE);
        this.process(view, 0);
        const oview = (0, utils_js_1.createView)(out);
        this.get().forEach((v, i) => oview.setUint32(4 * i, v, isLE));
    }
    digest() {
        const { buffer, outputLen } = this;
        this.digestInto(buffer);
        const res = buffer.slice(0, outputLen);
        this.destroy();
        return res;
    }
    _cloneInto(to) {
        to || (to = new this.constructor());
        to.set(...this.get());
        const { blockLen, buffer, length, finished, destroyed, pos } = this;
        to.length = length;
        to.pos = pos;
        to.finished = finished;
        to.destroyed = destroyed;
        if (length % blockLen)
            to.buffer.set(buffer);
        return to;
    }
}
exports.SHA2 = SHA2;

},{"./_assert.js":2,"./utils.js":7}],4:[function(require,module,exports){
"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.add = exports.toBig = exports.split = exports.fromBig = void 0;
const U32_MASK64 = BigInt(2 ** 32 - 1);
const _32n = BigInt(32);
// We are not using BigUint64Array, because they are extremely slow as per 2022
function fromBig(n, le = false) {
    if (le)
        return { h: Number(n & U32_MASK64), l: Number((n >> _32n) & U32_MASK64) };
    return { h: Number((n >> _32n) & U32_MASK64) | 0, l: Number(n & U32_MASK64) | 0 };
}
exports.fromBig = fromBig;
function split(lst, le = false) {
    let Ah = new Uint32Array(lst.length);
    let Al = new Uint32Array(lst.length);
    for (let i = 0; i < lst.length; i++) {
        const { h, l } = fromBig(lst[i], le);
        [Ah[i], Al[i]] = [h, l];
    }
    return [Ah, Al];
}
exports.split = split;
const toBig = (h, l) => (BigInt(h >>> 0) << _32n) | BigInt(l >>> 0);
exports.toBig = toBig;
// for Shift in [0, 32)
const shrSH = (h, l, s) => h >>> s;
const shrSL = (h, l, s) => (h << (32 - s)) | (l >>> s);
// Right rotate for Shift in [1, 32)
const rotrSH = (h, l, s) => (h >>> s) | (l << (32 - s));
const rotrSL = (h, l, s) => (h << (32 - s)) | (l >>> s);
// Right rotate for Shift in (32, 64), NOTE: 32 is special case.
const rotrBH = (h, l, s) => (h << (64 - s)) | (l >>> (s - 32));
const rotrBL = (h, l, s) => (h >>> (s - 32)) | (l << (64 - s));
// Right rotate for shift===32 (just swaps l&h)
const rotr32H = (h, l) => l;
const rotr32L = (h, l) => h;
// Left rotate for Shift in [1, 32)
const rotlSH = (h, l, s) => (h << s) | (l >>> (32 - s));
const rotlSL = (h, l, s) => (l << s) | (h >>> (32 - s));
// Left rotate for Shift in (32, 64), NOTE: 32 is special case.
const rotlBH = (h, l, s) => (l << (s - 32)) | (h >>> (64 - s));
const rotlBL = (h, l, s) => (h << (s - 32)) | (l >>> (64 - s));
// JS uses 32-bit signed integers for bitwise operations which means we cannot
// simple take carry out of low bit sum by shift, we need to use division.
// Removing "export" has 5% perf penalty -_-
function add(Ah, Al, Bh, Bl) {
    const l = (Al >>> 0) + (Bl >>> 0);
    return { h: (Ah + Bh + ((l / 2 ** 32) | 0)) | 0, l: l | 0 };
}
exports.add = add;
// Addition with more than 2 elements
const add3L = (Al, Bl, Cl) => (Al >>> 0) + (Bl >>> 0) + (Cl >>> 0);
const add3H = (low, Ah, Bh, Ch) => (Ah + Bh + Ch + ((low / 2 ** 32) | 0)) | 0;
const add4L = (Al, Bl, Cl, Dl) => (Al >>> 0) + (Bl >>> 0) + (Cl >>> 0) + (Dl >>> 0);
const add4H = (low, Ah, Bh, Ch, Dh) => (Ah + Bh + Ch + Dh + ((low / 2 ** 32) | 0)) | 0;
const add5L = (Al, Bl, Cl, Dl, El) => (Al >>> 0) + (Bl >>> 0) + (Cl >>> 0) + (Dl >>> 0) + (El >>> 0);
const add5H = (low, Ah, Bh, Ch, Dh, Eh) => (Ah + Bh + Ch + Dh + Eh + ((low / 2 ** 32) | 0)) | 0;
// prettier-ignore
const u64 = {
    fromBig, split, toBig: exports.toBig,
    shrSH, shrSL,
    rotrSH, rotrSL, rotrBH, rotrBL,
    rotr32H, rotr32L,
    rotlSH, rotlSL, rotlBH, rotlBL,
    add, add3L, add3H, add4L, add4H, add5H, add5L,
};
exports.default = u64;

},{}],5:[function(require,module,exports){
"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.crypto = {
    node: undefined,
    web: typeof self === 'object' && 'crypto' in self ? self.crypto : undefined,
};

},{}],6:[function(require,module,exports){
"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.sha384 = exports.sha512_256 = exports.sha512 = exports.SHA512 = void 0;
const _sha2_js_1 = require("./_sha2.js");
const _u64_js_1 = require("./_u64.js");
const utils_js_1 = require("./utils.js");
// Round contants (first 32 bits of the fractional parts of the cube roots of the first 80 primes 2..409):
// prettier-ignore
const [SHA512_Kh, SHA512_Kl] = _u64_js_1.default.split([
    '0x428a2f98d728ae22', '0x7137449123ef65cd', '0xb5c0fbcfec4d3b2f', '0xe9b5dba58189dbbc',
    '0x3956c25bf348b538', '0x59f111f1b605d019', '0x923f82a4af194f9b', '0xab1c5ed5da6d8118',
    '0xd807aa98a3030242', '0x12835b0145706fbe', '0x243185be4ee4b28c', '0x550c7dc3d5ffb4e2',
    '0x72be5d74f27b896f', '0x80deb1fe3b1696b1', '0x9bdc06a725c71235', '0xc19bf174cf692694',
    '0xe49b69c19ef14ad2', '0xefbe4786384f25e3', '0x0fc19dc68b8cd5b5', '0x240ca1cc77ac9c65',
    '0x2de92c6f592b0275', '0x4a7484aa6ea6e483', '0x5cb0a9dcbd41fbd4', '0x76f988da831153b5',
    '0x983e5152ee66dfab', '0xa831c66d2db43210', '0xb00327c898fb213f', '0xbf597fc7beef0ee4',
    '0xc6e00bf33da88fc2', '0xd5a79147930aa725', '0x06ca6351e003826f', '0x142929670a0e6e70',
    '0x27b70a8546d22ffc', '0x2e1b21385c26c926', '0x4d2c6dfc5ac42aed', '0x53380d139d95b3df',
    '0x650a73548baf63de', '0x766a0abb3c77b2a8', '0x81c2c92e47edaee6', '0x92722c851482353b',
    '0xa2bfe8a14cf10364', '0xa81a664bbc423001', '0xc24b8b70d0f89791', '0xc76c51a30654be30',
    '0xd192e819d6ef5218', '0xd69906245565a910', '0xf40e35855771202a', '0x106aa07032bbd1b8',
    '0x19a4c116b8d2d0c8', '0x1e376c085141ab53', '0x2748774cdf8eeb99', '0x34b0bcb5e19b48a8',
    '0x391c0cb3c5c95a63', '0x4ed8aa4ae3418acb', '0x5b9cca4f7763e373', '0x682e6ff3d6b2b8a3',
    '0x748f82ee5defb2fc', '0x78a5636f43172f60', '0x84c87814a1f0ab72', '0x8cc702081a6439ec',
    '0x90befffa23631e28', '0xa4506cebde82bde9', '0xbef9a3f7b2c67915', '0xc67178f2e372532b',
    '0xca273eceea26619c', '0xd186b8c721c0c207', '0xeada7dd6cde0eb1e', '0xf57d4f7fee6ed178',
    '0x06f067aa72176fba', '0x0a637dc5a2c898a6', '0x113f9804bef90dae', '0x1b710b35131c471b',
    '0x28db77f523047d84', '0x32caab7b40c72493', '0x3c9ebe0a15c9bebc', '0x431d67c49c100d4c',
    '0x4cc5d4becb3e42b6', '0x597f299cfc657e2a', '0x5fcb6fab3ad6faec', '0x6c44198c4a475817'
].map(n => BigInt(n)));
// Temporary buffer, not used to store anything between runs
const SHA512_W_H = new Uint32Array(80);
const SHA512_W_L = new Uint32Array(80);
class SHA512 extends _sha2_js_1.SHA2 {
    constructor() {
        super(128, 64, 16, false);
        // We cannot use array here since array allows indexing by variable which means optimizer/compiler cannot use registers.
        // Also looks cleaner and easier to verify with spec.
        // Initial state (first 32 bits of the fractional parts of the square roots of the first 8 primes 2..19):
        // h -- high 32 bits, l -- low 32 bits
        this.Ah = 0x6a09e667 | 0;
        this.Al = 0xf3bcc908 | 0;
        this.Bh = 0xbb67ae85 | 0;
        this.Bl = 0x84caa73b | 0;
        this.Ch = 0x3c6ef372 | 0;
        this.Cl = 0xfe94f82b | 0;
        this.Dh = 0xa54ff53a | 0;
        this.Dl = 0x5f1d36f1 | 0;
        this.Eh = 0x510e527f | 0;
        this.El = 0xade682d1 | 0;
        this.Fh = 0x9b05688c | 0;
        this.Fl = 0x2b3e6c1f | 0;
        this.Gh = 0x1f83d9ab | 0;
        this.Gl = 0xfb41bd6b | 0;
        this.Hh = 0x5be0cd19 | 0;
        this.Hl = 0x137e2179 | 0;
    }
    // prettier-ignore
    get() {
        const { Ah, Al, Bh, Bl, Ch, Cl, Dh, Dl, Eh, El, Fh, Fl, Gh, Gl, Hh, Hl } = this;
        return [Ah, Al, Bh, Bl, Ch, Cl, Dh, Dl, Eh, El, Fh, Fl, Gh, Gl, Hh, Hl];
    }
    // prettier-ignore
    set(Ah, Al, Bh, Bl, Ch, Cl, Dh, Dl, Eh, El, Fh, Fl, Gh, Gl, Hh, Hl) {
        this.Ah = Ah | 0;
        this.Al = Al | 0;
        this.Bh = Bh | 0;
        this.Bl = Bl | 0;
        this.Ch = Ch | 0;
        this.Cl = Cl | 0;
        this.Dh = Dh | 0;
        this.Dl = Dl | 0;
        this.Eh = Eh | 0;
        this.El = El | 0;
        this.Fh = Fh | 0;
        this.Fl = Fl | 0;
        this.Gh = Gh | 0;
        this.Gl = Gl | 0;
        this.Hh = Hh | 0;
        this.Hl = Hl | 0;
    }
    process(view, offset) {
        // Extend the first 16 words into the remaining 64 words w[16..79] of the message schedule array
        for (let i = 0; i < 16; i++, offset += 4) {
            SHA512_W_H[i] = view.getUint32(offset);
            SHA512_W_L[i] = view.getUint32((offset += 4));
        }
        for (let i = 16; i < 80; i++) {
            // s0 := (w[i-15] rightrotate 1) xor (w[i-15] rightrotate 8) xor (w[i-15] rightshift 7)
            const W15h = SHA512_W_H[i - 15] | 0;
            const W15l = SHA512_W_L[i - 15] | 0;
            const s0h = _u64_js_1.default.rotrSH(W15h, W15l, 1) ^ _u64_js_1.default.rotrSH(W15h, W15l, 8) ^ _u64_js_1.default.shrSH(W15h, W15l, 7);
            const s0l = _u64_js_1.default.rotrSL(W15h, W15l, 1) ^ _u64_js_1.default.rotrSL(W15h, W15l, 8) ^ _u64_js_1.default.shrSL(W15h, W15l, 7);
            // s1 := (w[i-2] rightrotate 19) xor (w[i-2] rightrotate 61) xor (w[i-2] rightshift 6)
            const W2h = SHA512_W_H[i - 2] | 0;
            const W2l = SHA512_W_L[i - 2] | 0;
            const s1h = _u64_js_1.default.rotrSH(W2h, W2l, 19) ^ _u64_js_1.default.rotrBH(W2h, W2l, 61) ^ _u64_js_1.default.shrSH(W2h, W2l, 6);
            const s1l = _u64_js_1.default.rotrSL(W2h, W2l, 19) ^ _u64_js_1.default.rotrBL(W2h, W2l, 61) ^ _u64_js_1.default.shrSL(W2h, W2l, 6);
            // SHA256_W[i] = s0 + s1 + SHA256_W[i - 7] + SHA256_W[i - 16];
            const SUMl = _u64_js_1.default.add4L(s0l, s1l, SHA512_W_L[i - 7], SHA512_W_L[i - 16]);
            const SUMh = _u64_js_1.default.add4H(SUMl, s0h, s1h, SHA512_W_H[i - 7], SHA512_W_H[i - 16]);
            SHA512_W_H[i] = SUMh | 0;
            SHA512_W_L[i] = SUMl | 0;
        }
        let { Ah, Al, Bh, Bl, Ch, Cl, Dh, Dl, Eh, El, Fh, Fl, Gh, Gl, Hh, Hl } = this;
        // Compression function main loop, 80 rounds
        for (let i = 0; i < 80; i++) {
            // S1 := (e rightrotate 14) xor (e rightrotate 18) xor (e rightrotate 41)
            const sigma1h = _u64_js_1.default.rotrSH(Eh, El, 14) ^ _u64_js_1.default.rotrSH(Eh, El, 18) ^ _u64_js_1.default.rotrBH(Eh, El, 41);
            const sigma1l = _u64_js_1.default.rotrSL(Eh, El, 14) ^ _u64_js_1.default.rotrSL(Eh, El, 18) ^ _u64_js_1.default.rotrBL(Eh, El, 41);
            //const T1 = (H + sigma1 + Chi(E, F, G) + SHA256_K[i] + SHA256_W[i]) | 0;
            const CHIh = (Eh & Fh) ^ (~Eh & Gh);
            const CHIl = (El & Fl) ^ (~El & Gl);
            // T1 = H + sigma1 + Chi(E, F, G) + SHA512_K[i] + SHA512_W[i]
            // prettier-ignore
            const T1ll = _u64_js_1.default.add5L(Hl, sigma1l, CHIl, SHA512_Kl[i], SHA512_W_L[i]);
            const T1h = _u64_js_1.default.add5H(T1ll, Hh, sigma1h, CHIh, SHA512_Kh[i], SHA512_W_H[i]);
            const T1l = T1ll | 0;
            // S0 := (a rightrotate 28) xor (a rightrotate 34) xor (a rightrotate 39)
            const sigma0h = _u64_js_1.default.rotrSH(Ah, Al, 28) ^ _u64_js_1.default.rotrBH(Ah, Al, 34) ^ _u64_js_1.default.rotrBH(Ah, Al, 39);
            const sigma0l = _u64_js_1.default.rotrSL(Ah, Al, 28) ^ _u64_js_1.default.rotrBL(Ah, Al, 34) ^ _u64_js_1.default.rotrBL(Ah, Al, 39);
            const MAJh = (Ah & Bh) ^ (Ah & Ch) ^ (Bh & Ch);
            const MAJl = (Al & Bl) ^ (Al & Cl) ^ (Bl & Cl);
            Hh = Gh | 0;
            Hl = Gl | 0;
            Gh = Fh | 0;
            Gl = Fl | 0;
            Fh = Eh | 0;
            Fl = El | 0;
            ({ h: Eh, l: El } = _u64_js_1.default.add(Dh | 0, Dl | 0, T1h | 0, T1l | 0));
            Dh = Ch | 0;
            Dl = Cl | 0;
            Ch = Bh | 0;
            Cl = Bl | 0;
            Bh = Ah | 0;
            Bl = Al | 0;
            const All = _u64_js_1.default.add3L(T1l, sigma0l, MAJl);
            Ah = _u64_js_1.default.add3H(All, T1h, sigma0h, MAJh);
            Al = All | 0;
        }
        // Add the compressed chunk to the current hash value
        ({ h: Ah, l: Al } = _u64_js_1.default.add(this.Ah | 0, this.Al | 0, Ah | 0, Al | 0));
        ({ h: Bh, l: Bl } = _u64_js_1.default.add(this.Bh | 0, this.Bl | 0, Bh | 0, Bl | 0));
        ({ h: Ch, l: Cl } = _u64_js_1.default.add(this.Ch | 0, this.Cl | 0, Ch | 0, Cl | 0));
        ({ h: Dh, l: Dl } = _u64_js_1.default.add(this.Dh | 0, this.Dl | 0, Dh | 0, Dl | 0));
        ({ h: Eh, l: El } = _u64_js_1.default.add(this.Eh | 0, this.El | 0, Eh | 0, El | 0));
        ({ h: Fh, l: Fl } = _u64_js_1.default.add(this.Fh | 0, this.Fl | 0, Fh | 0, Fl | 0));
        ({ h: Gh, l: Gl } = _u64_js_1.default.add(this.Gh | 0, this.Gl | 0, Gh | 0, Gl | 0));
        ({ h: Hh, l: Hl } = _u64_js_1.default.add(this.Hh | 0, this.Hl | 0, Hh | 0, Hl | 0));
        this.set(Ah, Al, Bh, Bl, Ch, Cl, Dh, Dl, Eh, El, Fh, Fl, Gh, Gl, Hh, Hl);
    }
    roundClean() {
        SHA512_W_H.fill(0);
        SHA512_W_L.fill(0);
    }
    destroy() {
        this.buffer.fill(0);
        this.set(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}
exports.SHA512 = SHA512;
class SHA512_256 extends SHA512 {
    constructor() {
        super();
        // h -- high 32 bits, l -- low 32 bits
        this.Ah = 0x22312194 | 0;
        this.Al = 0xfc2bf72c | 0;
        this.Bh = 0x9f555fa3 | 0;
        this.Bl = 0xc84c64c2 | 0;
        this.Ch = 0x2393b86b | 0;
        this.Cl = 0x6f53b151 | 0;
        this.Dh = 0x96387719 | 0;
        this.Dl = 0x5940eabd | 0;
        this.Eh = 0x96283ee2 | 0;
        this.El = 0xa88effe3 | 0;
        this.Fh = 0xbe5e1e25 | 0;
        this.Fl = 0x53863992 | 0;
        this.Gh = 0x2b0199fc | 0;
        this.Gl = 0x2c85b8aa | 0;
        this.Hh = 0x0eb72ddc | 0;
        this.Hl = 0x81c52ca2 | 0;
        this.outputLen = 32;
    }
}
class SHA384 extends SHA512 {
    constructor() {
        super();
        // h -- high 32 bits, l -- low 32 bits
        this.Ah = 0xcbbb9d5d | 0;
        this.Al = 0xc1059ed8 | 0;
        this.Bh = 0x629a292a | 0;
        this.Bl = 0x367cd507 | 0;
        this.Ch = 0x9159015a | 0;
        this.Cl = 0x3070dd17 | 0;
        this.Dh = 0x152fecd8 | 0;
        this.Dl = 0xf70e5939 | 0;
        this.Eh = 0x67332667 | 0;
        this.El = 0xffc00b31 | 0;
        this.Fh = 0x8eb44a87 | 0;
        this.Fl = 0x68581511 | 0;
        this.Gh = 0xdb0c2e0d | 0;
        this.Gl = 0x64f98fa7 | 0;
        this.Hh = 0x47b5481d | 0;
        this.Hl = 0xbefa4fa4 | 0;
        this.outputLen = 48;
    }
}
exports.sha512 = (0, utils_js_1.wrapConstructor)(() => new SHA512());
exports.sha512_256 = (0, utils_js_1.wrapConstructor)(() => new SHA512_256());
exports.sha384 = (0, utils_js_1.wrapConstructor)(() => new SHA384());

},{"./_sha2.js":3,"./_u64.js":4,"./utils.js":7}],7:[function(require,module,exports){
"use strict";
/*! noble-hashes - MIT License (c) 2022 Paul Miller (paulmillr.com) */
Object.defineProperty(exports, "__esModule", { value: true });
exports.randomBytes = exports.wrapConstructorWithOpts = exports.wrapConstructor = exports.checkOpts = exports.Hash = exports.concatBytes = exports.toBytes = exports.utf8ToBytes = exports.asyncLoop = exports.nextTick = exports.hexToBytes = exports.bytesToHex = exports.isLE = exports.rotr = exports.createView = exports.u32 = exports.u8 = void 0;
// The import here is via the package name. This is to ensure
// that exports mapping/resolution does fall into place.
const crypto_1 = require("@noble/hashes/crypto");
// Cast array to different type
const u8 = (arr) => new Uint8Array(arr.buffer, arr.byteOffset, arr.byteLength);
exports.u8 = u8;
const u32 = (arr) => new Uint32Array(arr.buffer, arr.byteOffset, Math.floor(arr.byteLength / 4));
exports.u32 = u32;
// Cast array to view
const createView = (arr) => new DataView(arr.buffer, arr.byteOffset, arr.byteLength);
exports.createView = createView;
// The rotate right (circular right shift) operation for uint32
const rotr = (word, shift) => (word << (32 - shift)) | (word >>> shift);
exports.rotr = rotr;
exports.isLE = new Uint8Array(new Uint32Array([0x11223344]).buffer)[0] === 0x44;
// There is almost no big endian hardware, but js typed arrays uses platform specific endianness.
// So, just to be sure not to corrupt anything.
if (!exports.isLE)
    throw new Error('Non little-endian hardware is not supported');
const hexes = Array.from({ length: 256 }, (v, i) => i.toString(16).padStart(2, '0'));
/**
 * @example bytesToHex(Uint8Array.from([0xde, 0xad, 0xbe, 0xef]))
 */
function bytesToHex(uint8a) {
    // pre-caching improves the speed 6x
    if (!(uint8a instanceof Uint8Array))
        throw new Error('Uint8Array expected');
    let hex = '';
    for (let i = 0; i < uint8a.length; i++) {
        hex += hexes[uint8a[i]];
    }
    return hex;
}
exports.bytesToHex = bytesToHex;
/**
 * @example hexToBytes('deadbeef')
 */
function hexToBytes(hex) {
    if (typeof hex !== 'string') {
        throw new TypeError('hexToBytes: expected string, got ' + typeof hex);
    }
    if (hex.length % 2)
        throw new Error('hexToBytes: received invalid unpadded hex');
    const array = new Uint8Array(hex.length / 2);
    for (let i = 0; i < array.length; i++) {
        const j = i * 2;
        const hexByte = hex.slice(j, j + 2);
        const byte = Number.parseInt(hexByte, 16);
        if (Number.isNaN(byte) || byte < 0)
            throw new Error('Invalid byte sequence');
        array[i] = byte;
    }
    return array;
}
exports.hexToBytes = hexToBytes;
// There is no setImmediate in browser and setTimeout is slow. However, call to async function will return Promise
// which will be fullfiled only on next scheduler queue processing step and this is exactly what we need.
const nextTick = async () => { };
exports.nextTick = nextTick;
// Returns control to thread each 'tick' ms to avoid blocking
async function asyncLoop(iters, tick, cb) {
    let ts = Date.now();
    for (let i = 0; i < iters; i++) {
        cb(i);
        // Date.now() is not monotonic, so in case if clock goes backwards we return return control too
        const diff = Date.now() - ts;
        if (diff >= 0 && diff < tick)
            continue;
        await (0, exports.nextTick)();
        ts += diff;
    }
}
exports.asyncLoop = asyncLoop;
function utf8ToBytes(str) {
    if (typeof str !== 'string') {
        throw new TypeError(`utf8ToBytes expected string, got ${typeof str}`);
    }
    return new TextEncoder().encode(str);
}
exports.utf8ToBytes = utf8ToBytes;
function toBytes(data) {
    if (typeof data === 'string')
        data = utf8ToBytes(data);
    if (!(data instanceof Uint8Array))
        throw new TypeError(`Expected input type is Uint8Array (got ${typeof data})`);
    return data;
}
exports.toBytes = toBytes;
/**
 * Concats Uint8Array-s into one; like `Buffer.concat([buf1, buf2])`
 * @example concatBytes(buf1, buf2)
 */
function concatBytes(...arrays) {
    if (!arrays.every((a) => a instanceof Uint8Array))
        throw new Error('Uint8Array list expected');
    if (arrays.length === 1)
        return arrays[0];
    const length = arrays.reduce((a, arr) => a + arr.length, 0);
    const result = new Uint8Array(length);
    for (let i = 0, pad = 0; i < arrays.length; i++) {
        const arr = arrays[i];
        result.set(arr, pad);
        pad += arr.length;
    }
    return result;
}
exports.concatBytes = concatBytes;
// For runtime check if class implements interface
class Hash {
    // Safe version that clones internal state
    clone() {
        return this._cloneInto();
    }
}
exports.Hash = Hash;
// Check if object doens't have custom constructor (like Uint8Array/Array)
const isPlainObject = (obj) => Object.prototype.toString.call(obj) === '[object Object]' && obj.constructor === Object;
function checkOpts(defaults, opts) {
    if (opts !== undefined && (typeof opts !== 'object' || !isPlainObject(opts)))
        throw new TypeError('Options should be object or undefined');
    const merged = Object.assign(defaults, opts);
    return merged;
}
exports.checkOpts = checkOpts;
function wrapConstructor(hashConstructor) {
    const hashC = (message) => hashConstructor().update(toBytes(message)).digest();
    const tmp = hashConstructor();
    hashC.outputLen = tmp.outputLen;
    hashC.blockLen = tmp.blockLen;
    hashC.create = () => hashConstructor();
    return hashC;
}
exports.wrapConstructor = wrapConstructor;
function wrapConstructorWithOpts(hashCons) {
    const hashC = (msg, opts) => hashCons(opts).update(toBytes(msg)).digest();
    const tmp = hashCons({});
    hashC.outputLen = tmp.outputLen;
    hashC.blockLen = tmp.blockLen;
    hashC.create = (opts) => hashCons(opts);
    return hashC;
}
exports.wrapConstructorWithOpts = wrapConstructorWithOpts;
/**
 * Secure PRNG
 */
function randomBytes(bytesLength = 32) {
    if (crypto_1.crypto.web) {
        return crypto_1.crypto.web.getRandomValues(new Uint8Array(bytesLength));
    }
    else if (crypto_1.crypto.node) {
        return new Uint8Array(crypto_1.crypto.node.randomBytes(bytesLength).buffer);
    }
    else {
        throw new Error("The environment doesn't have randomBytes function");
    }
}
exports.randomBytes = randomBytes;

},{"@noble/hashes/crypto":5}],8:[function(require,module,exports){
const { sha512 } = require('@noble/hashes/sha512');
let ed = require('../../lib');

globalThis.runNobleBenchmark = (async () => {
  const x = 1;

  ed.utils.precompute();

  function to64Bytes(numOrStr) {
    let hex = typeof numOrStr === 'string' ? numOrStr : numOrStr.toString(16);
    return ed.utils.hexToBytes(hex.padStart(64, '0'));
  }

  const priv1bit = to64Bytes(2n);
  const priv = to64Bytes(0x9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60n);
  const msg = to64Bytes('deadbeefdeadbeefdeadbeefdeadbeefdeadbeef');

  let pubHex, sigHex;
  for (var i = 0; i < 30 * x; i++) {
    pubHex = await ed.getPublicKey(priv1bit);
  }

  for (var i = 0; i < 30 * x; i++) {
    pubHex = await ed.getPublicKey(ed.utils.randomPrivateKey());
  }

  for (var i = 0; i < 20 * x; i++) {
    sigHex = await ed.sign(msg, priv);
  }

  for (var i = 0; i < 4 * x; i++) {
    await ed.verify(sigHex, msg, pubHex);
  }

  const sigInst = ed.Signature.fromHex(sigHex);
  const pubInst = ed.Point.fromHex(pubHex);

  for (var i = 0; i < 5 * x; i++) {
    await ed.verify(sigInst, msg, pubInst);
  }

  for (var i = 0; i < 65 * x; i++) {
    ed.Point.fromHex(pubHex);
  }

  const encodingsOfSmallMultiples = [
    // This is the identity point
    '0000000000000000000000000000000000000000000000000000000000000000',
    // This is the basepoint
    'e2f2ae0a6abc4e71a884a961c500515f58e30b6aa582dd8db6a65945e08d2d76',
    // These are small multiples of the basepoint
    '6a493210f7499cd17fecb510ae0cea23a110e8d5b901f8acadd3095c73a3b919',
    '94741f5d5d52755ece4f23f044ee27d5d1ea1e2bd196b462166b16152a9d0259',
    'da80862773358b466ffadfe0b3293ab3d9fd53c5ea6c955358f568322daf6a57',
    'e882b131016b52c1d3337080187cf768423efccbb517bb495ab812c4160ff44e',
    'f64746d3c92b13050ed8d80236a7f0007c3b3f962f5ba793d19a601ebb1df403',
    '44f53520926ec81fbd5a387845beb7df85a96a24ece18738bdcfa6a7822a176d',
    '903293d8f2287ebe10e2374dc1a53e0bc887e592699f02d077d5263cdd55601c',
    '02622ace8f7303a31cafc63f8fc48fdc16e1c8c8d234b2f0d6685282a9076031',
    '20706fd788b2720a1ed2a5dad4952b01f413bcf0e7564de8cdc816689e2db95f',
    'bce83f8ba5dd2fa572864c24ba1810f9522bc6004afe95877ac73241cafdab42',
    'e4549ee16b9aa03099ca208c67adafcafa4c3f3e4e5303de6026e3ca8ff84460',
    'aa52e000df2e16f55fb1032fc33bc42742dad6bd5a8fc0be0167436c5948501f',
    '46376b80f409b29dc2b5f6f0c52591990896e5716f41477cd30085ab7f10301e',
    'e0c418f7c8d9c4cdd7395b93ea124f3ad99021bb681dfc3302a9d99a2e53e64e',
  ].map((n) => ed.utils.hexToBytes(n));
  const hash = new Uint8Array([
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
  ]);
  for (var i = 0; i < 30 * x; i++) {
    ed.RistrettoPoint.hashToCurve(hash);
  }
  for (var i = 0; i < 30 * x; i++) {
    ed.RistrettoPoint.fromHex(encodingsOfSmallMultiples[2]).toHex();
  }
  for (var i = 0; i < 6 * x; i++) {
    ed.curve25519.scalarMultBase('aeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef');
  }
  for (var i = 0; i < 5 * x; i++) {
    await ed.getSharedSecret(0x12345, 'aeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef');
  }

  ed.utils.sha512Sync = (...m) => sha512(ed.utils.concatBytes(...m));
  const { sync } = ed;

  for (var i = 0; i < 30 * x; i++) {
    sync.getPublicKey(ed.utils.randomPrivateKey());
  }

  for (var i = 0; i < 20 * x; i++) {
    sync.sign(msg, priv);
  }

  for (var i = 0; i < 4 * x; i++) {
    sync.verify(sigHex, msg, pubHex);
  }
});

},{"../../lib":1,"@noble/hashes/sha512":6}],9:[function(require,module,exports){

},{}]},{},[8]);
