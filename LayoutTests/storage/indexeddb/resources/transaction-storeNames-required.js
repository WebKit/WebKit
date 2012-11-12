if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB: transaction storeNames required arguments");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    shouldThrow("db.transaction();");
    finishJSTest();
}
