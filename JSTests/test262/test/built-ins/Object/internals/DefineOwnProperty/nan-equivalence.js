// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-ordinary-object-internal-methods-and-internal-slots-defineownproperty-p-desc
es6id: 9.1.6
description: >
  Replaces value field even if they pass in the SameValue algorithm, including
  distinct NaN values
info: |
  Previously, this method compared the "value" field using the SameValue
  algorithm (thereby ignoring distinct NaN values)

  ---

  [[DefineOwnProperty]] (P, Desc)

  1. Return ? OrdinaryDefineOwnProperty(O, P, Desc).

  9.1.6.1 OrdinaryDefineOwnProperty

  1. Let current be ? O.[[GetOwnProperty]](P).
  2. Let extensible be O.[[Extensible]].
  3. Return ValidateAndApplyPropertyDescriptor(O, P, extensible, Desc,
     current).

  9.1.6.3 ValidateAndApplyPropertyDescriptor

  [...]
  7. Else if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both true,
     then
    a. If current.[[Configurable]] is false and current.[[Writable]] is false,
       then
      [...]
  [...]
  9. If O is not undefined, then
    a. For each field of Desc that is present, set the corresponding attribute
       of the property named P of object O to the value of the field.
  10. Return true.
features: [Float64Array, Uint8Array, Uint16Array]
includes: [nans.js]
---*/

var isLittleEndian = new Uint8Array(new Uint16Array([1]).buffer)[0] !== 0;

var float = new Float64Array(1);
var ints = new Uint8Array(float.buffer);
var len = distinctNaNs.length;

function byteValue(value) {
  float[0] = value;

  var hex = "0123456789ABCDEF";
  var s = "";
  for (var i = 0; i < 8; ++i) {
    var v = ints[isLittleEndian ? 7 - i : i];
    s += hex[(v >> 4) & 0xf] + hex[v & 0xf];
  }
  return s;
}

/**
 * Iterate over each pair of distinct NaN values (with replacement). If two or
 * more suitable NaN values cannot be identified, the semantics under test
 * cannot be verified and this test is expected to pass without evaluating any
 * assertions.
 */
for (var idx = 0; idx < len; ++idx) {
  for (var jdx = 0; jdx < len; ++jdx) {
    // NB: Don't store the distinct NaN values as global variables, because
    // global variables are properties of the global object. And in this test
    // we want to ensure NaN-valued properties in objects are properly handled,
    // so storing NaN values in the (global) object defeats the purpose.
    if (byteValue(distinctNaNs[idx]) === byteValue(distinctNaNs[jdx])) {
      continue;
    }

    var subject = {};
    subject.prop = distinctNaNs[idx];
    subject.prop = distinctNaNs[jdx];

    assert.sameValue(
      byteValue(subject.prop),
      byteValue(distinctNaNs[jdx]),
      'Property value was re-set'
    );
  }
}
