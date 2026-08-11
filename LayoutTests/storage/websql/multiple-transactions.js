var complete = 0;

function checkCompletion()
{
    if (++complete == 2 && window.testRunner)
        testRunner.notifyDone();
}

function runTest()
{
    var db = openDatabaseWithSuffix("MultipleTransactionsTest", "1.0", "Test to make sure multiple transactions can be queued at once for an HTML5 database", 32768);

    db.transaction(function(tx) {
        log("Transaction 1 Started");
    }, function(err) {
        log("Transaction 1 Errored - " + err);
        checkCompletion();
    }, function() {
        log("Transaction 1 Succeeded");
        checkCompletion();
    });

    db.transaction(function(tx) {
        log("Transaction 2 Started");
    }, function(err) {
        log("Transaction 2 Errored - " + err);
        checkCompletion();
    }, function() {
        log("Transaction 2 Succeeded");
        checkCompletion();
    });
}
