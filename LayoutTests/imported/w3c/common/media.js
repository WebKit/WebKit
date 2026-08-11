//
// Returns whether the 'relative' parameter, resolved to an absolute URI,
// is a match for the 'absolute' parameter
//
function isResolvedURI(absolute, relative)
{
    // This regex matches if the target starts with the current protocol,
    // contains the current host, and ends with 'relative'
    var regex = new RegExp('^' + location.protocol + '\/\/' + '.*' +
        location.hostname + '.*' + relative + '$', 'i');
    
    return absolute.match(regex);
}

//
// Returns whether the given variable is a valid value for a 'preload' property
//
function isValidPreloadValue(preload)
{
    preload = preload.toLowerCase();
	return preload == "" || preload == "none" || preload == "metadata" ||
		preload == "auto";
}

//
// Returns the URI of a supported video source based on the user agent
//
function getVideoURI(base)
{
    var extension = '.mp4';

    var videotag = document.createElement("video");

    if ( videotag.canPlayType  &&
         videotag.canPlayType('video/ogg; codecs="theora, vorbis"') )
    {
        extension = '.ogv';
    }

    return base + extension;
}

//
// Returns the URI of a supported audio source based on the user agent
//
function getAudioURI(base)
{
    var extension = '.mp3';

    var audiotag = document.createElement("audio");

    if ( audiotag.canPlayType &&
         audiotag.canPlayType('audio/ogg') )
    {
        extension = '.oga';
    }

    return base + extension;
}

//
// Returns the mime type (and codecs parameter) of a supported video source
// based on the user agent
//
function getVideoType(codecs)
{
    var mime = 'video/mp4';
    var codecs_param = 'avc1.42E01E, mp4a.40.2';

    var videotag = document.createElement("video");
    
    if ( videotag.canPlayType  &&
         videotag.canPlayType('video/ogg; codecs="theora, vorbis"') )
    {
        mime = 'video/ogg';
        codecs_param = 'theora, vorbis';
    }

    if ( codecs )
    {
        return mime + '; codecs="' + codecs_param + '"';
    }
    else
    {
        return mime;
    }
}

var result_timeout = 0;

//
// Clear the result timeout so that the test will not automatically pass/fail
//
function clearResultTimeout()
{
    if (result_timeout != 0)
    {
        window.clearTimeout(result_timeout);
        result_timeout = 0;
    }
}

//
// Passes the test
//
function passTest()
{
    document.getElementById("test_0_result").innerHTML = "PASS";
    clearResultTimeout();
}

//
// Fails the test
//
function failTest()
{
    document.getElementById("test_0_result").innerHTML = "FAIL";
    clearResultTimeout();
}

//
// Signals that the pass condition in this test should be manually verified
//
function manualTest()
{
    document.getElementById("test_0_result").innerHTML = "Manual";
    clearResultTimeout();
}

//
// Set the test to automatically fail after a timeout is reached
//
function setFailTimeout(milliseconds)
{
    clearResultTimeout();
    result_timeout = window.setTimeout("failTest();", milliseconds);
}

//
// Set the test to automatically pass after a timeout is reached
//
function setPassTimeout(milliseconds)
{
    clearResultTimeout();
    result_timeout = window.setTimeout("passTest();", milliseconds);
}

//
// Find an unbuffered time in the media resource
//
function findUnbufferedTime(media)
{
    var max = 0;
        
    for (var i=0; i < media.buffered.length; i++)
    {
        if (media.buffered.end(i) > max)
        {
            max = media.buffered.end(i);
        }
    }

    if (max < media.duration)
    {
        return (max + media.duration) / 2;
    }
    else
    {
        return -1;
    }
}

//
// Returns whether the actual time is approximately the expected time (within a
// tolerance)
//
function isApprox(actual, expected, tolerance)
{
    return ((expected - tolerance) < actual) && (actual < (expected + tolerance));
}

var checkPlaybackRate_media = null;
var checkPlaybackRate_tolerance = -1;
var checkPlaybackRate_callback = function(result){};
var checkPlaybackRate_time = -1;
var checkPlaybackRate_timeout = 0;
var checkPlaybackRate_seconds = -1;
var checkPlaybackRate_playbackRate = 0;

function checkPlaybackRate_check()
{
    var rate = (checkPlaybackRate_media.currentTime - checkPlaybackRate_time) / checkPlaybackRate_seconds;

    checkPlaybackRate_callback(
        isApprox(
            rate, 
            checkPlaybackRate_playbackRate, 
            checkPlaybackRate_tolerance
            )
        );
}

//
// Checks that the playback rate is a given value by estimating the actual rate
// over time
//
function checkPlaybackRate(media, playbackRate, seconds, tolerance, callback)
{
    if (checkPlaybackRate_timeout != 0)
    {
        window.clearTimeout(checkPlaybackRate_timeout);
        checkPlaybackRate_timeout = 0;
    }

    checkPlaybackRate_media = media;
    checkPlaybackRate_playbackRate = playbackRate;
    checkPlaybackRate_seconds = seconds;
    checkPlaybackRate_tolerance = tolerance;
    checkPlaybackRate_callback = callback;
    checkPlaybackRate_time = media.currentTime;
    checkPlaybackRate_timeout = window.setTimeout("checkPlaybackRate_check();", 1000 * seconds);
}

//
// Returns whether the given time has been buffered
//
function isTimeBuffered(media, time)
{
    for (var i=0; i < media.buffered.length; i++)
    {
        if (media.buffered.start(i) <= time && time <= media.buffered.end(i))
        {
            return true;
        }
    }

    return false;
}

var waitForTimeBuffered_media = null;
var waitForTimeBuffered_time = -1;
var waitForTimeBuffered_callback = function(){};
var waitForTimeBuffered_interval = 0;

function waitForTimeBuffered_check()
{
    if (isTimeBuffered(waitForTimeBuffered_media, waitForTimeBuffered_time))
    {
        window.clearInterval(waitForTimeBuffered_interval);
        waitForTimeBuffered_interval = 0;
        waitForTimeBuffered_callback();
    }
}

//
// Waits for a time to be buffered by polling
//
function waitForTimeBuffered(media, time, callback)
{
    waitForTimeBuffered_media = media;
    waitForTimeBuffered_time = time;
    waitForTimeBuffered_callback = callback;

    if (waitForTimeBuffered_interval != 0)
    {
        window.clearInterval(waitForTimeBuffered_interval);
        waitForTimeBuffered_interval = 0;
    }

    waitForTimeBuffered_interval = setInterval("waitForTimeBuffered_check();", 250);
}

//
// Makes sure that the media element is audible.  This is necessary because
// user agents may remember volume settings for media elements, on a per-domain
// basis or otherwise
//
function ensureAudible(media)
{
    media.muted = false;
    media.volume = 1.0;
}
