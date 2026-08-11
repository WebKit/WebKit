importScripts('../../resources/js-test-pre.js');

var global = this;
global.jsTestIsAsync = true;

description('Test Promise.');

var thisInInit;
var resolve, reject;
var promise = new Promise(function(newResolve, newReject) {
  thisInInit = this;
  resolve = newResolve;
  reject = newReject;
});

shouldBeTrue('promise instanceof Promise');
shouldBe('promise.constructor', 'Promise');
shouldBeFalse('thisInInit === promise');
shouldBeTrue('thisInInit === global');
shouldBeTrue('resolve instanceof Function');
shouldBeTrue('reject instanceof Function');

shouldThrow('new Promise()', '"TypeError: Promise constructor takes a function argument"');
shouldThrow('new Promise(37)', '"TypeError: Promise constructor takes a function argument"');

shouldNotThrow('promise = new Promise(function() { throw Error("foo"); })');
promise.then(undefined, function(result) {
  global.result = result;
  shouldBeEqualToString('result.message', 'foo');
});

new Promise(function(resolve) {
  resolve("hello");
  throw Error("foo");
}).then(function(result) {
  global.result = result;
  testPassed('fulfilled');
  shouldBeEqualToString('result', 'hello');
  finishJSTest();
}, function(result) {
  global.result = result;
  testFailed('rejected');
  finishJSTest();
});


