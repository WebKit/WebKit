// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: pending
description: BigInt.asIntN type coercion for bits parameter
info: >
  BigInt.asIntN ( bits, bigint )

  1. Let bits be ? ToIndex(bits).

features: [BigInt, Symbol, Symbol.toPrimitive, arrow-function]
---*/

function MyError() {}

assert.sameValue(BigInt.asIntN(undefined, 1n), 0n);
assert.sameValue(BigInt.asIntN(null, 1n), 0n);
assert.sameValue(BigInt.asIntN(true, 1n), -1n);
assert.sameValue(BigInt.asIntN(false, 1n), 0n);
assert.sameValue(BigInt.asIntN("foo", 1n), 0n);
assert.sameValue(BigInt.asIntN("3", 10n), 2n);
assert.throws(TypeError, () => BigInt.asIntN(Symbol(0), 0n));
assert.throws(TypeError, () => BigInt.asIntN(1n, 0n));
assert.sameValue(BigInt.asIntN(Object(3), 10n), 2n);
assert.sameValue(BigInt.asIntN({[Symbol.toPrimitive]:()=>3, valueOf(){throw new MyError();}, toString(){throw new MyError();}}, 10n), 2n);
assert.sameValue(BigInt.asIntN({valueOf:()=>3, toString(){throw new MyError();}}, 10n), 2n);
assert.sameValue(BigInt.asIntN({toString:()=>"3"}, 10n), 2n);
assert.throws(MyError, () => BigInt.asIntN({[Symbol.toPrimitive](){throw new MyError();}, valueOf:()=>3, toString:()=>"3"}, 0n));
assert.throws(MyError, () => BigInt.asIntN({valueOf(){throw new MyError();}, toString:()=>"3"}, 0n));
assert.throws(MyError, () => BigInt.asIntN({toString(){throw new MyError();}}, 0n));
assert.sameValue(BigInt.asIntN(NaN, 1n), 0n);
assert.sameValue(BigInt.asIntN(3.9, 10n), 2n);
assert.sameValue(BigInt.asIntN(-0.9, 1n), 0n);
assert.throws(RangeError, () => BigInt.asIntN(-1, 0n));
assert.throws(RangeError, () => BigInt.asIntN("-2.5", 0n));
assert.throws(RangeError, () => BigInt.asIntN(2 ** 53, 0n));
assert.throws(RangeError, () => BigInt.asIntN(Infinity, 0n));
