// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.timezone.prototype.id
description: TypeError thrown when toString property not present
includes: [compareArray.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "get timeZone.toString",
];

const timeZone = new Temporal.TimeZone("UTC");
Object.defineProperty(timeZone, "toString", {
  get() {
    actual.push("get timeZone.toString");
    return undefined;
  },
});

const descriptor = Object.getOwnPropertyDescriptor(Temporal.TimeZone.prototype, "id");
assert.throws(TypeError, () => descriptor.get.call(timeZone));
assert.compareArray(actual, expected);
