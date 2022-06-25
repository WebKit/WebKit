if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

var transaction;
var store;
var db;
var body = document.getElementsByTagName("iframe")[0].contentDocument.body;

description("Test that pending transactions are not completed during recursive JS calls until all JS (in all frames) is finished.");

indexedDBTest(prepareDatabase, click);
function prepareDatabase()
{
    db = event.target.result;
    store = db.createObjectStore('objectStore', null);
}

function click() {
    body.onclick = test;
    var pendingTransaction = evalAndLog("pendingTransaction = db.transaction(['objectStore'], 'readwrite')");
    pendingTransaction.onerror = unexpectedErrorCallback;
    pendingTransaction.onabort = unexpectedAbortCallback;
    pendingTransaction.oncomplete = completeCallback;
    var event = document.createEvent("MouseEvent");
    event.initMouseEvent("click", true, true, document.defaultView, 0, 0, 0, 0, 0, false, false, false, false, 0, null);
    body.dispatchEvent(event);
    var store = evalAndLog("store = pendingTransaction.objectStore('objectStore')");
    shouldBeTrue("store !== undefined");
    body.onclick = undefined;
}

function test()
{
    debug("Start re-entrant JS");
    transaction = evalAndLog("transaction = db.transaction(['objectStore'], 'readwrite')");
    debug("End re-entrant JS");
}


function completeCallback()
{
    debug("Pending transaction completed");
    finishJSTest();
}
