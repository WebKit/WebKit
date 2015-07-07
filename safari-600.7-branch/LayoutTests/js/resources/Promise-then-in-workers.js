importScripts('../../resources/js-test-pre.js');

description('Test Promise.');

var global = this;

global.jsTestIsAsync = true;

var resolve;

var firstPromise = new Promise(function(newResolve) {
  global.thisInInit = this;
  resolve = newResolve;
});

var secondPromise = firstPromise.then(function(result) {
  global.thisInFulfillCallback = this;
  shouldBeFalse('thisInFulfillCallback === firstPromise');
  shouldBeFalse('thisInFulfillCallback === secondPromise');
  shouldBeTrue('thisInFulfillCallback === global');
  global.result = result;
  shouldBeEqualToString('result', 'hello');
  return 'world';
});

shouldBeFalse('thisInInit === firstPromise');
shouldBeTrue('thisInInit === global');
shouldBeTrue('firstPromise instanceof Promise');
shouldBeTrue('secondPromise instanceof Promise');

secondPromise.then(null, 37).then(function(result) {
  global.result = result;
  shouldBeEqualToString('result', 'world');
  throw 'exception'
}).then(1, 2).then(function() {
  testFailed('resolved');
}, function(result) {
  testPassed('rejected');
  global.result = result;
  shouldBeEqualToString('result', 'exception');
}).then(function() {
  testPassed('resolved');
  finishJSTest();
}, function() {
  testFailed('rejected');
  finishJSTest();
});

resolve('hello');


