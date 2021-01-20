importScripts('../../resources/js-test-pre.js');

description('Test Promise.');

var global = this;

global.jsTestIsAsync = true;
var reject;

var firstPromise = new Promise(function(newResolve, newReject) {
  global.thisInInit = this;
  reject = newReject;
});

var secondPromise = firstPromise.catch(function(result) {
  global.thisInFulfillCallback = this;
  shouldBeFalse('thisInFulfillCallback === firstPromise');
  shouldBeFalse('thisInFulfillCallback === secondPromise');
  shouldBeTrue('thisInFulfillCallback === global');
  global.result = result;
  shouldBeEqualToString('result', 'hello');
  return 'bye';
});

secondPromise.then(function(result) {
  global.result = result;
  shouldBeEqualToString('result', 'bye');
  testPassed('fulfilled');
  finishJSTest();
}, function() {
  testFailed('rejected');
  finishJSTest();
}, function() {
});

shouldBeFalse('thisInInit === firstPromise');
shouldBeTrue('thisInInit === global');
shouldBeTrue('firstPromise instanceof Promise');
shouldBeTrue('secondPromise instanceof Promise');

reject('hello');

