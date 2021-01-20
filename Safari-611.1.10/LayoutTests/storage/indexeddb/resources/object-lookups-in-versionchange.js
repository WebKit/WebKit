if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Regression test for http://webkit.org/b/102547");

indexedDBTest(prepareDatabase, finishJSTest);

function prepareDatabase() {
    evalAndLog("db = event.target.result");
    evalAndLog("transaction = event.target.transaction");
    evalAndLog("store = db.createObjectStore('store')");
    debug("");
    evalAndExpectException("transaction.objectStore('no-such-store')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    debug("");
    evalAndExpectException("store.index('no-such-index')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
}
