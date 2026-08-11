if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB open request does not crash on indexedDB.databases().");

indexedDBTest(testDoesNotCrash);
async function testDoesNotCrash()
{
    document.documentElement.append(document.createElement('frameset'));
    indexedDB.open('b', 1);
    indexedDB.open('c', 1);
    indexedDB.open('d', 1);
    for (let i = 0; i < 100; i++) {
        let idbOpenDBRequest = indexedDB.open('a', 1);
        for (let x of await indexedDB.databases()) {}
        let transaction = idbOpenDBRequest.transaction;
        if (transaction)
            transaction.oncomplete = () => location.hash = 'h';
    }
    finishJSTest();
}
