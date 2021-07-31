// Copyright (C) 2021 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype-@@tostringtag
description: >
  `Symbol.toStringTag` property descriptor
info: |
  The initial value of the @@toStringTag property is the String value
  "Realm".

  This property has the attributes { [[Writable]]: false, [[Enumerable]]:
  false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [callable-boundary-realms, Symbol.toStringTag]
---*/

verifyProperty(Realm.prototype, Symbol.toStringTag, {
    value: 'Realm',
    enumerable: false,
    writable: false,
    configurable: true
});
