importScripts('../../resources/js-test-pre.js');

description('Test rejected promises are returned from bindings and trigger unhandledrejection.');

var global = this;
global.jsTestIsAsync = true;

global.error = null;
global.promise = null;
global.onunhandledrejection = function (e) {
    error = e;
    shouldBe(`error.promise`, `promise`);
    shouldBeTrue(`error.reason instanceof TypeError`);
};

try {
    promise = PromiseRejectionEvent.prototype.promise;
} catch (e) {
    testFailed("TypeErrors produced by getters for Promise results should be wrapped in a Promise");
}

setTimeout(function () { finishJSTest(); }, 100);
