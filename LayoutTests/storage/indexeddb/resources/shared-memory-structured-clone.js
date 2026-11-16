if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test structured clone permutations in IndexedDB with shared memories.");

indexedDBTest(prepareDatabase, startTests);
function prepareDatabase()
{
    db = event.target.result;
    evalAndLog("store = db.createObjectStore('storeName')");
    debug("This index is not used, but evaluating key path on each put() call will exercise (de)serialization:");
    evalAndLog("store.createIndex('indexName', 'dummyKeyPath')");
}

async function startTests()
{
    debug("");
    debug("Running tests...");

    await testSharedWebAssemblyMemory();
    finishJSTest();
}

function testSharedWebAssemblyMemory()
{
    return new Promise((resolve, reject) => {
        debug("Test shared WebAssembly.Memory");

        evalAndLog("transaction = db.transaction('storeName', 'readwrite')");
        evalAndLog("store = transaction.objectStore('storeName')");
        transaction.onerror = reject;
        transaction.onabort = reject;
        transaction.oncomplete = resolve;

        memory = new WebAssembly.Memory({ initial: 1, maximum: 1, shared: true });
        shouldThrow(`store.put(memory, 'key')`);
    });
}
