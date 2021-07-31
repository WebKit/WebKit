// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.evaluate
description: >
    Realm.prototype.evaluate is an ECMAScript Standard built-in object function.
includes: [propertyHelper.js]
features: [callable-boundary-realms]
---*/

verifyProperty(Realm.prototype, "evaluate", {
  enumerable: false,
  writable: true,
  configurable: true,
});
