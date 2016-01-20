description("This test creates some object stores with keypaths. It then puts some values in them. It makes sure the keys used are as expected.");


if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("KeypathBasicTestDatabase", 1);

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    var database = event.target.result;    
    var objectStore1 = database.createObjectStore("OS1", { keyPath: "foo" });
    var objectStore2 = database.createObjectStore("OS2", { keyPath: "foo.bar" });

    var object = new Object;
    object.stuff = "bar1";
    object.foo = "foo1";
        
    var request1 = objectStore1.put(object);
    request1.onsuccess = function(event) {
        debug("object put SUCCESS - " + request1.result);
    }

    var array = { foo: "foo2", stuff: "bar2" };

    var request2 = objectStore1.put(array);
    request2.onsuccess = function(event) {
        debug("array put SUCCESS - " + request2.result);
    }
    
    object.foo = new Object;
    object.foo.bar = "baz1";
    var request3 = objectStore2.put(object);
    request3.onsuccess = function(event) {
        debug("Second object put SUCCESS - " + request3.result);
    }

    array.foo = { bar: "baz2" };

    var request4 = objectStore2.put(array);
    request4.onsuccess = function(event) {
        debug("Second array put SUCCESS - " + request4.result);
    }

    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction unexpected aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction complete");
        done();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}


