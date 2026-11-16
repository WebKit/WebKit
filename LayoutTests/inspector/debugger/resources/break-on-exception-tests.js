TestPage.needToSanitizeUncaughtExceptionURLs = true;

var arr = [ 1, 2, 3 ];
var mapData = [[ "a", arr ]];

function doThrow() {
    console.log("throwing TestError");
    throw "TestError";
}

function testCatch() {
    console.log("testCatch");
    try {
        doThrow();
    } catch (e) {
        console.log("catch " + e);
    }
    console.log("DONE");
}

function testFinally() {
    console.log("testFinally");
    try {
        doThrow();
    } finally {
        console.log("finally");
    }
}

function testThrowingThruNativeCode() {
    console.log("testThrowingThruNativeCode");
    (new Map(mapData)).forEach(doThrow);
}

function testThrowingInPromise() {
    console.log("testThrowingInPromise");
    new Promise(function promise(resolve, reject) {
        console.log("in promise");
        doThrow();
    });
}

function testThrowingInPromiseWithCatch() {
    console.log("testThrowingInPromiseWithCatch");
    new Promise(function promise(resolve, reject) {
        console.log("in promise");
        doThrow();
    }).catch(function promiseCatch(e) {
        console.log("in promise.catch");
        console.log("DONE");
    });
}

function testThrowingInPromiseThen() {
    console.log("testThrowingInPromiseThen");
    new Promise(function promise(resolve, reject) {
        console.log("in promise");
        resolve();
    }).then(function promiseThen(x) {
        console.log("in promise.then");
        doThrow();
    });
}

function testThrowingInPromiseThenWithCatch() {
    console.log("testThrowingInPromiseThenWithCatch");
    new Promise(function promise(resolve, reject) {
        console.log("in promise");
        resolve();
    }).then(function promiseThen(x) {
        console.log("in promise.then");
        doThrow();
    }).catch(function promiseCatch(e) {
        console.log("in promise.catch");
        console.log("DONE");
    });
}

function testThrowingInPromiseWithRethrowInCatch() {
    console.log("testThrowingInPromiseWithRethrowInCatch");
    new Promise(function promise(resolve, reject) {
        console.log("in promise");
        doThrow();
    }).catch(function promiseCatch(e) {
        console.log("in promise.catch");
        throw e;
    });
}
