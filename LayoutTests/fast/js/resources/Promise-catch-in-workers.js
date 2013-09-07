importScripts('../../../resources/js-test-pre.js');

description('Test Promise.');

var global = this;

global.jsTestIsAsync = true;
var resolver;

var firstPromise = new Promise(function(newResolver) {
  global.thisInInit = this;
  resolver = newResolver;
});

var secondPromise = firstPromise.catch(function(result) {
  global.thisInFulfillCallback = this;
  shouldBeFalse('thisInFulfillCallback === firstPromise');
  shouldBeTrue('thisInFulfillCallback === secondPromise');
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

shouldBeTrue('thisInInit === firstPromise');
shouldBeTrue('firstPromise instanceof Promise');
shouldBeTrue('secondPromise instanceof Promise');

shouldThrow('firstPromise.catch(null)', '"TypeError: Expected function or undefined as as first argument"');
shouldThrow('firstPromise.catch(37)', '"TypeError: Expected function or undefined as as first argument"');

resolver.reject('hello');

