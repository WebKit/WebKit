function testRespondTwice()
{
    var event = self.internals.createBeingDispatchedFetchEvent();
    event.respondWith(undefined);
    try {
        event.respondWith(undefined);
        return 'Should throw';
    } catch (e) {
        return e.name === 'InvalidStateError' ? 'PASS' : 'Got exception ' + e;
    }
}

function gc()
{
    // FIXME: Add gc in WTR
    function gcRec(n) {
        if (n < 1)
            return {};
        var temp = {i: "ab" + i + (i / 100000)};
        temp += "foo";
        gcRec(n-1);
    }
    for (var i = 0; i < 10000; i++)
    gcRec(10);
}

function testRequestSameObject()
{
    var event = new FetchEvent('FetchEvent', { request : new Request('test') });
    event.request.value = 1;
    gc();
    return event.request.value === 1 ? "PASS" : "FAIL";
}

async function promise_rejects(promise)
{
    return promise.then(() => {
        return "FAIL";
    }, (e) => {
        return e.name === 'TypeError' ? 'PASS' : 'Got error ' + e;
    })
}

async function testRespondUndefined()
{
    var event = internals.createBeingDispatchedFetchEvent();
    var promise = internals.waitForFetchEventToFinish(event);
    event.respondWith(undefined);
    return await promise_rejects(promise);
}

async function testRespondNotResponse()
{
    var event = internals.createBeingDispatchedFetchEvent();
    var promise = internals.waitForFetchEventToFinish(event);
    event.respondWith(new Request(''));
    return await promise_rejects(promise);
}

async function testRespondPromiseNotResponse()
{
    var event = internals.createBeingDispatchedFetchEvent();
    var promise = internals.waitForFetchEventToFinish(event);
    event.respondWith(new Promise((resolve, reject) => {
        resolve(new Request(''));
    }));
    return await promise_rejects(promise);
}

async function testRespondPromiseReject()
{
    var event = internals.createBeingDispatchedFetchEvent();
    var promise = internals.waitForFetchEventToFinish(event);
    event.respondWith(new Promise((resolve, reject) => {
        reject('not good');
    }));
    return await promise_rejects(promise);
}

async function testRespondPromiseResponse()
{
    var event = internals.createBeingDispatchedFetchEvent();
    var response = new Response;
    event.respondWith(response);
    return (response === await internals.waitForFetchEventToFinish(event)) ? "PASS" : "FAIL";
}

async function doTest(event)
{
    try {
        var result = event.data + " is an unknown test";
        if (event.data === "respond-twice")
            result = testRespondTwice();
        else if (event.data === "request-sameobject")
            result = testRequestSameObject();
        else if (event.data === "respond-undefined")
            result = await testRespondUndefined();
        else if (event.data === "respond-not-response")
            result = await testRespondNotResponse();
        else if (event.data === "respond-promise-not-response")
            result = await testRespondPromiseNotResponse();
        else if (event.data === "respond-promise-reject")
            result = await testRespondPromiseReject();
        else if (event.data === "respond-promise-response")
            result = await testRespondPromiseResponse();

        event.source.postMessage(result);
    } catch (e) {
        event.source.postMessage("Exception: " + e.message);
    }
}

self.addEventListener("message", doTest);
