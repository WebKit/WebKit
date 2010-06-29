function finishTest()
{
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var TOTAL_TESTS = 7;
var testsRun = 0;
function transactionErrorCallback(error, expectedErrorCodeName)
{
    if (error.code == error[expectedErrorCodeName]) {
        log("PASS: expected and got error code " + expectedErrorCodeName);
        if (++testsRun == TOTAL_TESTS)
            finishTest();
    } else {
        log("FAIL: expected error code " + expectedErrorCodeName + " (" + error[expectedErrorCodeName] + "); got " + error.code);
        finishTest();
    }
}

function transactionSuccessCallback()
{
    log("FAIL: a transaction has completed successfully.");
    finishTest();
}

function testTransaction(db, transactionCallback, expectedErrorCodeName)
{
    db.transaction(transactionCallback,
                   function(error) {
                       transactionErrorCallback(error, expectedErrorCodeName);
                   }, transactionSuccessCallback);
}

function testTransactionThrowsException(db)
{
    testTransaction(db, function(tx) { throw "Exception thrown in transaction callback."; }, "UNKNOWN_ERR");
}

function testTransactionFailureBecauseOfStatementFailure(db)
{
    testTransaction(db,
                    function(tx) {
                        tx.executeSql("BAD STATEMENT", [], null, function(tx, error) { return true; });
                    }, "UNKNOWN_ERR");
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
    testTransaction(db,
                    function(tx) {
                        tx.executeSql("CREATE TABLE IF NOT EXISTS QuotaTest (Foo BLOB)");
                        tx.executeSql("INSERT INTO QuotaTest VALUES (ZEROBLOB(10 * 1024 * 1024))");
                    }, "QUOTA_ERR");
}

function testVersionMismatch(db)
{
    // Use another DB handle to change the version. However, in order to make sure that the DB version is not
    // changed before the transactions in the other tests have run, we need to do it in a transaction on 'db'.
    db.transaction(function(tx) {
        var db2 = openDatabaseWithSuffix("SQLErrorCodesTest", "1.0", "Tests the error codes.", 1);
        db2.changeVersion("1.0", "2.0", function(tx) { },
                          function(error) {
                              log("FAIL: could not change the DB version.");
                              finishTest();
                          }, function() { });
        });

    testTransaction(db,
                    function(tx) {
                        tx.executeSql("THIS STATEMENT SHOULD NEVER GET EXECUTED");
                    }, "VERSION_ERR");
}

function runTest()
{
    if (window.layoutTestController)
        layoutTestController.clearAllDatabases();

    var db = openDatabaseWithSuffix("SQLErrorCodesTest", "1.0", "Tests the error codes.", 1);
    testTransactionThrowsException(db);
    testTransactionFailureBecauseOfStatementFailure(db);
    testInvalidStatement(db);
    testIncorrectNumberOfBindParameters(db);
    testBindParameterOfWrongType(db);
    testQuotaExceeded(db);
    testVersionMismatch(db);
}
