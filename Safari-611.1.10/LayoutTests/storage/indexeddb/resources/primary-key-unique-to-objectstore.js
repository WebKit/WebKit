if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

if (testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function log(msg) {
    document.getElementById("logger").innerHTML += msg + "<br>";
}

var dbname = setDBNameFromPath();
var openRequest = indexedDB.open(dbname, 1);

openRequest.onupgradeneeded = function(e) {
    var thisDB = e.target.result;

    if(!thisDB.objectStoreNames.contains("people")) {
        thisDB.createObjectStore("people", {keyPath:"id"});
    }

    if(!thisDB.objectStoreNames.contains("notes")) {
        thisDB.createObjectStore("notes", {keyPath:"uid"});
    }
}

openRequest.onsuccess = function(e) {
    db = e.target.result;
    addPerson();
}    

openRequest.onerror = function(e) {
    log("openRequest error - " + e);
}

var sharedID = 0;

function addPerson() {
    log("About to add person and note");

    //Get a transaction
    //default for OS list is all, default for type is read
    var transaction = db.transaction(["people"], "readwrite");

    //Ask for the objectStore
    var store = transaction.objectStore("people");

    //Define a person
    var person = {
        name: "Person",
        id: sharedID
    }

    //Perform the add
    var request = store.add(person);

    request.onerror = function(e) {
        log("Error adding person - ", e);
    }

    request.onsuccess = function(e) {
        log("Successfully added person");
        setTimeout("addComplete();", 0);
    }
    
    //Define a note
    var note = {
        note: "Note",
        uid: sharedID
    }

    var transaction2 = db.transaction(["notes"], "readwrite");

    //Ask for the objectStore
    var store2 = transaction2.objectStore("notes");

    //Perform the add
    var request2 = store2.add(note);

    request2.onerror = function(e) {
        log("Error adding note - ", e);
    }

    request2.onsuccess = function(e) {
        log("Successfully added note");
        setTimeout("addComplete();", 0);
    }
    
}

var completedAdds = 0;
function addComplete()
{
    if (++completedAdds < 2)
        return;

    //Get a transaction
    var transaction = db.transaction(["people"], "readwrite");

    //Ask for the objectStore
    var store = transaction.objectStore("people");

    //Perform the add
    var request = store.get(sharedID);

    request.onerror = function(e) {
        log("Error getting person - ", e);
    }

    request.onsuccess = function(e) {
        log("Successfully got person");
        for (n in e.target.result) {
            log(n);
        }
        getComplete();
    }

    var transaction2 = db.transaction(["notes"], "readwrite");

    //Ask for the objectStore
    var store2 = transaction2.objectStore("notes");

    //Perform the add
    var request2 = store2.get(sharedID);

    request2.onerror = function(e) {
        log("Error getting note - ", e);
    }

    request2.onsuccess = function(e) {
        log("Successfully got note");
        for (n in e.target.result) {
            log(n);
        }
        getComplete();
    }
}

var completedGets = 0;
function getComplete()
{
    if (++completedGets == 2 && testRunner)
        testRunner.notifyDone();
}
