if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Ensure that some obsolete IndexedDB features are gone.");

function test()
{
    removeVendorPrefixes();
    shouldBeUndefined("self.webkitIDBDatabaseError");
    finishJSTest();
}

test();
