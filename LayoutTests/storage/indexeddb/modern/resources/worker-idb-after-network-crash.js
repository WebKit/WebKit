// Worker script for testing IndexedDB recovery after network process crash.
var dbname = "worker-idb-crash-test-" + Date.now();

self.onmessage = function(event) {
    if (event.data === "open-db")
        openInitialDatabase();
    else if (event.data === "idb-after-crash")
        attemptRecoveryOpen();
};

function openInitialDatabase()
{
    var request = indexedDB.open(dbname, 1);
    request.onupgradeneeded = function(e) {
        e.target.result.createObjectStore("store", { keyPath: "id" });
    };
    request.onsuccess = function(e) {
        var db = e.target.result;
        var tx = db.transaction("store", "readwrite");
        tx.objectStore("store").put({ id: "key1", value: "hello" });
        tx.oncomplete = function() {
            // Keep DB open — main page will use its own onclose as the crash signal.
            self.postMessage("db-ready");
        };
        tx.onerror = function(e) {
            self.postMessage("recovery-fail:initial write: " + e.target.error);
        };
    };
    request.onerror = function(e) {
        self.postMessage("recovery-fail:initial open: " + e.target.error);
    };
}

function attemptRecoveryOpen()
{
    var newDbName = dbname + "-after-crash";
    var request = indexedDB.open(newDbName, 1);

    request.onupgradeneeded = function(e) {
        e.target.result.createObjectStore("recoveryStore", { keyPath: "id" });
    };

    request.onsuccess = function(e) {
        var db = e.target.result;
        var tx = db.transaction("recoveryStore", "readwrite");
        tx.objectStore("recoveryStore").put({ id: "recovered", value: "it works" });
        tx.oncomplete = function() {
            db.close();
            self.postMessage("recovery-ok");
        };
        tx.onerror = function(e) {
            db.close();
            self.postMessage("recovery-fail:write tx: " + e.target.error);
        };
    };

    request.onerror = function(e) {
        self.postMessage("recovery-fail:open: " + e.target.error);
    };
}
