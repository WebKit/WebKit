if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB ordering of pending open calls.");

indexedDBTest(prepareDatabase, runTest, {version: 2});

function prepareDatabase()
{
}

function runTest(evt)
{
    preamble(evt);
    evalAndLog("connection = event.target.result");

    debug("");
    debug("First a delete request, which will defer subsequent opens. The delete itself will be blocked by the open connection.");
    evalAndLog("deleteRequest = indexedDB.deleteDatabase(dbname)");
    deleteRequest.onerror = unexpectedErrorCallback;
    deleteRequest.onsuccess = onDeleteSuccess;

    debug("");
    debug("Now three open requests:");
    evalAndLog("order = []");
    evalAndLog("request1 = indexedDB.open(dbname, 2)");
    request1.onsuccess = onRequest1Success;
    evalAndLog("request2 = indexedDB.open(dbname)");
    request2.onsuccess = onRequest2Success;
    evalAndLog("request3 = indexedDB.open(dbname, 2)");
    request3.onsuccess = onRequest3Success;

    debug("");
    debug("Close the connection to unblock the delete");
    evalAndLog("connection.close()");
}

function onDeleteSuccess(evt)
{
    preamble(evt);
}

function onRequest1Success(evt)
{
    preamble(evt);
    evalAndLog("order.push(1)");
    if (order.length === 3)
        checkOrder();
}

function onRequest2Success(evt)
{
    preamble(evt);
    evalAndLog("order.push(2)");
    if (order.length === 3)
        checkOrder();
}

function onRequest3Success(evt)
{
    preamble(evt);
    evalAndLog("order.push(3)");
    if (order.length === 3)
        checkOrder();
}

function checkOrder()
{
    preamble();
    shouldBeEqualToString("JSON.stringify(order)", "[1,2,3]");
    finishJSTest();
}
