importScripts('./js-test-pre.js');

description('Test Promise.');

jsTestIsAsync = true;

var resolver;
var promise = new Promise(function(r) { resolver = r; });
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

resolver.reject('hello');

shouldBeEqualToString('promiseState', 'pending');

promise.then(function() {
  testFailed('promise is fulfilled.');
  finishJSTest();
}, function() {
  shouldBeEqualToString('promiseState', 'rejected');
  shouldBeEqualToString('promiseResult', 'hello');
  finishJSTest();
});
