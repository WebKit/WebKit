// Copyright (C) 2021 Nikhil Singhal. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DurationFormat.prototype
description: Prototype attributes verification
info: |
  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
features: [Intl.DurationFormat]
includes: [propertyHelper.js]
---*/

verifyNotEnumerable(Intl.DurationFormat, "prototype");
verifyNotWritable(Intl.DurationFormat, "prototype");
verifyNotConfigurable(Intl.DurationFormat, "prototype");
