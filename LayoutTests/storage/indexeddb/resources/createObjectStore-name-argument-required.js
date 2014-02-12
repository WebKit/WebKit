if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB createObjectStore name argument is required");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = evalAndLog("db = event.target.result");
    shouldThrow("db.createObjectStore();");
    finishJSTest();
}
