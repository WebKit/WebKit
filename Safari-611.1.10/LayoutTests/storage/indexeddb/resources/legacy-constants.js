if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

testObsoleteConstants();
function testObsoleteConstants()
{
    debug("");
    debug("Verify that constants from previous version of the spec (beyond a grace period) have been removed:");

    // http://www.w3.org/TR/2010/WD-IndexedDB-20100819/
    shouldBe("IDBKeyRange.SINGLE", "undefined");
    shouldBe("IDBKeyRange.LEFT_OPEN", "undefined");
    shouldBe("IDBKeyRange.RIGHT_OPEN", "undefined");
    shouldBe("IDBKeyRange.LEFT_BOUND", "undefined");
    shouldBe("IDBKeyRange.RIGHT_BOUND", "undefined");

    // Unclear that this was ever in the spec, but it was present in mozilla tests:
    shouldBe("IDBTransaction.LOADING", "undefined");

    // http://www.w3.org/TR/2011/WD-IndexedDB-20111206/
    shouldBe("IDBRequest.LOADING", "undefined");
    shouldBe("IDBRequest.DONE", "undefined");

    // http://www.w3.org/TR/2011/WD-IndexedDB-20111206/
    shouldBe("IDBCursor.NEXT", "undefined");
    shouldBe("IDBCursor.NEXT_NO_DUPLICATE", "undefined");
    shouldBe("IDBCursor.PREV", "undefined");
    shouldBe("IDBCursor.PREV_NO_DUPLICATE", "undefined");

    // http://www.w3.org/TR/2011/WD-IndexedDB-20111206/
    shouldBe("IDBTransaction.READ_ONLY", "undefined");
    shouldBe("IDBTransaction.READ_WRITE", "undefined");
    shouldBe("IDBTransaction.VERSION_CHANGE", "undefined");

    finishJSTest();
}
