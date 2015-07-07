try {
    var db = openDatabaseSync("EmptyStatementSync", "1.0", "Test an empty statement.", 1);
    db.transaction(function(tx) {
        var result = tx.executeSql("");
        postMessage(result ? "FAIL: result is not null" : "PASS: result is null");
    });
    postMessage("PASS");
} catch (err) {
    postMessage("FAIL");
}

postMessage("done");
