function executeStatement(tx, expectedToPass, statement)
{
    var message;
    try {
        tx.executeSql(statement);
        message = expectedToPass ? "PASS: " : "FAIL: ";
    } catch (err) {
        message = !expectedToPass ? "PASS: " : "FAIL: ";
    }
    postMessage(message + statement + ", " + db.lastErrorMessage);
}

var db = openDatabaseSync("ExecuteSQLAcceptsOnlyOneStatementTest", "1.0", "", 1);
db.transaction(function(tx) {
    tx.executeSql("CREATE TABLE IF NOT EXISTS Test (Foo INT)");
    executeStatement(tx, true, "INSERT INTO Test VALUES (1)");
    executeStatement(tx, true, "INSERT INTO Test VALUES (2);");
    executeStatement(tx, true, "   INSERT INTO Test VALUES (3)    ");
    executeStatement(tx, true, "   INSERT INTO Test VALUES (4);   ");
    executeStatement(tx, true, "INSERT INTO Test VALUES (5)   ;");
    executeStatement(tx, false, "INSERT INTO Test VALUES (6); garbage");
    executeStatement(tx, false, "INSERT INTO Test VALUES (7); INSERT INTO Test VALUES (8)");
    executeStatement(tx, false, "  INSERT INTO Test VALUES (9);   INSERT INTO Test VALUES (10);   ");
});
postMessage("done");
