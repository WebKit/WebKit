if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Check to make sure we can enable IndexedDB in workers via a runtime setting");

var propertiesToTest = ['indexedDB', 'IDBCursor', 'IDBCursorWithValue', 'IDBDatabase', 'IDBFactory', 'IDBIndex', 'IDBKeyRange', 'IDBObjectStore', 'IDBOpenDBRequest', 'IDBRequest', 'IDBTransaction', 'IDBVersionChangeEvent'];

for (var i = 0; i < propertiesToTest.length; i++) {
    propertyToTest = propertiesToTest[i];
    shouldBeDefined("self." + propertyToTest);
    shouldBeNonNull("self." + propertyToTest);
    shouldBeTrue("'" + propertyToTest + "' in self");
}

shouldBeTrue("self.indexedDB instanceof IDBFactory");

finishJSTest();
