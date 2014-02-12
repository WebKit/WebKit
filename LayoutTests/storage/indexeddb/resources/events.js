if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's event interfaces.");

function test()
{
    removeVendorPrefixes();
    shouldBeTrue("'IDBVersionChangeEvent' in self");

    if ('document' in self) {
        shouldBeTrue("'oldVersion' in document.createEvent('IDBVersionChangeEvent')");
        shouldBeTrue("'newVersion' in document.createEvent('IDBVersionChangeEvent')");
        shouldBeTrue("'dataLoss' in document.createEvent('IDBVersionChangeEvent')");
    }

    finishJSTest();
}

test();
