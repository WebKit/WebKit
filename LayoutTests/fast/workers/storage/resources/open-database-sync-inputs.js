var db;

var notAString = {
    toString: function() { throw "foo"; }
};

try {
    db = openDatabaseSync();
    postMessage("FAIL: calling openDatabaseSync() without any argument should throw an exception.");
} catch (err) {
    postMessage("PASS: " + err.message);
}

try {
    db = openDatabaseSync("DBName", "DBVersion");
    postMessage("FAIL: calling openDatabaseSync() with fewer than four arguments should throw an exception.");
} catch (err) {
    postMessage("PASS: " + err.message);
}

try {
    db = openDatabaseSync(notAString, "DBVersion", "DBDescription", 1024);
    postMessage("FAIL: the first argument to openDatabaseSync() must be a string.");
} catch (err) {
    postMessage("PASS: " + err.message);
}

try {
    db = openDatabaseSync("DBName", notAString, "DBDescription", 1024);
    postMessage("FAIL: the second argument to openDatabaseSync() must be a string.");
} catch (err) {
    postMessage("PASS: " + err.message);
}

try {
    db = openDatabaseSync("DBName", "DBVersion", notAString, 1024);
    postMessage("FAIL: the fourth argument to openDatabaseSync() must be an integer.");
} catch (err) {
    postMessage("PASS: " + err.message);
}

try {
    db = openDatabaseSync("DBName", "DBVersion", "DBDescription", 1024, 0);
    postMessage("FAIL: the fifth argument to openDatabaseSync() must be an object, if present.");
} catch (err) {
    postMessage("PASS: " + err.message);
}

// FIXME: Uncomment these tests when the sync DB API is implemented.
//try {
//    db = openDatabaseSync("DBName", "DBVersion", "DBDescription", 1024);
//    postMessage("PASS: openDatabaseSync() succeeded.");
//} catch (err) {
//    postMessage("FAIL: " + err.message);
//}
//
//try {
//    db = openDatabaseSync("DBName", "DBVersion", "DBDescription", 1024, function(db) { });
//    postMessage("PASS: calling openDatabaseSync() with a creation callback succeeded.");
//} catch (err) {
//    postMessage("FAIL: " + err.message);
//}

postMessage("done");
