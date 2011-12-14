var db = openDatabaseSync("InterruptDatabaseSyncTest", "1", "", 1);
db.transaction(function(tx) {
    tx.executeSql("CREATE TABLE IF NOT EXISTS Test (Foo INT)");
    var counter = 0;
    while (true) {
        // Put both statements on the same line, to make sure the exception is always reported on the same line.
        tx.executeSql("INSERT INTO Test VALUES (1)"); tx.executeSql("DELETE FROM Test WHERE Foo = 1");
        if (++counter == 100)
            postMessage("terminate");
    }
});
