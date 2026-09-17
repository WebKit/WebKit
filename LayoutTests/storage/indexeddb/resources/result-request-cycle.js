if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('../../../resources/gc.js');
    importScripts('shared.js');
}

description("Verify that IDBRequest is not leaked when there is a reference cycle for result attribute");

const requestCount = 100;

indexedDBTest(prepareDatabase, onOpen);

function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    store.put({ value: 'value1' }, 'key1');
    store.put({ value: 'value2' }, 'key2');
}

function onOpen(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("tx = db.transaction('store')");
    evalAndLog("store = tx.objectStore('store')");

    debug("Issue " + requestCount + " get requests for 'key1'.");
    getRequests = [];
    for (let i = 0; i < requestCount; ++i)
        getRequests.push(store.get('key1'));

    results = [];
    requestRefs = [];
    resultRefs = [];

    // A WeakRef keeps its target alive for the rest of the turn it was created in, so the
    // references are taken here and every collection check happens in a later event handler.
    getRequests[requestCount - 1].onsuccess = (evt) => {
        preamble(evt);

        debug("Verify that the request's result can be accessed lazily:");
        evalAndLog("gc()");

        for (let i = 0; i < requestCount; ++i) {
            let result = getRequests[i].result;
            result.source = getRequests[i];
            results.push(result);
            requestRefs.push(new WeakRef(getRequests[i]));
            resultRefs.push(new WeakRef(result));
        }
        shouldBeEqualToString("results[0].value", "value1");
    }

    evalAndLog("getRequest2 = store.get('key2')");
    getRequest2.onsuccess = () => {
        shouldBeEqualToString("getRequest2.result.value", "value2");

        debug("Ensure requests are not released while their results are referenced.");
        nukeArray(getRequests);
        evalAndLog("gc()");
        shouldBeFalse("anyCollected(requestRefs)");
        shouldBeFalse("anyCollected(resultRefs)");
    }

    evalAndLog("getRequest3 = store.get('key2')");
    getRequest3.onsuccess = () => {
        debug("Ensure requests and results are released once the results are dropped.");
        nukeArray(results);
        evalAndLog("gc()");
        shouldBeTrue("anyCollected(requestRefs)");
        shouldBeTrue("anyCollected(resultRefs)");
    }

    tx.oncomplete = finishJSTest;
}
