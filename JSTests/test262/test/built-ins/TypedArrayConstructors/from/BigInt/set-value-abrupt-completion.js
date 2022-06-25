// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.from
description: >
  Return abrupt from setting a value on the new typedarray
info: |
  22.2.2.1 %TypedArray%.from ( source [ , mapfn [ , thisArg ] ] )

  ...
  10. Repeat, while k < len
    ...
    c. If mapping is true, then
      i. Let mappedValue be ? Call(mapfn, T, « kValue, k »).
    d. Else, let mappedValue be kValue.
    e. Perform ? Set(targetObj, Pk, mappedValue, true).
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

var obj = {
  valueOf() {
    throw new Test262Error();
  }
};

testWithBigIntTypedArrayConstructors(function(TA) {
  var source = [42n, obj, 1n];
  var lastValue;
  var mapfn = function(kValue) {
    lastValue = kValue;
    return kValue;
  };

  assert.throws(Test262Error, function() {
    TA.from(source, mapfn);
  });

  assert.sameValue(lastValue, obj, "interrupted source iteration");

  assert.throws(Test262Error, function() {
    TA.from(source);
  });
});
