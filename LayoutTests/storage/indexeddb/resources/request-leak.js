if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Verify that that requests weakly hold script value properties");

if (window.internals) {
    indexedDBTest(prepareDatabase, onOpen);
} else {
    testFailed('This test requires access to the Internals object');
    finishJSTest();
}

function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("store.put({value: 'value'}, 'key')");
}

function onOpen(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("tx = db.transaction('store')");
    evalAndLog("store = tx.objectStore('store')");
    evalAndLog("request = store.get('key')");
    tx.oncomplete = function onTransactionComplete() {
        preamble();
        evalAndLog("db.close()");
        shouldBeEqualToString("typeof request.result", "object");

        // Verify that the same object is returned on each access to request.result.
        evalAndLog("request.result.x = 123");
        shouldBe("request.result.x", "123");

        // Try and induce a leak by a reference cycle from DOM to V8 and back.
        // If the v8 value of request.result (etc) is only held by the requests's
        // V8 wrapper then there will be no leak.
        evalAndLog("request.result.leak = request");
        evalAndLog("observer = internals.observeGC(request)");
        evalAndLog("request = null");
        evalAndLog("gc()");
        shouldBeTrue("observer.wasCollected");
        finishJSTest();
    };
}
