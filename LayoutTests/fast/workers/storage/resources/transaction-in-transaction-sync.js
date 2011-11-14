var db = openDatabaseSync("TransactionInTransactionTest", "1.0", "Test that trying to run a nested transaction fails.", 1);
db.transaction(function(tx) {
    try {
        db.transaction(function(nestedTx) { });
        postMessage("FAIL: Trying to run a nested transaction should throw an exception." + db.lastErrorMessage);
    } catch (err) {
        postMessage("PASS: Exception thrown while trying to run a nested transaction (" + db.lastErrorMessage + ").");
    }
});

postMessage("done");
