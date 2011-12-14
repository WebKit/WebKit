var EXPECTED_VERSION_AFTER_HIXIE_TEST = "2";
var EXPECTED_VERSION_AFTER_RELOAD = "3";

var db1 = openDatabaseSync("ChangeVersionTest", "1", "Test for the database.changeVersion() function", 1);
var db2 = openDatabaseSync("ChangeVersionTest", "1", "Test for the database.changeVersion() function", 1);

// First run Hixie's test to ensure basic changeVersion() functionality works (see bug 28418).
db1.changeVersion("1", EXPECTED_VERSION_AFTER_HIXIE_TEST);
if (db2.version != db1.version) {
    postMessage("FAIL: changing db1's version (" + db1.version + ") did not change db2's version (" + db2.version + ") as expected.");
    postMessage("fail");
} else
    postMessage("PASS: changing db1's version (" + db1.version + ") changed db2's version too.");

db2.transaction(function(tx) {
    try {
        tx.executeSql("CREATE TABLE IF NOT EXISTS Test (Foo INT)");
        postMessage("FAIL: The DB version changed, executing any statement on db2 should fail.");
        postMessage("fail");
    } catch(err) {
      postMessage("PASS: Executing a statement on db2 threw an exception as expected.");
    }
});

// Make sure any new handle to the same DB sees the new version
try {
    var db3 = openDatabaseSync("ChangeVersionTest", EXPECTED_VERSION_AFTER_HIXIE_TEST, "", 1);
    postMessage("PASS: Successfully opened a new DB handle.");
} catch (err) {
    postMessage("FAIL: Unexpected exception thrown while trying to open a new DB handle: " + err);
    postMessage("fail");
}

if (db1.version != db3.version) {
    postMessage("FAIL: db1.version (" + db1.version + ") does not match db3.version(" + db3.version +")");
    postMessage("fail");
} else
    postMessage("PASS: db1.version (" + db1.version + ") matches db3.version.");

// Now try a test to ensure the version persists after reloading (see bug 27836)
db1.changeVersion(EXPECTED_VERSION_AFTER_HIXIE_TEST, EXPECTED_VERSION_AFTER_RELOAD,
                  function(tx) {
                      tx.executeSql("DROP TABLE IF EXISTS Info");
                      tx.executeSql("CREATE TABLE IF NOT EXISTS Info (Version INTEGER)");
                      tx.executeSql("INSERT INTO Info VALUES (?)", [EXPECTED_VERSION_AFTER_RELOAD]);
                  });

postMessage("done");
