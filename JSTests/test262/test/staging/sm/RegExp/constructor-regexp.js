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
var BUGNUMBER = 1130860;
var summary = "RegExp constructor shouldn't invoke source/flags getters on argument RegExp instance.";

print(BUGNUMBER + ": " + summary);

// same-compartment
var a = /foo/;
var flagsCalled = false;
var sourceCalled = false;
Object.defineProperty(a, "source", { get: () => {
  sourceCalled = true;
  return "bar";
}});
Object.defineProperty(a, "flags", { get: () => {
  flagsCalled = true;
  return "i";
}});

assert.sameValue(a.source, "bar");
assert.sameValue(a.flags, "i");
assert.sameValue(sourceCalled, true);
assert.sameValue(flagsCalled, true);

sourceCalled = false;
flagsCalled = false;
assert.sameValue(new RegExp(a).source, "foo");
assert.sameValue(sourceCalled, false);
assert.sameValue(flagsCalled, false);

// cross-compartment
var g = createNewGlobal();
var b = g.eval(`
var b = /foo2/;
var flagsCalled = false;
var sourceCalled = false;
Object.defineProperty(b, "source", { get: () => {
  sourceCalled = true;
  return "bar2";
}});
Object.defineProperty(b, "flags", { get: () => {
  flagsCalled = true;
  return "i";
}});
b;
`);

assert.sameValue(b.source, "bar2");
assert.sameValue(b.flags, "i");
assert.sameValue(g.eval("sourceCalled;"), true);
assert.sameValue(g.eval("flagsCalled;"), true);

g.eval(`
sourceCalled = false;
flagsCalled = false;
`);
assert.sameValue(new RegExp(b).source, "foo2");
assert.sameValue(g.eval("sourceCalled;"), false);
assert.sameValue(g.eval("flagsCalled;"), false);

