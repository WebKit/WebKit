var expectedStartTimes = new Array();
var expectedEndTimes = new Array();
var timeRangeCount = 0;
var currentTimeRange = 0;
var currentTest = 0;
var willPauseInExistingRange = false;
var willExtendAnExistingRange = false;

function testRanges()
{
    testExpected("video.played.length", timeRangeCount);
    
    for (i = 0; i < timeRangeCount; i++) {
        testExpected("video.played.start(" + (i) + ").toFixed(2)", expectedStartTimes[i]);
        testExpected("video.played.end("   + (i) + ").toFixed(2)", expectedEndTimes[i]);
    }
}

function nextTest()
{
    if (currentTest >= testFunctions.length)
        endTest();
    else
        (testFunctions[currentTest])();
    currentTest++;
}

function pause(evt)
{
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
    playForMillisecs(250); // Triggers pause()
}

function secToMilli(seconds)
{
    return seconds * 1000.;
}

function milliToSecs(milliseconds)
{
    return milliseconds / 1000;
}

function playForMillisecs(milliseconds)
{
    if (milliToSecs(milliseconds) > video.duration) {
        consoleWrite("WARNING: playForMillisecs() does not support range ("+milliToSecs(milliseconds)+") bigger than video duration ("+video.duration+") (yet)");
        return;
    }
    var playedFromTime = video.currentTime;
    run("video.play()");
    var callCount = 0;
    var callPauseIfTimeIsReached = function ()
    {
        var playedTime = video.currentTime - playedFromTime;
        
        if (playedTime < 0) {
            // Deal with 'loop' attribue. This work only once, hence the warning
            // At the begining of playForMillisecs()
            playedTime = video.duration - playedFromTime + video.currentTime;
        }
        
        if (callCount++ > 10) {
            // Just in case something goes wrong.
            consoleWrite("WARNING: Call count exceeded");
            return;
        }
        
        if (playedTime >= milliToSecs(milliseconds) || video.currentTime == video.duration) {
            run("video.pause()");
        } else {
            var waitingTime = milliseconds - playedTime  * 1000;
            setTimeout(callPauseIfTimeIsReached, waitingTime);
        }
    }
    setTimeout(callPauseIfTimeIsReached, milliseconds);
}

function videoPlayedMain()
{
    findMediaElement();
    
    video.src = 'content/test.mp4';
    
    waitForEvent("error");
    waitForEvent("loadstart");
    waitForEvent("ratechange");
    waitForEvent("loadedmetadata");
    waitForEvent("canplay", canplay); // Will trigger nextTest() which launches the tests.
    waitForEvent("pause", pause);
    
    video.load();
}
