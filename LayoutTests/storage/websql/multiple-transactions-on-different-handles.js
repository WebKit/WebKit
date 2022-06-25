var complete = 0;

function checkCompletion()
{
    // The test should end after two transactions
    if (++complete == 2 && window.testRunner)
        testRunner.notifyDone();
}

// Opens the database used in this test case
function openTestDatabase()
{
    return openDatabaseWithSuffix("MultipleTransactionsOnDifferentHandlesTest",
                                  "1.0",
                                  "Test to make sure that queueing multiple transactions on different DB handles does not result in a deadlock.",
                                  32768);
}

function statementSuccessCallback(dbName, statementType)
{
    log(dbName + " " + statementType + " statement succeeded");
}

function statementErrorCallback(dbName, statementType, error)
{
    log(dbName + " " + statementType + " statement failed: " + error.message);
}

// Runs a transaction on the given database
function runTransaction(db, dbName, val)
{
    db.transaction(function(tx) {
       // Execute a read-only statement
       tx.executeSql("SELECT COUNT(*) FROM Test;", [],
                     function(result) { statementSuccessCallback(dbName, "read"); },
                     function(tx, error) { statementErrorCallback(dbName, "read", error); });

       // Execute a write statement to make sure SQLite tries to acquire an exclusive lock on the DB file
       tx.executeSql("INSERT INTO Test VALUES (?);", [val],
                     function(result) { statementSuccessCallback(dbName, "write"); },
                     function(tx, error) { statementErrorCallback(dbName, "write", error); });
       }, function(error) {
           // Transaction failure callback
           log(dbName + " transaction failed: " + error.message);
           checkCompletion();
       }, function() {
           // Transaction success callback
           log(dbName + " transaction succeeded");
           checkCompletion();
       });
}

// We need to guarantee that the Test table exists before we run our test.
// Therefore, the test code is in the successCallback of the transaction that creates the table.
function runTest() {
    try {
        var db = openTestDatabase();
        db.transaction(function(tx) {
            // Create the Test table if it does not exist
            tx.executeSql("CREATE TABLE IF NOT EXISTS Test (Foo int);", [],
                          function(result) {}, function(tx, error) {});
            }, function(error) {
                log("Creating the Test table failed: " + error.message);
            }, function() {
                // The Test table was created successfully
                var db1 = openTestDatabase();
                var db2 = openTestDatabase();
                if (db1 == db2)
                    log("failure: db1 == db2");
                else {
                    runTransaction(db1, "db1", 1);
                    runTransaction(db2, "db2", 2);
                }
            });
    } catch(err) {}
}


