if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description('Ensure durability of transaction is expected');

indexedDBTest(onOpenUpgradeNeeded, onOpenSuccess);

function onOpenUpgradeNeeded(event)
{
    preamble(event);

    database = evalAndLog('database = event.target.result');
    database.createObjectStore('objectstore');
    shouldBeEqualToString('event.target.transaction.durability', 'default');
}

function onOpenSuccess(event)
{
    preamble(event);

    transaction = evalAndLog("transaction = database.transaction('objectstore')");
    transaction.oncomplete = onTransactionComplete;
    shouldBeEqualToString('transaction.durability', 'default');
}

function onTransactionComplete(event)
{
    preamble(event);

    shouldBeEqualToString('transaction.durability', 'default');
    finishJSTest();
}