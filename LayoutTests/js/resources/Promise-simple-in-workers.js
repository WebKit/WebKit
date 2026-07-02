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
  shouldBeFalse('thisInFulfillCallback === secondPromise');
  shouldBeTrue('thisInFulfillCallback === global');
  global.result = result;
  shouldBeEqualToString('result', 'hello');
  finishJSTest();
});

shouldBeFalse('thisInInit === firstPromise');
shouldBeTrue('thisInInit === global');

resolve('hello');
