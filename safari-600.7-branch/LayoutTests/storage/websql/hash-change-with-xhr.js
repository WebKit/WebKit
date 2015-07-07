var DB_UPDATE_INTERVAL = 100;
var SEND_XHR_INTERVAL = 100;
var BACK_INTERVAL = 100;
var CREATE_HEALTH_TABLE = 'CREATE TABLE IF NOT EXISTS health (key VARCHAR(16) PRIMARY KEY);';
var UPDATE_DATA = 'REPLACE INTO health VALUES("health-check-key");';
 
var db = openDatabaseWithSuffix('bug25710', '1.0', 'LayoutTest for bug 25710', 102400);
var backIterations;
var xhrFctIntervalId;
var backFctIntervalId;
var successCheckIntervalId;
var dbFctIntervalId;
var successes;
var databaseUpdates;
var stoppedIntervals;
 
function stopIntervals()
{
    stoppedIntervals = true;
    clearInterval(dbFctIntervalId);
    clearInterval(xhrFctIntervalId);
    clearInterval(backFctIntervalId);
}

function stopTest(message)
{
    if (!stoppedIntervals)
        stopIntervals();

    log(message);

    if (window.testRunner)
        testRunner.notifyDone();
}
    
function updateDatabase()
{
    databaseUpdates++;  
    db.transaction(function(transaction) {
        transaction.executeSql(UPDATE_DATA, [], function() {}, errorHandler);
    }, errorHandler, function() {
        successes++;
    });
}

function checkForSuccess()
{
    if (successes == databaseUpdates) {
        stopTest('Test Complete, SUCCESS');
        clearInterval(successCheckIntervalId);
    }
}
 
function errorHandler(tx, error)
{
    log('DB error, code: ' + error.code + ', msg: ' + error.message);
    stopTest('Test Complete, FAILED');
}
 
function sendXhr()
{
    xhr = new XMLHttpRequest();
    xhr.open('GET', location.href, true);
    xhr.send('');
}
 
function invokeBack()
{
    backIterations--;
    if (backIterations) {
        history.back();
    } else {
        stopIntervals();
        // Allow a little time for all the database transactions to complete now we've stopped making them.
        successCheckIntervalId = setInterval(checkForSuccess, 250);
        // If we don't finish before this time, then we consider the test failed.
        setTimeout(function() { stopTest('Timed out waiting for transactions to complete. FAILED'); }, 20000);
        
    }
}

function runTest()
{
    // Location changes need to happen outside the onload handler to generate history entries.
    setTimeout(runTestsInner, 0);
}

function runTestsInner()
{
    backIterations = 10;
    consecutiveFailures = 0;
    successes = 0;
    databaseUpdates = 0;
    stoppedIntervals = false;

    // Create some hashes so we can call history.back().
    log('Changing the hash to create history entries.');
    for (var i = 0; i < backIterations; i++) {
        setLocationHash(i);
    }

    // Init the database.
    db.transaction(function(transaction) {
        transaction.executeSql(CREATE_HEALTH_TABLE, [], function() {}, errorHandler);
    }, errorHandler, function() {
        // Give a little for the database to 'warm up' before making xhr requests
        // and calling history.back().
        setTimeout(function() {
            log('Db is warmed up');

            // NOTE: If we don't make any xhr requests, then the test
            // successfully passes (comment this line out).
            xhrFctIntervalId = setInterval(sendXhr, SEND_XHR_INTERVAL);
            backFctIntervalId = setInterval(invokeBack, BACK_INTERVAL);
            dbFctIntervalId = setInterval(updateDatabase, DB_UPDATE_INTERVAL);
        }, 500);
    });
}
