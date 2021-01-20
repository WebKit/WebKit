importScripts('../../resources/js-test-pre.js');

description('Test unhandled promise rejection event.');

var global = this;
global.jsTestIsAsync = true;

global.error = null;
global.promise = null;
global.onunhandledrejection = function (e) {
    error = e;
    shouldBe(`error.type`, `"unhandledrejection"`);
    shouldBe(`error.cancelable`, `true`);
    shouldBe(`error.promise`, `promise`);
    shouldBe(`error.reason`, `"ERROR"`);
    finishJSTest();
    return false;
};

global.promise = Promise.reject("ERROR");
