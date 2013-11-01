function testTrackListContainsTrack(listStr, trackStr)
{
    var list = eval(listStr);
    var track = eval(trackStr);
    var found = false;
    for (var i = 0; i < list.length; ++i) {
        if (list[i] == track) {
            found = true;
            break;
        }
    }
    reportExpected(found, listStr, " contains ", trackStr, list);
}

function compareTracks(track1, track2)
{
    var equal = (eval(track1) == eval(track2));
    reportExpected(equal, track1, "==", track2, track1);
}

function testAddTrack(uri, type)
{
    var addtrackEventCount = 0;

    function trackAdded(event)
    {
        consoleWrite("EVENT(" + event.type + ")");
        ++addtrackEventCount;

        /* Don't make assumptions about the order of track events. If you know
         * the expected order, it should be tested separately. */
        testTrackListContainsTrack("video." + type + "Tracks", "event.track");
        consoleWrite("");
    }

    findMediaElement();
    var tracks = eval("video." + type + "Tracks");
    tracks.addEventListener("addtrack", trackAdded);
    video.src = uri;
    waitForEventAndEnd('canplaythrough');
}

function testAttribute(uri, type, attribute, values)
{
    function canplaythrough()
    {
        consoleWrite("<br><i>** Check in-band kind attributes</i>");
        testExpected("video." + type +"Tracks.length", values.length);
        for (var i = 0; i < values.length; ++i)
            testExpected("video." + type +"Tracks[" + i + "]." + attribute, values[i]);

        consoleWrite("");
        endTest();
    }

    findMediaElement();
    video.src = uri;
    waitForEvent('canplaythrough', canplaythrough);
}

function testCuesAddedOnce(uri, kind)
{
    var seekedCount = 0;
    var cuesStarts = [];

    function pollProgress()
    {
        if (video.currentTime < 2)
            return;

        testExpected("inbandTrack1.cues.length", 0, ">");

        if (!seekedCount) {
            // Collect the start times of all cues, seek back to the\beginning and play
            // the same segment of the video file.
            run("video.pause()");
            for (var i = 0; i < inbandTrack1.cues.length; ++i)
                cuesStarts.push(inbandTrack1.cues[i].startTime);
            run("video.currentTime = 0");
            run("video.play()");
            consoleWrite("");
            return;
        }

        run("video.pause()");
        try {
            var success = true;
            for (var i = 0; i < cuesStarts.length; ++i) {
                if (inbandTrack1.cues[i].startTime != cuesStarts[i]) {
                    // Since we don't know the exact number of cues, only print
                    // output if the test fails.
                    testExpected("inbandTrack1.cues[" + i + "].startTime", cuesStarts[i]);
                    success = false;
                }
            }
            logResult(success, "Test all cues are equal");
            endTest();
        } catch (e) {
            failTest(e);
        }
    }

    function canplaythrough()
    {
        waitForEvent('seeked', function() { ++seekedCount; });
        setInterval(pollProgress, 100);

        consoleWrite("<br><i>** Setting track 1 to showing</i>");
        for (var i = 0; i < video.textTracks.length; ++i) {
            if (video.textTracks[i].kind == kind) {
                inbandTrack1 = video.textTracks[i];
                break;
            }
        }
        run("inbandTrack1.mode = 'showing'");
        run("video.play()");
    }

    findMediaElement();
    video.src = uri;
    waitForEvent('canplaythrough', canplaythrough);
}

function testTextTrackMode(uri, kind)
{
    function seeked()
    {
        testExpected("textTrackDisplayElement(video, 'cue')", null);

        consoleWrite("<br><i>** Showing a track should show active cues immediately<" + "/i>");
        run("inbandTrack1.mode = 'showing'");

        testExpected("textTrackDisplayElement(video, 'cue').textContent", null, '!=');
        testExpected("inbandTrack1.activeCues.length", 1);

        consoleWrite("");
        endTest();
    }

    function canplaythrough()
    {
        for (var i = 0; i < video.textTracks.length; ++i) {
            if (video.textTracks[i].kind == kind) {
                inbandTrack1 = video.textTracks[i];
                break;
            }
        }

        consoleWrite("<br><i>** A hidden track should not have visible cues<" + "/i>");
        run("inbandTrack1.mode = 'hidden'");
        testExpected("inbandTrack1.activeCues.length", 0);

        run("video.play()");
        setTimeout(function() { video.pause(); video.currentTime = 0.5; }, 500);
    }

    findMediaElement();
    video.src = uri;
    waitForEvent('seeked', seeked);
    waitForEvent('canplaythrough', canplaythrough);
}

function testCueStyle(uri)
{
    function seeked()
    {
        consoleWrite("<br><i>** Test current cue colors</i>");

        run("cueDisplayElement = textTrackDisplayElement(video, 'display', 0)");
        testExpected("getComputedStyle(cueDisplayElement).color", "rgb(255, 255, 255)");

        run("cueNode = textTrackDisplayElement(video, 'cue')");
        testExpected("getComputedStyle(cueNode).backgroundColor", "rgba(0, 0, 0, 0.8)");

        endTest();
    }

    function canplaythrough()
    {
        consoleWrite("<br><i>** Setting track 1 to showing and starting video</i>");
        for (var i = 0; i < video.textTracks.length; ++i) {
            if (video.textTracks[i].kind == "subtitles" || video.textTracks[i].kind == "captions") {
                inbandTrack1 = video.textTracks[i];
                break;
            }
        }

        inbandTrack1.mode = 'showing';
        run("video.play()");
        setTimeout(function() { video.pause(); video.currentTime = 0.3; }, 1500);
    }

    consoleWrite("Test that style is applied to all cues correctly.");
    findMediaElement();
    video.src = uri;
    waitForEvent('seeked', seeked);
    waitForEvent('canplaythrough', canplaythrough);
}

function testTextTrackOrder(uri, numInBandTracks)
{
    function compareTracks(track1, track2)
    {
        var equal = (eval(track1) == eval(track2));
        reportExpected(equal, track1, "==", track2, track1);
    }

    function canplaythrough()
    {
        consoleWrite("<br><i>** Check initial in-band track states</i>");
        testExpected("video.textTracks.length", numInBandTracks);
        for (var i = 0; i < numInBandTracks; ++i)
          run("inbandTrack" + (i + 1) + " = video.textTracks[" + i + "]");

        consoleWrite("<br><i>** Add two tracks, check sort order<" + "/i>");
        run("addTrack = video.addTextTrack('captions', 'Caption Track', 'en')");
        run("trackElement = document.createElement('track')");
        trackElement.label = '<track>';
        run("video.appendChild(trackElement)");
        testExpected("video.textTracks.length", numInBandTracks + 2);

        compareTracks("video.textTracks[0]", "trackElement.track");
        compareTracks("video.textTracks[1]", "addTrack");
        for (var i = 1; i < numInBandTracks + 1; ++i)
          compareTracks("video.textTracks[" + (i + 1) + "]", "inbandTrack" + i);

        consoleWrite("<br><i>** Unload video file, check track count<" + "/i>");
        run("video.src = ''");
        testExpected("video.textTracks.length", 2);

        consoleWrite("");
        endTest();
    }

    findMediaElement();
    video.src = uri;
    waitForEvent('canplaythrough', canplaythrough);
}
