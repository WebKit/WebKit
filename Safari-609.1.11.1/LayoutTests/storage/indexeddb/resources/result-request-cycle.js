if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Verify that IDBRequest is not leaked when there is a reference cycle for result attribute");

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

    evalAndLog("getRequest = store.get('key1')");
    getRequest.onsuccess = (evt) => {
        preamble(evt);

        debug("Verify that the request's result can be accessed lazily:");
        evalAndLog("gc()");

        evalAndLog("result = getRequest.result");
        shouldBeEqualToString("result.value", "value1");
        evalAndLog("result.source = getRequest");
    }

    evalAndLog("getRequest2 = store.get('key2')");
    getRequest2.onsuccess = (evt) => {
        shouldBeEqualToString("getRequest2.result.value", "value2");

        getRequestObervation = internals.observeGC(getRequest);
        resultObservation = internals.observeGC(result);
        evalAndLog("getRequest = null");
        evalAndLog("gc()");
        shouldBeFalse("getRequestObervation.wasCollected");
        shouldBeFalse("resultObservation.wasCollected");

        evalAndLog("result = null");
        evalAndLog("gc()");
        shouldBeTrue("getRequestObervation.wasCollected");
        shouldBeTrue("resultObservation.wasCollected");
    }

    tx.oncomplete = finishJSTest;
}