var EXPECTED_VERSION_AFTER_RELOAD = '3';

var db = openDatabaseSync("ChangeVersionTest", "", "Test the changeVersion() function.", 1);
if (db.version == EXPECTED_VERSION_AFTER_RELOAD)
    postMessage("PASS: db.version is " + EXPECTED_VERSION_AFTER_RELOAD + " as expected.");
else
    postMessage("FAIL: db.version is " + db.version + "; expected " + EXPECTED_VERSION_AFTER_RELOAD);

// Reset the version; otherwise this test will fail the next time it's run
db.changeVersion(db.version, "1");

postMessage("done");
