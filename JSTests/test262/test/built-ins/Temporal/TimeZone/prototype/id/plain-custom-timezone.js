// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.timezone.prototype.id
description: Getter calls toString() and returns its value
includes: [compareArray.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "get timeZone[@@toPrimitive]",
  "get timeZone.toString",
  "call timeZone.toString",
];

const timeZone = new Proxy({
  toString() {
    actual.push("call timeZone.toString");
    return "time zone";
  },
}, {
  has(target, property) {
    if (property === Symbol.toPrimitive) {
      actual.push('has timeZone[@@toPrimitive]');
    } else {
      actual.push(`has timeZone.${property}`);
    }
    return property in target;
  },
  get(target, property) {
    if (property === Symbol.toPrimitive) {
      actual.push('get timeZone[@@toPrimitive]');
    } else {
      actual.push(`get timeZone.${property}`);
    }
    return target[property];
  },
});

const descriptor = Object.getOwnPropertyDescriptor(Temporal.TimeZone.prototype, "id");
const result = descriptor.get.call(timeZone);
assert.compareArray(actual, expected);
assert.sameValue(result, "time zone");
