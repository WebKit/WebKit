description("This test opens a new database, then aborts the version change transaction. \
It then reopens the database, making sure it's a default, empty database, and changes the version successfully. \
It then reopens the database, upgrading it's version. It aborts this versionchange, as well. \
Finally it reopens the database again, upgrading its version, making sure things had reverted back to before the second aborted versionchange.");

indexedDBTest(prepareDatabase);


function done()
{
    finishJSTest();
}

var dbname;
function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    event.target.onerror = null;

    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    dbname = database.name;
    
    versionTransaction.abort();

    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction aborted");
        continueTest1();
        database.close();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction unexpected complete");
        done();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction error " + event);
    }
}

function continueTest1()
{
    createRequest = window.indexedDB.open(dbname, 1);

    createRequest.onupgradeneeded = function(event) {
        debug("Second upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

        var versionTransaction = event.target.transaction;
        var database = event.target.result;

        versionTransaction.onabort = function(event) {
            debug("Second upgrade versionchange transaction unexpected abort");
            done();
        }

        versionTransaction.oncomplete = function(event) {
            debug("Second upgrade versionchange transaction complete");
            continueTest2();
            database.close();
        }

        versionTransaction.onerror = function(event) {
            debug("Second upgrade versionchange transaction unexpected error" + event);
            done();
        }
    }
}

function continueTest2()
{
    createRequest = window.indexedDB.open(dbname, 2);

    createRequest.onupgradeneeded = function(event) {
        debug("Third upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

        var versionTransaction = event.target.transaction;
        var database = event.target.result;

        versionTransaction.abort();
    
        versionTransaction.onabort = function(event) {
            debug("Third upgrade versionchange transaction aborted");
            continueTest3();
            database.close();
        }

        versionTransaction.oncomplete = function(event) {
            debug("Third upgrade versionchange transaction unexpected complete");
            done();
        }

        versionTransaction.onerror = function(event) {
            debug("Third upgrade versionchange transaction error" + event);
        }
    }
}

function continueTest3()
{
    createRequest = window.indexedDB.open(dbname, 2);

    createRequest.onupgradeneeded = function(event) {
        debug("Fourth upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
        var database = event.target.result;
        done();
    }
}


