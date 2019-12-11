if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Simple test to open IndexedDB database in private browsing mode.");

removeVendorPrefixes();
shouldBeNull("indexedDB.open('db')");
finishJSTest();
