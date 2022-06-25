// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
esid: sec-atomics.compareexchange
description: Testing descriptor property of Atomics.compareExchange
includes: [propertyHelper.js]
features: [Atomics]
---*/

verifyProperty(Atomics, 'compareExchange', {
  enumerable: false,
  writable: true,
  configurable: true,
});
