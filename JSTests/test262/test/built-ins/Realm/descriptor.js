// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm-constructor
description: >
  The Realm constructor is the initial value of the "Realm" property of the global object.
includes: [propertyHelper.js]
features: [callable-boundary-realms]
---*/

verifyProperty(this, "Realm", {
  enumerable: false,
  writable: true,
  configurable: true,
});
