if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB object store required arguments");

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

test();