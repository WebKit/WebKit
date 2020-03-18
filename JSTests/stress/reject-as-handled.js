function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var set = new Set;
var count = 0;
setUnhandledRejectionCallback(function (promise) {
    shouldBe(set.has(promise), true);
    ++count;
});
var promise1 = Promise.reject(42);
set.add(promise1);
var promise2 = new Promise(function () { });
$vm.rejectPromiseAsHandled(promise2, 42);

// If it is already rejected, rejectPromiseAsHandled is no-op: not marking the promise as handled.
var promise3 = Promise.reject(43);
set.add(promise3);
$vm.rejectPromiseAsHandled(promise3, 43);

drainMicrotasks();
shouldBe(count, 2);

promise1.then($vm.abort, function (value) {
    shouldBe(value, 42);
    ++count;
});
promise2.then($vm.abort, function (value) {
    shouldBe(value, 42);
    ++count;
});
promise3.then($vm.abort, function (value) {
    shouldBe(value, 43);
    ++count;
});
drainMicrotasks();
shouldBe(count, 5);
