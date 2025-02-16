/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// JSON.stringify ignores symbol-keyed properties, even enumerable ones.

var obj = {a: 1};
obj[Symbol.for("ponies")] = {toJSON: function () { throw "fit"; }};
obj[Symbol.iterator] = {toJSON: function () { throw "fit"; }};
assert.sameValue(JSON.stringify(obj), '{"a":1}');

var replacer = function (k, v) {
    if (typeof k === "symbol")
        throw "fit";
    return v;
};
assert.sameValue(JSON.stringify(obj, replacer), '{"a":1}');

