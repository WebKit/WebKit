if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB undefined as record value");

function test()
{
    removeVendorPrefixes();
    shouldThrow("indexedDB.open();");
    finishJSTest();
}

test();