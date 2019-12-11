//@ skip
// To execute this test, need to specify the JSC_exposeInternalModuleLoader environment variable and execute it on non Windows platform.
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function shouldResolve(name, referrer, expected)
{
    var promise = Loader.resolve(name, referrer);
    return promise.then(function (actual) {
        shouldBe(actual, expected);
    });
}

function shouldThrow(name, referrer, errorMessage)
{
    var notThrown = false;
    return Loader.resolve(name, referrer).then(function (error) {
        notThrown = true;
    }).catch(function (error) {
        shouldBe(String(error), errorMessage);
    }).then(function () {
        if (notThrown)
            throw new Error("not thrown");
    });
}

var error = null;

// On windows platform, all "/" becomes "\".
Promise.all([
    shouldResolve('tmp.js', '/home/WebKit/', '/home/WebKit/tmp.js'),
    shouldResolve('tmp.js', '/home/', '/home/tmp.js'),
    shouldResolve('/tmp.js', '/home/WebKit/', '/tmp.js'),
    shouldResolve('///tmp.js', '/home/WebKit/', '/tmp.js'),
    shouldResolve('.///tmp.js', '/home/WebKit/', '/home/WebKit/tmp.js'),
    shouldResolve('./../tmp.js', '/home/WebKit/', '/home/tmp.js'),
    shouldResolve('./../../tmp.js', '/home/WebKit/', '/tmp.js'),
    shouldResolve('./../../../tmp.js', '/home/WebKit/', '/tmp.js'),
    shouldResolve('./../../home/../tmp.js', '/home/WebKit/', '/tmp.js'),
    shouldResolve('./../../../home/WebKit/../tmp.js', '/home/WebKit/', '/home/tmp.js'),
    shouldResolve('../home/WebKit/tmp.js', '/home/WebKit/', '/home/home/WebKit/tmp.js'),
    shouldResolve('../home/WebKit/../tmp.js', '/home/WebKit/', '/home/home/tmp.js'),
    shouldResolve('./tmp.js', '/home/WebKit/hello.js', '/home/WebKit/tmp.js'),

    shouldResolve('./tmp.js', 'C:/', 'C:/tmp.js'),
    shouldResolve('./tmp.js', 'C:/home/', 'C:/home/tmp.js'),
    shouldResolve('../tmp.js', 'C:/home/', 'C:/tmp.js'),
    shouldResolve('../../tmp.js', 'C:/home/', 'C:/tmp.js'),
    shouldResolve('./hello/tmp.js', 'C:/home/', 'C:/home/hello/tmp.js'),
    shouldResolve('/tmp.js', 'C:/home/', 'C:/tmp.js'),

    shouldThrow('/tmp.js', '', `Error: Could not resolve the referrer name ''.`),
    shouldThrow('/tmp.js', 'hello', `Error: Could not resolve the referrer name 'hello'.`),
    shouldThrow('tmp.js', 'hello', `Error: Could not resolve the referrer name 'hello'.`),
]).catch(function (e) {
    error = e;
});

// Force to run all pending tasks.
drainMicrotasks();
if (error)
    throw error;
