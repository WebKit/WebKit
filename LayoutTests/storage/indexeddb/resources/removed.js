if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Ensure that some obsolete IndexedDB features are gone.");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();

    shouldBeUndefined("self.webkitIDBDatabaseError");
    shouldBeFalse("'IDBDatabaseException' in self");
    shouldBeFalse("'errorCode' in indexedDB.open(dbname)");
    shouldBeFalse("'setVersion' in IDBDatabase.prototype");

    if ('document' in self) {
        shouldThrow("document.createEvent('IDBUpgradeNeededEvent')");
        shouldBeFalse("'version' in document.createEvent('IDBVersionChangeEvent')");
    }

    finishJSTest();
}

test();
