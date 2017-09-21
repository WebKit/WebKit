// Copyright (C) 2017 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
author: Jordan Harband
description: finally observably calls .then
esid: sec-promise.prototype.finally
features: [Promise.prototype.finally]
flags: [async]
---*/

var initialThenCount = 0;
var noReason = {};
var no = Promise.reject(noReason);
no.then = function () {
  initialThenCount += 1;
  return Promise.prototype.then.apply(this, arguments);
};

var onFinallyThenCount = 0;
var yesValue = {};
var yes = Promise.resolve(yesValue);
yes.then = function () {
  onFinallyThenCount += 1;
  return Promise.prototype.then.apply(this, arguments);
};

var finallyCalled = false;
var catchCalled = false;

no.catch(function (e) {
  assert.sameValue(e, noReason);
  throw e;
}).finally(function () {
  finallyCalled = true;
  return yes;
}).catch(function (e) {
  catchCalled = true;
  assert.sameValue(e, noReason);
}).then(function () {
  assert.sameValue(finallyCalled, true, 'initial finally was called');
  assert.sameValue(initialThenCount, 1, 'initial finally invokes .then once');

  assert.sameValue(catchCalled, true, 'catch was called');
  assert.sameValue(onFinallyThenCount, 1, 'onFinally return promise has .then invoked once');
  $DONE();
}).catch($ERROR);
