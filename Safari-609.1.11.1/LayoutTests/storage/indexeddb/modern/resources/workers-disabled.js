if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Check to make sure IndexedDB in workers can be disabled at runtime");

var propertiesToTest = ['indexedDB', 'IDBCursor', 'IDBCursorWithValue', 'IDBDatabase', 'IDBFactory', 'IDBIndex', 'IDBKeyRange', 'IDBObjectStore', 'IDBOpenDBRequest', 'IDBRequest', 'IDBTransaction', 'IDBVersionChangeEvent'];

for (var i = 0; i < propertiesToTest.length; i++) {
    propertyToTest = propertiesToTest[i];
    shouldBeUndefined("self." + propertyToTest);
    shouldBeFalse("'" + propertyToTest + "' in self");
}

finishJSTest();
