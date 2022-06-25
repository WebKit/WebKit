if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Verify Indexed DB transactions are ordered per spec");

indexedDBTest(prepareDatabase, onOpen);

function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
}

function onOpen(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    debug("");
    debug("Create in order tx1, tx2");
    evalAndLog("tx1 = db.transaction('store', 'readwrite')");
    evalAndLog("tx2 = db.transaction('store', 'readwrite')");
    debug("");
    debug("Use in order tx2, tx1");
    evalAndLog("tx2.objectStore('store').get(0)");
    evalAndLog("tx1.objectStore('store').get(0)");
    debug("");
    evalAndLog("order = []");

    tx1.oncomplete = function(evt) {
        debug("tx1 complete");
        order.push(1);
        if (order.length === 2) done();
    };

    tx2.oncomplete = function(evt) {
        debug("tx1 complete");
        order.push(2);
        if (order.length === 2) done();
    };

    function done() {
        preamble();
        // IndexedDB Spec:
        // https://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#transaction-concept
        //
        // If multiple "readwrite" transactions are attempting to
        // access the same object store (i.e. if they have overlapping
        // scope), the transaction that was created first must be the
        // transaction which gets access to the object store first.
        //
        shouldBeTrue("areArraysEqual(order, [ 1, 2 ])");
        finishJSTest();
    }
}
