// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-RegExp-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1120169;
var summary = "Implement RegExp.prototype.source";

print(BUGNUMBER + ": " + summary);

assert.sameValue(RegExp.prototype.source, "(?:)");
assert.sameValue(/foo/.source, "foo");
assert.sameValue(/foo/iymg.source, "foo");
assert.sameValue(/\//.source, "\\/");
assert.sameValue(/\n\r/.source, "\\n\\r");
assert.sameValue(/\u2028\u2029/.source, "\\u2028\\u2029");
assert.sameValue(RegExp("").source, "(?:)");
assert.sameValue(RegExp("", "mygi").source, "(?:)");
assert.sameValue(RegExp("/").source, "\\/");
assert.sameValue(RegExp("\n\r").source, "\\n\\r");
assert.sameValue(RegExp("\u2028\u2029").source, "\\u2028\\u2029");

assertThrowsInstanceOf(() => genericSource(), TypeError);
assertThrowsInstanceOf(() => genericSource(1), TypeError);
assertThrowsInstanceOf(() => genericSource(""), TypeError);
assertThrowsInstanceOf(() => genericSource({}), TypeError);
assertThrowsInstanceOf(() => genericSource(new Proxy(/foo/, {get(){ return true; }})), TypeError);

function genericSource(obj) {
    return Object.getOwnPropertyDescriptor(RegExp.prototype, "source").get.call(obj);
}

