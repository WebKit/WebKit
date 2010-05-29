function terminateTest()
{
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

function openTestDatabase()
{
    return openDatabaseWithSuffix("ReadAndWriteTransactionsDontRunTogetherTest",
                                  "1.0",
                                  "Test to make sure that read and write transactions on different DB handles to the same DB don't run at the same time.",
                                  32768);
}

var readTransactionsInProgress = 0;
var writeTransactionsInProgress = 0;
var totalTransactions = 0;
var finishedTransactions = 0;

function runTransaction(db, readOnly)
{
    var transactionFunction = (readOnly ? db.readTransaction : db.transaction);
    transactionFunction.call(db, function(tx) {
            if (readOnly) {
                if (writeTransactionsInProgress != 0) {
                    log("Read transaction starting while write transaction in progress.");
                    terminateTest();
                }
                readTransactionsInProgress++;
            } else {
                if ((readTransactionsInProgress != 0) || (writeTransactionsInProgress != 0)) {
                    log("Write transaction starting while another transaction in progress.");
                    terminateTest();
                }
                writeTransactionsInProgress++;
            }
            tx.executeSql("SELECT * FROM Test;");
        }, function(error) {
            log((readOnly ? "Read" : "Write") + " transaction failed: " + error.message);
            terminateTest();
        }, function() {
             finishedTransactions++;
             if (readOnly)
                 readTransactionsInProgress--;
             else
                 writeTransactionsInProgress--;
             log("Transaction successful.");
             if ((finishedTransactions == totalTransactions) && (readTransactionsInProgress == 0) && (writeTransactionsInProgress == 0))
                 terminateTest();
        });
}

function runReadAndWriteTransactions(db1, db2, db3)
{
    totalTransactions = 10;
    finishedTransactions = 0;
    runTransaction(db1, true);
    runTransaction(db2, true);
    runTransaction(db1, false);
    runTransaction(db1, true);
    runTransaction(db2, true);
    runTransaction(db3, true);
    runTransaction(db1, false);
    runTransaction(db2, false);
    runTransaction(db1, true);
    runTransaction(db3, true);
}

function runTest() {
    var db1 = openTestDatabase();
    var db2 = openTestDatabase();
    var db3 = openTestDatabase();
    db1.transaction(function(tx) {
            tx.executeSql("CREATE TABLE IF NOT EXISTS Test (Foo int);");
        }, function(error) {
            log("Cannot create the Test table: " + error.message);
            terminateTest();
        }, function() {
            runReadAndWriteTransactions(db1, db2, db3);
        });
}
