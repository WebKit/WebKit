importScripts('../../resources/js-test-pre.js');

description('Test unhandled promise rejection event.');

var global = this;
global.jsTestIsAsync = true;

global.error = null;
global.promise = [];
global.onunhandledrejection = function (e) {
    error = e;

    shouldBe(`error.type`, `"unhandledrejection"`);
    shouldBe(`error.cancelable`, `true`);
    shouldBe(`error.promise`, `promise[0]`);
    shouldBe(`error.reason`, `0`);

    evalAndLog(`global.promise[1].catch(function () { });`);
    evalAndLog(`global.promise[2].catch(function () { });`);
    setTimeout(function () { finishJSTest(); }, 100);
    return false;
};

for (let i = 0; i < 3; ++i)
    evalAndLog(`global.promise[${i}] = Promise.reject(${i});`);
