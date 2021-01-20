importScripts('../../resources/js-test-pre.js');

description('Test rejection handled event.');

var global = this;
global.jsTestIsAsync = true;

global.error = null;
global.promise = null;
global.reason = null;

global.onunhandledrejection = function (e) {
    error = e;
    shouldBe(`error.type`, `"unhandledrejection"`);
    shouldBe(`error.cancelable`, `true`);
    shouldBe(`error.promise`, `promise`);
    shouldBe(`error.reason`, `"ERROR"`);
    promise.catch(function (r) {
        global.reason = r;
        shouldBe(`reason`, `"ERROR"`);
    });

    setTimeout(function () {
        finishJSTest();
    }, 100);
    return false;
};

global.onrejectionhandled = function (e) {
    testFailed(`rejectionhandled is fired.`);
};

global.promise = Promise.reject("ERROR");
