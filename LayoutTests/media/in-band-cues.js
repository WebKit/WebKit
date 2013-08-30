function testAttribute(uri, attribute, values)
{
    function canplaythrough()
    {
        consoleWrite("<br><i>** Check in-band kind attributes</i>");
        testExpected("video.textTracks.length", values.length);
        for (var i = 0; i < values.length; ++i) {
            testExpected("video.textTracks[" + i + "]." + attribute, values[i]);
        }

        consoleWrite("");
        endTest();
    }

    findMediaElement();
    video.src = uri;
    waitForEvent('canplaythrough', canplaythrough);
}

function testCuesAddedOnce(uri)
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
        run("inbandTrack1 = video.textTracks[0]");
        run("inbandTrack1.mode = 'showing'");
        run("video.play()");
    }

    findMediaElement();
    video.src = uri;
    waitForEvent('canplaythrough', canplaythrough);
}

function testMode(uri)
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
        run("inbandTrack1 = video.textTracks[0]");

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

function testStyle(uri)
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
        run("inbandTrack1 = video.textTracks[0]");

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

function testTrackOrder(uri) {
    var addtrackEventCount = 0;

    function trackAdded(event)
    {
        consoleWrite("EVENT(" + event.type + ")");
        compareTracks("event.track", "video.textTracks[" + addtrackEventCount + "]");
        ++addtrackEventCount;
        consoleWrite("");
    }

    function compareTracks(track1, track2)
    {
        var equal = (eval(track1) == eval(track2));
        reportExpected(equal, track1, "==", track2, track1);
    }

    function canplaythrough()
    {
        consoleWrite("<br><i>** Check initial in-band track states</i>");
        testExpected("video.textTracks.length", 2);
        run("inbandTrack1 = video.textTracks[0]");
        run("inbandTrack2 = video.textTracks[1]");

        consoleWrite("<br><i>** Add two tracks, check sort order<" + "/i>");
        run("addTrack = video.addTextTrack('captions', 'Caption Track', 'en')");
        run("trackElement = document.createElement('track')");
        trackElement.label = '<track>';
        run("video.appendChild(trackElement)");
        testExpected("video.textTracks.length", 4);

        compareTracks("video.textTracks[0]", "trackElement.track");
        compareTracks("video.textTracks[1]", "addTrack");
        compareTracks("video.textTracks[2]", "inbandTrack1");
        compareTracks("video.textTracks[3]", "inbandTrack2");

        consoleWrite("<br><i>** Unload video file, check track count<" + "/i>");
        run("video.src = ''");
        testExpected("video.textTracks.length", 2);

        consoleWrite("");
        endTest();
    }

    findMediaElement();
    video.textTracks.addEventListener("addtrack", trackAdded);
    video.src = uri;
    waitForEvent('canplaythrough', canplaythrough);
}
