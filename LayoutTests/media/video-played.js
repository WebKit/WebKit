
var expectedStartTimes = new Array();
var expectedEndTimes = new Array();
var timeRangeCount = 0;
var currentTimeRange = 0;
var currentTest = 0;
var willPauseInExistingRange = false;
var willExtendAnExistingRange = false;

var testStartTime = 0;
var logTestTiming = false;

//@@@@@ Uncomment the following line to log the time each "video-played" sub-test takes in test output
//@@@@@ logTestTiming = true;

function logRanges()
{
    consoleWrite("");
    for (i = 0; i < timeRangeCount; i++) {
        consoleWrite("****  range " + i + " ( " + video.played.start(i).toFixed(2) + ".." + video.played.end(i).toFixed(2) + ")");
    }
}

function testRanges()
{
    if (testStartTime) {
        logRanges();

        var duration = (new Date().getTime() - testStartTime) / 1000;
        consoleWrite("**** Test " + currentTest + " took " + duration.toFixed(2) + " seconds");
    }

    testExpected("video.played.length", timeRangeCount);
    
    // We skip played range at index 0 as it's the value of currentTime following a call to play() immediately followed by pause().
    // The test assume that currentTime would have progressed by less than 0.01s. Experimentation shows that it could be greater than this value.
    // Using 0.5s for now.
    for (i = 0; i < timeRangeCount; i++) {
        testExpected("video.played.start(" + (i) + ").toFixed(2)", expectedStartTimes[i]);
        testExpected("video.played.end("   + (i) + ").toFixed(2) - expectedEndTimes[i] <= 0.5", true);
    }
}

function nextTest()
{
    if (logTestTiming)
        testStartTime = new Date().getTime();

    if (currentTest >= testFunctions.length)
        endTest();
    else
        (testFunctions[currentTest])();
    currentTest++;
}

function pause(evt)
{
    if (video.ended)
        return;
    currentTime = video.currentTime.toFixed(2);
    
    if (!willExtendAnExistingRange)
        expectedEndTimes.splice(currentTimeRange, 0, currentTime)
        else if(expectedEndTimes[currentTimeRange] < currentTime || expectedEndTimes[currentTimeRange] == undefined)
            expectedEndTimes[currentTimeRange] = currentTime;
    
    testRanges();
    nextTest();
}

function canplay(event) 
{
    testRanges();
    nextTest();
}

function ended(evt)
{
    testExpected('video.currentTime == video.duration', true);
    testExpected('video.played.length >= 1', true);
    testExpected('video.played.end(video.played.length -1) == video.duration', true);

    nextTest();
}

function willCreateNewRange()
{
    expectedStartTimes.splice(currentTimeRange, 0, video.currentTime.toFixed(2))
    ++timeRangeCount;
}

function startPlayingInNewRange()
{
    willCreateNewRange();
    startPlaying();
}

function startPlaying()
{
    playForMillisecs(100); // Triggers pause()
}

function secToMilli(seconds)
{
    return seconds * 1000.;
}

function milliToSecs(milliseconds)
{
    return milliseconds / 1000;
}

function nowInSecs()
{
    return milliToSecs((new Date()).getTime());
}

function playForMillisecs(milliseconds)
{
    if (milliToSecs(milliseconds) > video.duration) {
        failTest("WARNING: playForMillisecs() does not support range ("+milliToSecs(milliseconds)+") bigger than video duration ("+video.duration+") (yet)");
        return;
    }

    run("handlePromise(video.play())");

    var startTime = nowInSecs();
    var playedFromTime = video.currentTime;
    var playDuration = milliToSecs(milliseconds);
    var callPauseIfTimeIsReached = function ()
    {
        var playedTime = video.currentTime - playedFromTime;
        
        if (playedTime < 0) {
            // Deal with 'loop' attribue. This work only once, hence the warning
            // At the begining of playForMillisecs()
            playedTime = video.duration - playedFromTime + video.currentTime;
        }

        var elapsed = nowInSecs() - startTime;
        if (elapsed > 2 + playDuration) {
            // Just in case something goes wrong.
            failTest("ERROR: test stalled, waited " + elapsed + " seconds for movie to play " + playedTime + " seconds");
            return;
        }
        
        if (playedTime >= playDuration || video.currentTime == video.duration)
            run("video.pause()");
        else {
            var delta = milliseconds - playedTime  * 1000;
            setTimeout(callPauseIfTimeIsReached, delta);
        }
    }

    // Add a small amount to the timer because it will take a non-zero amount of time for the 
    // video to start playing.
    setTimeout(callPauseIfTimeIsReached, milliseconds + 100);
}

async function playUntilEnded()
{
    run("handlePromise(video.play())");

    await waitFor(video, 'ended');
}

function videoPlayedMain()
{
    findMediaElement();
    
    video.src = findMediaFile("video", "content/test");
    
    waitForEvent("error");
    waitForEvent("loadstart");
    waitForEvent("ratechange");
    waitForEvent("loadedmetadata");
    waitForEvent("canplay", canplay); // Will trigger nextTest() which launches the tests.
    waitForEvent("pause", pause);
    waitForEvent("ended", ended);
    
    video.load();
}
