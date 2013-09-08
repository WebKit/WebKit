if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB keyrange required arguments");

function test()
{
    removeVendorPrefixes();

    shouldThrow("IDBKeyRange.only();");
    shouldThrow("IDBKeyRange.lowerBound();");
    shouldThrow("IDBKeyRange.upperBound();");
    shouldThrow("IDBKeyRange.bound(1);");
    shouldThrow("IDBKeyRange.bound();");

    finishJSTest();
}

test();