importScripts('../../resources/js-test-pre.js');

description('Test Promise.');

var global = this;
global.jsTestIsAsync = true;

var firstPromise = new Promise(function(resolve) {
  resolve('hello');
});

var secondPromise = firstPromise.then(function(result) {
  global.thisInFulfillCallback = this;
  shouldBeFalse('thisInFulfillCallback === secondPromise');
  shouldBeTrue('thisInFulfillCallback === global');
  global.result = result;
  shouldBeEqualToString('result', 'hello');
  finishJSTest();
}, function() {
  fail('rejected');
  finishJSTest();
});
