// Copyright 2018 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-segment-iterator-prototype
description: Verifies the behavior for the iterators.
includes: [propertyHelper.js]
features: [Intl.Segmenter]
---*/

const prototype = Object.getPrototypeOf((new Intl.Segmenter()).segment('text'));
for (const func of ["next", "following", "preceding"]) {
  verifyProperty(prototype, func, {
    writable: true,
    enumerable: false,
    configurable: true,
  });
}

for (const property of ["index", "breakType"]) {
  let desc = Object.getOwnPropertyDescriptor(prototype, property);
  assert.sameValue(desc.get.name, `get ${property}`);
  assert.sameValue(typeof desc.get, "function")
  assert.sameValue(desc.set, undefined);
  assert.sameValue(desc.enumerable, false);
  assert.sameValue(desc.configurable, true);
}
