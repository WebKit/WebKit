if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB object store required arguments");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");
    shouldThrow("objectStore.put();");
    shouldThrow("objectStore.add();");
    shouldThrow("objectStore.delete();");
    shouldThrow("objectStore.get();");
    shouldThrow("objectStore.createIndex();");
    shouldThrow("objectStore.createIndex('foo');");
    shouldThrow("objectStore.index();");
    shouldThrow("objectStore.deleteIndex();");
    finishJSTest();
}
