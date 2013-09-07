importScripts('../../../resources/js-test-pre.js');

description('Test Promise.');

var global = this;
global.jsTestIsAsync = true;

var firstPromise = new Promise(function(resolver) {
  resolver.resolve('hello');
});

var secondPromise = firstPromise.then(function(result) {
  global.thisInFulfillCallback = this;
  shouldBeTrue('thisInFulfillCallback === secondPromise');
  global.result = result;
  shouldBeEqualToString('result', 'hello');
  finishJSTest();
}, function() {
  fail('rejected');
  finishJSTest();
});
