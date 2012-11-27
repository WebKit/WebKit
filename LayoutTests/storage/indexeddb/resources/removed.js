if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
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

    finishJSTest();
}

test();
