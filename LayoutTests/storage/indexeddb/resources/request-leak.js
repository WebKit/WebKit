if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('../../../resources/gc.js');
    importScripts('shared.js');
}

description("Verify that that requests weakly hold script value properties");

const requestCount = 100;

indexedDBTest(prepareDatabase, onOpen);

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

    debug("Issue " + requestCount + " get requests.");
    requests = [];
    for (let i = 0; i < requestCount; ++i)
        requests.push(store.get('key'));

    tx.oncomplete = async function onTransactionComplete() {
        preamble();
        evalAndLog("db.close()");
        shouldBeEqualToString("typeof requests[0].result", "object");

        // Verify that the same object is returned on each access to request.result.
        evalAndLog("requests[0].result.x = 123");
        shouldBe("requests[0].result.x", "123");

        // Try and induce a leak by a reference cycle from DOM to JS and back. If the JS value of
        // request.result (etc) is only held by the request's wrapper then there will be no leak.
        debug("Give every request a result that refers back to the request.");
        requestRefs = [];
        for (let i = 0; i < requestCount; ++i) {
            requests[i].result.leak = requests[i];
            requestRefs.push(new WeakRef(requests[i]));
        }
        nukeArray(requests);

        collected = await gcUntil(() => anyCollected(requestRefs));
        shouldBeTrue("collected");
        finishJSTest();
    };
}
