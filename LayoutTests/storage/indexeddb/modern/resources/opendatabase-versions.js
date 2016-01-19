description("This test creates a new database with the default version, commits that versionchange transaction, and then reopens it at different versions to make sure the IDBOpenDBRequests behave appropriately.");



function log(msg)
{
    document.getElementById("logger").innerHTML += msg + "";
}

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var request = window.indexedDB.open("VersionTestDatabase");
debug(request + " (firstPhase)");

request.onsuccess = function()
{
    debug("First version change successful");
}
request.onerror = function(e)
{
    debug("Open request error (firstPhase) " + request.error.name);
}

request.onupgradeneeded = function(e)
{
    var database = event.target.result;

    debug("upgradeneeded (firstPhase): old version - " + e.oldVersion + " new version - " + e.newVersion);
    debug(request.transaction);
    request.transaction.oncomplete = function()
    {
        debug("Version change complete (firstPhase). Database version is now - " + database.version);
        database.close();
        secondPhase();
    }
    request.transaction.onabort = function()
    {
        debug("Version change transaction unexpected abort! (firstPhase)");
        done();
    }
    request.transaction.onerror = function()
    {
        debug("Version change transaction unexpected error! (firstPhase)");
        done();
    }
}

function secondPhase()
{
    var request = window.indexedDB.open("VersionTestDatabase", 1);
    debug(request + " (secondPhase)");
    request.onsuccess = function()
    {
        debug("Successfully opened database at version 1 (secondPhase)");
        request.result.close();
        request.result.close(); // Close it twice just for the heck of it
        thirdPhase();
    }
    request.onerror = function(e)
    {
        debug("Unexpected error (secondPhase)" + e);
        done();
    }
    request.onupgradeneeded = function(e)
    {
    	debug("Unexpected upgrade needed (secondPhase)" + e);
    	done();
    }
}

function thirdPhase()
{
    var request = window.indexedDB.open("VersionTestDatabase", 2);
    debug(request + " (thirdPhase)");
    request.onsuccess = function()
    {
        debug("Version change to version 2 successful");
    }
    request.onerror = function(e)
    {
        debug("Open request error (thirdPhase) " + request.error.name);
    }
    request.onupgradeneeded = function(e)
    {
        var database = event.target.result;

        debug("upgradeneeded (thirdPhase): old version - " + e.oldVersion + " new version - " + e.newVersion);
        debug(request.transaction);
        request.transaction.oncomplete = function()
        {
            debug("Version change complete (thirdPhase). Database version is now - " + database.version);
            database.close();
            fourthPhase();
        }
        request.transaction.onabort = function()
        {
            debug("Version change transaction unexpected abort! (thirdPhase)");
            done();
        }
        request.transaction.onerror = function()
        {
            debug("Version change transaction unexpected error! (thirdPhase)");
            done();
        }
    } 
}

function fourthPhase()
{
    // We've upgraded to version 2, so version 1 should not be openable.
    var request = window.indexedDB.open("VersionTestDatabase", 1);
    debug(request + " (fourthPhase)");
    request.onsuccess = function()
    {
        debug("Unexpected success (fourthPhase)");
        done();
    }
    request.onerror = function(e)
    {
        debug("Expected error (fourthPhase) - " + request.error.name);
        done();
    }
    request.onupgradeneeded = function(e)
    {
        debug("Unexpected upgradeneeded (fourthPhase)");
        done();
    } 
}


