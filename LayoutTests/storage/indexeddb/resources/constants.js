if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's constants.");

function test()
{
    removeVendorPrefixes();
    shouldBe("IDBDatabaseException.UNKNOWN_ERR", "1");
    shouldBe("IDBDatabaseException.NON_TRANSIENT_ERR", "2");
    shouldBe("IDBDatabaseException.CONSTRAINT_ERR", "4");
    shouldBe("IDBDatabaseException.DATA_ERR", "5");
    shouldBe("IDBDatabaseException.NOT_ALLOWED_ERR", "6");
    shouldBe("IDBDatabaseException.TRANSACTION_INACTIVE_ERR", "7");
    shouldBe("IDBDatabaseException.READ_ONLY_ERR", "9");
    shouldBe("IDBDatabaseException.VER_ERR", "12");

    shouldBe("IDBDatabaseException.TIMEOUT_ERR", "DOMException.TIMEOUT_ERR");
    shouldBe("IDBDatabaseException.QUOTA_ERR", "DOMException.QUOTA_EXCEEDED_ERR");
    shouldBe("IDBDatabaseException.ABORT_ERR", "DOMException.ABORT_ERR");
    shouldBe("IDBDatabaseException.NOT_FOUND_ERR", "DOMException.NOT_FOUND_ERR");
}

test();

finishJSTest();
