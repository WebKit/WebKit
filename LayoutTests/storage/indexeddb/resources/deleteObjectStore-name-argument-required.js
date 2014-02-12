if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB deleteObjectStore required argument");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    shouldThrow("db.deleteObjectStore();");
    finishJSTest();
}
