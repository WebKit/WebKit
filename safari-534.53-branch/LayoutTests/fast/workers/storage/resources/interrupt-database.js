var counter = 0;
function runTransaction()
{
    db.transaction(function(tx) {
        tx.executeSql("INSERT INTO Test VALUES (1)");
        tx.executeSql("DELETE FROM Test WHERE Foo = 1");
        if (++counter == 100)
            postMessage("terminate");
    }, null, runTransaction);
}

var db = openDatabase("InterruptDatabaseTest", "1", "", 1);
db.transaction(function(tx) {
    tx.executeSql("CREATE TABLE IF NOT EXISTS Test (Foo INT)");
}, function(error) {
    postMessage("Error: " + error.message);
}, function() {
    runTransaction();
});
