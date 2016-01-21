if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function log(msg) {
    document.getElementById("logger").innerHTML += msg + "<br>";
}

var dbname = setDBNameFromPath();

var request1 = indexedDB.open(dbname + "1");
request1.onupgradeneeded = function(e) {
    log("First request: " + e.oldVersion + " " + e.newVersion);
}

var request2 = indexedDB.open(dbname + "2", 1);
request2.onupgradeneeded = function(e) {
    log("Second request: " + e.oldVersion + " " + e.newVersion);
}

var request3 = indexedDB.open(dbname + "3", 2);
request3.onupgradeneeded = function(e) {
    log("Third request: " + e.oldVersion + " " + e.newVersion);
    if (window.testRunner)
        testRunner.notifyDone();
}

try {
    var request = indexedDB.open(dbname + "4", 0);
} catch (e) {
    log("0 version: " + e);
}

try {
    var request = indexedDB.open(dbname + "5", -1);
} catch (e) {
    log("Negative version: " + e);
}

try {
    var request = indexedDB.open(dbname + "6", "string");
} catch (e) {
    log("String version: " + e);
}
