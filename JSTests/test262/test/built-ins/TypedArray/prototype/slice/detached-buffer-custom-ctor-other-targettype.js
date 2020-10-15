// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.slice
description: >
  Throws a TypeError if _O_.[[ViewedArrayBuffer]] is detached. Using other
  targetType
info: |
  22.2.3.24 %TypedArray%.prototype.slice ( start, end )

  ...
  Let A be ? TypedArraySpeciesCreate(O, « count »).
  If count > 0, then
    If IsDetachedBuffer(O.[[ViewedArrayBuffer]]) is true, throw a TypeError exception.
  ...
includes: [testTypedArray.js, detachArrayBuffer.js, propertyHelper.js]
features: [align-detached-buffer-semantics-with-web-reality, Symbol.species, TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  let counter = 0;
  var sample = new TA(1);

  verifyConfigurable(sample, "0");
  Object.defineProperty(sample, "0", {
    get() {
      // If the slice operation reaches this code,
      // then it did not throw a TypeError exception
      // as a result of the detached buffer.
      //
      // Reaching this code is the equivalent to:
      //
      // If count > 0, then
      //    If IsDetachedBuffer(O.[[ViewedArrayBuffer]]) is true, throw a TypeError exception.
      //    Let srcName be the String value of O.[[TypedArrayName]].
      //    Let srcType be the Element Type value in Table 62 for srcName.
      //    Let targetName be the String value of A.[[TypedArrayName]].
      //    Let targetType be the Element Type value in Table 62 for targetName.
      //    If srcType is different from targetType, then
      //      Let n be 0.
      //      Repeat, while k < final,
      //        Let Pk be ! ToString(k).
      //        Let kValue be ! Get(O, Pk).
      //
      throw new Test262Error();
    }
  });

  sample.constructor = {};
  sample.constructor[Symbol.species] = function(count) {
    var other = TA === Int8Array ? Int16Array : Int8Array;
    counter++;
    $DETACHBUFFER(sample.buffer);
    return new other(count);
  };

  assert.throws(TypeError, function() {
    counter++;
    sample.slice();
  }, '`sample.slice()` throws TypeError');

  assert.sameValue(counter, 2, 'The value of `counter` is 2');
});
