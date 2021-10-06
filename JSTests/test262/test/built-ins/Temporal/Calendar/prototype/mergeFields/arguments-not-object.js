// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.mergefields
description: Non-object arguments are converted with ToObject and merge their [[OwnPropertyKeys]]
includes: [compareArray.js]
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");

assert.throws(TypeError, () => calendar.mergeFields(undefined, {}));
assert.throws(TypeError, () => calendar.mergeFields({}, undefined));

assert.throws(TypeError, () => calendar.mergeFields(null, {}));
assert.throws(TypeError, () => calendar.mergeFields({}, null));

const boolResult = calendar.mergeFields(true, false);
assert.compareArray(Object.keys(boolResult), [], "Boolean objects have no own property keys");
assert.sameValue(Object.getPrototypeOf(boolResult), Object.prototype, "plain object returned");

const numResult = calendar.mergeFields(3, 4);
assert.compareArray(Object.keys(numResult), [], "Number objects have no own property keys");
assert.sameValue(Object.getPrototypeOf(boolResult), Object.prototype, "plain object returned");

const strResult = calendar.mergeFields("abc", "de");
assert.compareArray(Object.keys(strResult), ["0", "1", "2"], "String objects have integer indices as own property keys");
assert.sameValue(strResult["0"], "d");
assert.sameValue(strResult["1"], "e");
assert.sameValue(strResult["2"], "c");
assert.sameValue(Object.getPrototypeOf(boolResult), Object.prototype, "plain object returned");

const symResult = calendar.mergeFields(Symbol("foo"), Symbol("bar"));
assert.compareArray(Object.keys(symResult), [], "Symbol objects have no own property keys");
assert.sameValue(Object.getPrototypeOf(symResult), Object.prototype, "plain object returned");

const bigintResult = calendar.mergeFields(3n, 4n);
assert.compareArray(Object.keys(bigintResult), [], "BigInt objects have no own property keys");
assert.sameValue(Object.getPrototypeOf(bigintResult), Object.prototype, "plain object returned");
