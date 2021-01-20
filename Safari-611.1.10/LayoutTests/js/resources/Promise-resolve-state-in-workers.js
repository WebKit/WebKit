importScripts('../../resources/js-test-pre.js');

description('Test Promise.');

jsTestIsAsync = true;

var resolve;
var promise = new Promise(function(r) { resolve = r; });
var promiseState = 'pending';
var promiseResult = undefined;
promise.then(function(result) {
  promiseState = 'fulfilled';
  promiseResult = result;
}, function(result) {
  promiseState = 'rejected';
  promiseResult = result;
});

shouldBeEqualToString('promiseState', 'pending');

resolve('hello');

shouldBeEqualToString('promiseState', 'pending');

promise.then(function() {
  shouldBeEqualToString('promiseState', 'fulfilled');
  shouldBeEqualToString('promiseResult', 'hello');
  finishJSTest();
}, function() {
  testFailed('promise is rejected.');
  finishJSTest();
});
