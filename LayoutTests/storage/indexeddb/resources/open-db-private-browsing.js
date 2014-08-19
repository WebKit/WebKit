if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Simple test to open IndexedDB database in private browsing mode.");

testRunner.setPrivateBrowsingEnabled(true);
removeVendorPrefixes();
shouldBeNull("indexedDB.open('db')");
finishJSTest();
