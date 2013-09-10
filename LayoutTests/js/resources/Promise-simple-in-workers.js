importScripts('../../resources/js-test-pre.js');

description('Test Promise.');

var global = this;

global.jsTestIsAsync = true;

var resolver;

var firstPromise = new Promise(function(newResolver) {
  global.thisInInit = this;
  resolver = newResolver;
});

var secondPromise = firstPromise.then(function(result) {
  global.thisInFulfillCallback = this;
  shouldBeTrue('thisInFulfillCallback === secondPromise');
  global.result = result;
  shouldBeEqualToString('result', 'hello');
  finishJSTest();
});

shouldBeTrue('thisInInit === firstPromise');

resolver.fulfill('hello');
