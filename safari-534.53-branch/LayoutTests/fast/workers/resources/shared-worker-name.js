if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}

description("Checks the various use cases around the SharedWorker constructor's optional name parameter");

var currentTest = 0;
nextTest();

// Iterates through the tests until none are left.
function nextTest()
{
    currentTest++;
    var testFunction = window["test" + currentTest];
    if (testFunction)
        testFunction();
    else
        done();
}



function test1()
{
    // Make sure we can create a shared worker with no name.
    try {
        var worker = new SharedWorker('resources/shared-worker-common.js');
        testPassed("created SharedWorker with no name");
        worker.port.postMessage("eval self.foo = 1234");
        worker.port.onmessage = function(event) {
            shouldBeEqual("setting self.foo", event.data, "self.foo = 1234: 1234");
            nextTest();
        };
    } catch (e) {
        testFailed("SharedWorker with no name threw an exception: " + e);
        done();
    }
}

function test2()
{
    // Creating a worker with no name should match an existing worker with no name
    var worker = new SharedWorker('resources/shared-worker-common.js');
    worker.port.postMessage("eval self.foo");
    worker.port.onmessage = function(event) {
        shouldBeEqual("creating worker with no name", event.data, "self.foo: 1234");
        nextTest();
    }
}

function test3()
{
    // Creating a worker with an empty name should be the same as a worker with no name.
    var worker = new SharedWorker('resources/shared-worker-common.js', '');
    worker.port.postMessage("eval self.foo");
    worker.port.onmessage = function(event) {
        shouldBeEqual("creating worker with empty name", event.data, "self.foo: 1234");
        nextTest();
    };
}

function test4()
{
    // Creating a worker with a different name should not be the same as a worker with no name.
    var worker = new SharedWorker('resources/shared-worker-common.js', 'name');
    worker.port.postMessage("eval self.foo");
    worker.port.onmessage = function(event) {
        shouldBeEqual("creating worker with different name but same URL", event.data, "self.foo: undefined");
        nextTest();
    };
}

function test5()
{
    // Creating a worker for an alternate URL with no name should work.
    var worker = new SharedWorker('resources/shared-worker-common.js?url=1');
    worker.port.postMessage("eval self.foo");
    worker.port.onmessage = function(event) {
        shouldBeEqual("creating no-name worker with alternate URL", event.data, "self.foo: undefined");
        nextTest();
    };
}

function test6()
{
    // Creating a worker for an alternate URL with empty name should work.
    var worker = new SharedWorker('resources/shared-worker-common.js?url=2', '');
    worker.port.postMessage("eval self.foo");
    worker.port.onmessage = function(event) {
        shouldBeEqual("creating empty name worker with alternate URL", event.data, "self.foo: undefined");
        nextTest();
    };
}

function shouldBeEqual(description, a, b)
{
    if (a == b)
        testPassed(description);
    else
        testFailed(description + " - passed value: " + a + ", expected value: " + b);
}
