// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-get-sharedarraybuffer.prototype.growable
description: >
  "growable" property of SharedArrayBuffer.prototype
info: |
  SharedArrayBuffer.prototype.growable is an accessor property whose set accessor
  function is undefined.

  Section 17: Every accessor property described in clauses 18 through 26 and in
  Annex B.2 has the attributes {[[Enumerable]]: false, [[Configurable]]: true }
includes: [propertyHelper.js]
features: [resizable-arraybuffer]
---*/

var desc = Object.getOwnPropertyDescriptor(SharedArrayBuffer.prototype, 'growable');

assert.sameValue(desc.set, undefined);
assert.sameValue(typeof desc.get, 'function');

verifyProperty(SharedArrayBuffer.prototype, 'growable', {
  enumerable: false,
  configurable: true
});
