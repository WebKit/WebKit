var creationCallbackCalled1 = false;
var db1Name = "OpenDatabaseCreationCallback1" + (new Date()).getTime();
var db2Name = "OpenDatabaseCreationCallback2" + (new Date()).getTime();
var db1 = openDatabaseSync(db1Name, "1.0", "", 1,
                           function(db) {
                               postMessage("PASS: Creation callback was called.");
                               if (db.version != "")
                                   postMessage("FAIL: Wrong version " + db.version + "; empty string expected.");
                               else
                                   postMessage("PASS: Version set to empty string as expected.");
                           });

var db1Fail = null;
try {
    db1Fail = openDatabaseSync(db1Name, "1.0", "", 1);
    postMessage("FAIL: An INVALID_STATE_ERR exception should've been thrown.");
} catch(err) {
    if (db1Fail)
        postMessage("FAIL: db1Fail should have been null.");
    else
        postMessage("PASS: An exception was thrown and db1Fail is null as expected.");
}

// Open a handle to another database, first without a creation callback, then with one.
// Make sure the creation callback is not called.
var db2 = openDatabaseSync(db2Name, "1.0", "", 1);
db2 = openDatabaseSync(db2Name, "1.0", "", 1, function(db) { postMessage("FAIL: Creation callback should not have been called."); });

postMessage("done");
