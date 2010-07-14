var dbName = "SQLExceptionCodesTest" + (new Date()).getTime();

function testTransaction(db, executeStatementsCallback, expectedErrorCodeName)
{
    db.transaction(function(tx) {
        try {
            executeStatementsCallback(tx);
            postMessage("FAIL: an exception (" + expectedErrorCodeName + ") should've been thrown.");
        } catch (err) {
            if (err.code == err[expectedErrorCodeName])
                postMessage("PASS: expected and got error code " + expectedErrorCodeName);
            else
                postMessage("FAIL: expected error code " + expectedErrorCodeName + " (" +
                            err[expectedErrorCodeName] + "), got " + err.code);
        }
      });
}

function testTransactionThrowsException(db)
{
    testTransaction(db, function(tx) { throw "Exception thrown in transaction callback."; }, "UNKNOWN_ERR");
}

function testInvalidStatement(db)
{
    testTransaction(db, function(tx) { tx.executeSql("BAD STATEMENT"); }, "SYNTAX_ERR");
}

function testIncorrectNumberOfBindParameters(db)
{
    testTransaction(db,
                    function(tx) {
                        tx.executeSql("CREATE TABLE IF NOT EXISTS BadBindNumberTest (Foo INT, Bar INT)");
                        tx.executeSql("INSERT INTO BadBindNumberTest VALUES (?, ?)", [1]);
                    }, "SYNTAX_ERR");
}

function testBindParameterOfWrongType(db)
{
    var badString = { };
    badString.toString = function() { throw "Cannot call toString() on this object." };

    testTransaction(db, function(tx) {
        tx.executeSql("CREATE TABLE IF NOT EXISTS BadBindTypeTest (Foo TEXT)");
        tx.executeSql("INSERT INTO BadBindTypeTest VALUES (?)", [badString]);
    }, "UNKNOWN_ERR");
}

function testQuotaExceeded(db)
{
    // Sometimes, SQLite automatically rolls back a transaction if executing a statement fails.
    // This seems to be one of those cases.
    try {
        testTransaction(db,
                        function(tx) {
                            tx.executeSql("CREATE TABLE IF NOT EXISTS QuotaTest (Foo BLOB)");
                            tx.executeSql("INSERT INTO QuotaTest VALUES (ZEROBLOB(10 * 1024 * 1024))");
                        }, "QUOTA_ERR");
        postMessage("FAIL: Transaction should've been rolled back by SQLite.");
    } catch (err) {
        if (err.code == err["DATABASE_ERR"])
            postMessage("PASS: Transaction was rolled back by SQLite as expected.");
        else
            postMessage("FAIL: An unexpected exception was thrown: " + err);
    }
}

function testVersionMismatch(db)
{
    var db2 = openDatabaseSync(dbName, "1.0", "Tests the error codes.", 1);
    db2.changeVersion("1.0", "2.0", function(tx) { });
    testTransaction(db,
                    function(tx) {
                        tx.executeSql("THIS STATEMENT SHOULD NEVER GET EXECUTED");
                    }, "VERSION_ERR");
}

var db = openDatabaseSync(dbName, "1.0", "Tests the exception codes.", 1);
testTransactionThrowsException(db);
testInvalidStatement(db);
testIncorrectNumberOfBindParameters(db);
testBindParameterOfWrongType(db);
testQuotaExceeded(db);
testVersionMismatch(db);

postMessage("done");
