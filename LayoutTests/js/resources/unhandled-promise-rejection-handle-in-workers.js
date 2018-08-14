importScripts('../../resources/js-test-pre.js');

description('Test rejection handled event.');

var global = this;
global.jsTestIsAsync = true;

global.error = null;
global.handled = null;
global.promise = null;
global.reason = null;

global.onunhandledrejection = function (e) {
    error = e;
    shouldBe(`error.type`, `"unhandledrejection"`);
    shouldBe(`error.cancelable`, `true`);
    shouldBe(`error.promise`, `promise`);
    shouldBe(`error.reason`, `"ERROR"`);
    setTimeout(function () {
        promise.catch(function (r) {
            global.reason = r;
            shouldBe(`reason`, `"ERROR"`);
        });
    }, 0);
    return false;
};

global.onrejectionhandled = function (e) {
    global.handled = e;
    shouldBe(`handled.type`, `"rejectionhandled"`);
    shouldBe(`handled.cancelable`, `false`);
    shouldBe(`handled.promise`, `promise`);
    shouldBe(`handled.reason`, `"ERROR"`);
    finishJSTest();
};

global.promise = Promise.reject("ERROR");
