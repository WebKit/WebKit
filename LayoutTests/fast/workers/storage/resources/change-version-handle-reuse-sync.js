var db = null;
try {
    var dbName = "ChangeVersionHandleReuseTest" + (new Date()).getTime();
    db = openDatabaseSync(dbName, "", "Test that the DB handle is valid after changing the version.", 1);
    var version = db.version;
    var newVersion = version ? (parseInt(version) + 1).toString() : "1";
    db.changeVersion(version, newVersion, function(tx) {
        postMessage("PASS: changeVersion() transaction callback was called.");
    });
} catch (err) {
    postMessage("FAIL: changeVersion() threw an exception: " + err);
}

try {
    db.transaction(function(tx) {
        try {
            tx.executeSql("CREATE TABLE IF NOT EXISTS Test (Foo INT)");
            tx.executeSql("SELECT * from Test");
            postMessage("PASS: No exception thrown while executing statements.");
        } catch (err) {
            postMessage("FAIL: An exception was thrown while executing statements: " + err);
        }
    });
    postMessage("PASS: No exception thrown while running a transaction.");
} catch (err) {
    postMessage("FAIL: An exception was thrown while running a transaction: " + err);
}

postMessage("done");
