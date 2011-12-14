function runTransaction(db)
{
    db.transaction(function(tx) {
       // Execute a read-only statement
       tx.executeSql("SELECT COUNT(*) FROM Test;");

       // Execute a write statement to make sure SQLite tries to acquire an exclusive lock on the DB file
       tx.executeSql("INSERT INTO Test VALUES (?);", [1]);
    });
}

var db1 = openDatabaseSync("MultipleTransactionsOnDifferentHandlesTest", "1.0",
                           "Test transactions on different handles to the same DB.", 1);
db1.transaction(function(tx) {
    tx.executeSql("CREATE TABLE IF NOT EXISTS Test (Foo int);");
});

var db2 = openDatabaseSync("MultipleTransactionsOnDifferentHandlesTest", "1.0",
                           "Test transactions on different handles to the same DB.", 1);
if (db1 == db2)
    postMessage("FAIL: db1 == db2");
else {
    try {
        runTransaction(db1);
        runTransaction(db2);
        postMessage("PASS");
    } catch (err) {
        postMessage("FAIL: " + err);
    }
}

postMessage("done");
