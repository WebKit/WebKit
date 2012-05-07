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
    shouldBe("IDBDatabaseException.NOT_FOUND_ERR", "3");
    shouldBe("IDBDatabaseException.CONSTRAINT_ERR", "4");
    shouldBe("IDBDatabaseException.DATA_ERR", "5");
    shouldBe("IDBDatabaseException.NOT_ALLOWED_ERR", "6");
    shouldBe("IDBDatabaseException.TRANSACTION_INACTIVE_ERR", "7");
    shouldBe("IDBDatabaseException.ABORT_ERR", "8");
    shouldBe("IDBDatabaseException.READ_ONLY_ERR", "9");
    shouldBe("IDBDatabaseException.TIMEOUT_ERR", "10");
    shouldBe("IDBDatabaseException.QUOTA_ERR", "11");
    shouldBe("IDBDatabaseException.VER_ERR", "12");

    shouldBe("IDBCursor.NEXT", "0");
    shouldBe("IDBCursor.NEXT_NO_DUPLICATE", "1");
    shouldBe("IDBCursor.PREV", "2");
    shouldBe("IDBCursor.PREV_NO_DUPLICATE", "3");

    shouldBe("IDBTransaction.READ_ONLY", "0");
    shouldBe("IDBTransaction.READ_WRITE", "1");
    shouldBe("IDBTransaction.VERSION_CHANGE", "2");
}

test();

finishJSTest();