// Copyright (C) 2021 Nikhil Singhal. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DurationFormat.prototype-@@tostringtag
description: Checks prototype's toStringTag value
info: |
  The initial value of the @@toStringTag property is the string value "Intl.DurationFormat".
  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
features: [Intl.DurationFormat, Symbol.toStringTag]
includes: [propertyHelper.js]
---*/

verifyProperty(Intl.DurationFormat.prototype, Symbol.toStringTag, {
  value: "Intl.DurationFormat",
  writable: false,
  enumerable: false,
  configurable: true
});
