function terminateTest()
{
    if (window.testRunner)
        testRunner.notifyDone();
}

function logAndTerminateTest(message, error)
{
    log(message + ": " + error.message);
    terminateTest();
}

function cleanup(db)
{
    db.transaction(function(tx) {
        tx.executeSql("DROP TABLE IF EXISTS Results;");
        tx.executeSql("DROP TABLE IF EXISTS TempTable;");
        tx.executeSql("DROP TRIGGER IF EXISTS TempTrigger;");
    },
    function(error) { 
        logAndTerminateTest("Cleanup failed", error); 
    });
}

function statementSuccessCallback(statementType)
{
    log(statementType + " statement succeeded.");
}

function statementErrorCallback(statementType, error)
{
    log(statementType + " statement failed: " + error.message);
    return false;
}

function executeStatement(tx, statement, operation)
{
    tx.executeSql(statement, [],
    function(result) { 
        statementSuccessCallback(operation);
    },
    function(tx, error) { 
        return statementErrorCallback(operation, error); 
    });
}

function testCallbacks(tx)
{
	executeStatement(tx, "CREATE TABLE Results (Key TEXT, Value TEXT);", "CREATE TABLE");

	// Create a temporary table with the same schema as __WebKitDatabaseInfoTable__ and populate it with a valid version.
	executeStatement(tx, "CREATE TEMPORARY TABLE TempTable (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, value TEXT NOT NULL ON CONFLICT FAIL);", "CREATE TEMP TABLE");
	executeStatement(tx, "INSERT INTO TempTable VALUES ('WebKitDatabaseVersionKey','1.0');", "INSERT IN TEMP TABLE");

    // Set up a trigger to capture changes to the table we just created.
	executeStatement(tx, "CREATE TRIGGER TempTrigger BEFORE INSERT ON TempTable BEGIN INSERT INTO Results VALUES (NEW.key, NEW.value); END;", "CREATE TRIGGER");
	
	// Try to spoof that table as the info table.
	executeStatement(tx, "ALTER TABLE TempTable RENAME TO __WebKitDatabaseInfoTable__;", "ALTER TO INFO TABLE");
}

function testStep1(db)
{
    db.transaction(function(tx) {
        testCallbacks(tx);
    },
    function(error) { 
        logAndTerminateTest("Step 1 transaction failed", error); 
    },
    function() {
        log("Step 1 transaction succeeded.");
        testStep2(db); 
    });	
}

function testStep2(db)
{
    // At this point there's a temporary table named the same as the internal info table.
    // WebKit should not use it.
    db.changeVersion('1.0', '2.0', null, function(error) {
        log("Failed to change DB version - " + error.message);
    }, 
    function() {
        log("Successfully changed DB version");
    });
    
    // If our trigger fired it will have captured the changed to the info table and put them in the results table.
    db.transaction(function(tx) {
        tx.executeSql("SELECT * FROM Results;", [], function(tx, results) {
            if (results.rows.length == 0)
                return;
            log("The Results table actually has stuff in it, and it shouldn't");
            var result = results.rows.item(0);
            for (n in result)
                log(n + " " + result[n]);
        }); 
    },
    function(error) { 
        logAndTerminateTest("Step 2 transaction failed", error); 
    },
    function() { 
        log("Step 2 transaction succeeded.");
        terminateTest(); 
    });	
}

function runTest()
{
    var db = openDatabaseWithSuffix("AlterInfoTableTest", "1.0", "Tests altering the info table", 32768);
    cleanup(db);
    testStep1(db);
}
