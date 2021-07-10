// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-sharedarraybuffer-constructor
description: Invoked with a non-object value for options
info: |
  SharedArrayBuffer( length [ , options ] )

  1. If NewTarget is undefined, throw a TypeError exception.
  2. Let byteLength be ? ToIndex(length).
  3. Let requestedMaxByteLength be ? GetArrayBufferMaxByteLengthOption(options).
  4. If requestedMaxByteLength is empty, then
     a. Return ? AllocateArrayBuffer(NewTarget, byteLength).

  1.1.5 GetArrayBufferMaxByteLengthOption ( options )

  1. If Type(options) is not Object, return empty.
features: [resizable-arraybuffer]
---*/

assert.sameValue(new SharedArrayBuffer(0, null).resizable, false, 'null');
assert.sameValue(new SharedArrayBuffer(0, true).resizable, false, 'boolean');
assert.sameValue(new SharedArrayBuffer(0, Symbol(3)).resizable, false, 'symbol');
assert.sameValue(new SharedArrayBuffer(0, 1n).resizable, false, 'bigint');
assert.sameValue(new SharedArrayBuffer(0, 'string').resizable, false, 'string');
assert.sameValue(new SharedArrayBuffer(0, 9).resizable, false, 'number');
assert.sameValue(new SharedArrayBuffer(0, undefined).resizable, false, 'undefined');
