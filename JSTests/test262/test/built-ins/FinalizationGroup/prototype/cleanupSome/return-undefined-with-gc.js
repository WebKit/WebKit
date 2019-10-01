// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.cleanupSome
description: Return undefined regardless the result of CleanupFinalizationGroup
info: |
  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ? CleanupFinalizationGroup(finalizationGroup, callback).
  6. Return undefined.
features: [FinalizationGroup, arrow-function, async-functions, async-iteration, class, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var called;
var fn = function() {
  called += 1;
  return 39;
};
var cb = function() {
  called += 1;
  return 42;
};
var fg = new FinalizationGroup(fn);

function emptyCells() {
  var target = {};
  fg.register(target);

  var prom = asyncGC(target);
  target = null;

  return prom;
}

var tests = [];

tests.push(emptyCells().then(function() {
  called = 0;
  assert.sameValue(fg.cleanupSome(cb), undefined, 'regular callback');
  assert.sameValue(called, 1);
}));

tests.push(emptyCells().then(function() {
  called = 0;
  assert.sameValue(fg.cleanupSome(fn), undefined, 'regular callback, same FG cleanup function');
  assert.sameValue(called, 1);
}));

tests.push(emptyCells().then(function() {
  called = 0;
  assert.sameValue(fg.cleanupSome(), undefined, 'undefined (implicit) callback, defer to FB callback');
  assert.sameValue(called, 1);
}));

tests.push(emptyCells().then(function() {
  called = 0;
  assert.sameValue(fg.cleanupSome(undefined), undefined, 'undefined (explicit) callback, defer to FB callback');
  assert.sameValue(called, 1);
}));

tests.push(emptyCells().then(function() {
  assert.sameValue(fg.cleanupSome(() => 1), undefined, 'arrow function');  
}));

tests.push(emptyCells().then(function() {
  assert.sameValue(fg.cleanupSome(async function() {}), undefined, 'async function');
}));

tests.push(emptyCells().then(function() {
  assert.sameValue(fg.cleanupSome(function *() {}), undefined, 'generator');
}));

tests.push(emptyCells().then(function() {
  assert.sameValue(fg.cleanupSome(async function *() {}), undefined, 'async generator');
}));

Promise.all(tests).then(() => { $DONE(); }, resolveAsyncGC);
