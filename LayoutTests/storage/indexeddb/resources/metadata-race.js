if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Ensure IndexedDB transactions created before open onsuccess have correct metadata");

indexedDBTest(prepareDatabase, onOpenSuccess);
function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("store.createIndex('index', 'keyPath')");
    evalAndLog("transaction = event.target.transaction");
    shouldNotThrow("index = transaction.objectStore('store').index('index')");
    transaction.oncomplete = onTransactionComplete;
}

function onTransactionComplete(evt)
{
    preamble(evt);
    debug("In multiprocess mode, 'complete' event may be dispatched before\n" +
          "the 'success' arrives with updated metadata. Ensure the new metadata\n" +
          "is still used for transactions.");
    evalAndLog("store = db.transaction('store').objectStore('store')");
    shouldNotThrow("index = store.index('index')");
}

function onOpenSuccess(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.transaction('store').objectStore('store')");
    shouldNotThrow("index = store.index('index')");
    finishJSTest();
}
