if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB index .get() required argument");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    objectStore = evalAndLog("objectStore = db.createObjectStore('foo', { keyPath: 'id', autoIncrement: true });");
    index = evalAndLog("index = objectStore.createIndex('first', 'first');");
    shouldThrow("index.get();");
    shouldThrow("index.getKey();");
    finishJSTest();
}
