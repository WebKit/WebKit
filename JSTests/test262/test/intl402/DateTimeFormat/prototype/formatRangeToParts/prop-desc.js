// Copyright 2016-2019 Mozilla Corporation, Igalia S.L. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
description: Property type and descriptor.
includes: [propertyHelper.js]
features: [Intl.DateTimeFormat-formatRange]
---*/

assert.sameValue(
  typeof Intl.DateTimeFormat.prototype.formatRangeToParts,
  'function',
  '`typeof Intl.DateTimeFormat.prototype.formatRangeToParts` is `function`'
);

verifyProperty(Intl.DateTimeFormat.prototype, 'formatRangeToParts', {
  enumerable: false,
  writable: true,
  configurable: true,
});
