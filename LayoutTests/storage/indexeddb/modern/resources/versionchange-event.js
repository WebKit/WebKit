description("This test: \
-Opens a connection to a database at version 1, creating the database \
-Commits the version change transaction for that database \
-Opens a second connection to that database, requesting version 1 \
-Opens a third connection to that database, requesting version 2 \
-Makes sure the first and second connections get the versionchange event \
-Closes the first and second connections \
-Makes sure the versionchange transaction for the second connection starts successfully");

indexedDBTest(prepareDatabase, openSuccess);


function done()
{
    finishJSTest();
}

var connection1;
var connection2;

function openSuccess()
{
    debug("First version change successful");
}

var dbname;
function prepareDatabase(e)
{
    var database = event.target.result;
    dbname = database.name;

    debug("upgradeneeded (firstPhase): old version - " + e.oldVersion + " new version - " + e.newVersion);
    event.target.transaction.oncomplete = function()
    {
        debug("Version change complete (firstPhase). Database version is now - " + database.version);

        connection1 = database;
        connection1.onversionchange = function(e)
        {
            connection1.oldVersion = e.oldVersion;
            connection1.newVersion = e.newVersion;
            connection1.close();
        }
        secondPhase();
    }
    event.target.transaction.onabort = function()
    {
        debug("Version change transaction unexpected abort (firstPhase)");
        done();
    }
    event.target.transaction.onerror = function()
    {
        debug("Version change transaction unexpected error (firstPhase)");
        done();
    }
}

function secondPhase()
{
    var request = window.indexedDB.open(dbname, 1);
    request.onsuccess = function()
    {
        debug("Open success (secondPhase)");
        connection2 = request.result;
        connection2.onversionchange = function(e)
        {
            connection2.oldVersion = e.oldVersion;
            connection2.newVersion = e.newVersion;
            connection2.close();
        }
        thirdPhase();
    }
    request.onerror = function(e)
    {
        debug("Unexpected open error (secondPhase)" + e);
        done();
    }
    request.onupgradeneeded = function(e)
    {
    	debug("Unexpected upgrade needed (secondPhase)");
    	done();
    }
}

function thirdPhase()
{
    var request = window.indexedDB.open(dbname, 2);
    debug("thirdPhase - Requested database connection with version 2");
    request.onsuccess = function()
    {
        debug("Version change to version 2 successful");
    }
    request.onerror = function(e)
    {
        debug("Unexpected open error (thirdPhase)" + e);
        done();
    }
    request.onupgradeneeded = function(e)
    {
    	debug("Expected upgrade needed (thirdPhase)");
    	debug("firstPhase connection had received oldVersion: " + connection1.oldVersion + ", newVersion: " + connection1.newVersion);
    	debug("secondPhase connection had received oldVersion: " + connection2.oldVersion + ", newVersion: " + connection2.newVersion);

    	done();
    }
}

