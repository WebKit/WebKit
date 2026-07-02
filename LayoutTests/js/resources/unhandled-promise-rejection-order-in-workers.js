importScripts('../../resources/js-test-pre.js');

description('Test unhandled promise rejection event.');

var global = this;
global.jsTestIsAsync = true;

global.error = null;
global.promise = [];
global.count = 0;
global.index = 0;
global.onunhandledrejection = function (e) {
    error = e;
    index = count++;

    shouldBe(`error.type`, `"unhandledrejection"`);
    shouldBe(`error.cancelable`, `true`);
    shouldBe(`error.promise`, `promise[${index}]`);
    shouldBe(`error.reason`, `${index}`);

    if (count === 3)
        finishJSTest();
    return false;
};

for (let i = 0; i < 3; ++i)
    global.promise[i] = Promise.reject(i);
