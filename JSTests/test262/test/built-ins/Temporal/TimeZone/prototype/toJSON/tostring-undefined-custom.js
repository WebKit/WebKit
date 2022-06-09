// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.tojson
description: TypeError thrown when toString property not present
includes: [compareArray.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  'get timeZone[@@toPrimitive]',
  'get timeZone.toString',
  'get timeZone.valueOf',
];

const timeZone = new Proxy(
  {
    toString: undefined
  },
  {
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
    }
  }
);

assert.throws(TypeError, () => Temporal.TimeZone.prototype.toJSON.call(timeZone));
assert.compareArray(actual, expected);
