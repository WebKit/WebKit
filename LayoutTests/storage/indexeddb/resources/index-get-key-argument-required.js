if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB index .get() required argument");

function test()
{
    removeVendorPrefixes();

    name = self.location.pathname;
    request = evalAndLog("indexedDB.open(name)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("request = db.setVersion('1')");
    request.onsuccess = createAndPopulateObjectStore;
    request.onerror = unexpectedErrorCallback;
}

function createAndPopulateObjectStore()
{
    deleteAllObjectStores(db);

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo', { keyPath: 'id', autoIncrement: true });");
    index = evalAndLog("index = objectStore.createIndex('first', 'first');");
    shouldThrow("index.get();");
    shouldThrow("index.getKey();");
    finishJSTest();
}

test();