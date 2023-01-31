
function buttonClickHandler()
{
    var movie = movieInfo.movies[movieInfo.current];

    consoleWrite("EVENT(mouseup)");
    
    consoleWrite("* event handler triggered by user gesture");

    // Try to enter fullscreen in response to a mouse click

    if (movie.supportsFS)
        run("mediaElement.webkitEnterFullScreen()");
    else {
        if (movie.type == 'video')
            testDOMException("mediaElement.webkitEnterFullScreen()", "DOMException.INVALID_STATE_ERR");

        // A user gesture will transfer across setTimeout for 1 second, so pause to let that 
        // expire before opening the next movie.
        setTimeout(openNextMovie, 1010);
    }
}

function clickEnterFullscreenButton()
{
    consoleWrite("* clicking on button");
    var button = document.getElementById('button');
    eventSender.mouseMoveTo(button.offsetLeft + 20, button.offsetTop + 7);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function fullscreenchange()
{
    if (document.webkitIsFullScreen)
        beginfullscreen(true);
    else
        endfullscreen();
}

async function beginfullscreen(wasTriggeredFromFullscreenChangeEvent)
{
    if (window.internals)
        await testExpectedEventually("internals.isChangingPresentationMode(mediaElement)", false);
    if (!wasTriggeredFromFullscreenChangeEvent)
        run("mediaElement.webkitExitFullScreen()");
    else {
        // Call asynchronously to give time to the WebCore FullscreenManager to notify the video
        // element that it entered full-screen, and thus allowing the exitFullScreen() call to go
        // through.
        setTimeout('run("mediaElement.webkitExitFullScreen()")', 0);
    }
}

function endfullscreen()
{
    setTimeout(openNextMovie, 10);
}

async function fullscreenerror()
{
    var movie = movieInfo.movies[movieInfo.current];
    if (movie.inline) {
        failTest("Unexpected fullscreenerror event");
    } else {
        await testExpectedEventually("mediaElement.webkitDisplayingFullscreen", false);
        openNextMovie();
    }
}

function canplaythrough()
{
    var movie = movieInfo.movies[movieInfo.current];

    consoleWrite("* event handler NOT triggered by a user gesture");
    if (movie.type == 'video') {
        testExpected("mediaElement.webkitSupportsFullscreen", movie.supportsFS);
        if (mediaElement.webkitSupportsPresentationMode)
            testExpected("mediaElement.webkitSupportsPresentationMode('fullscreen')", movie.supportsFS);
        testExpected("mediaElement.webkitDisplayingFullscreen", false);
    } else {
        testExpected("mediaElement.webkitSupportsFullscreen", undefined);
        testExpected("mediaElement.webkitSupportsPresentationMode", undefined);
        testExpected("mediaElement.webkitDisplayingFullscreen", undefined);
    }

    // Click on the button
    runWithKeyDown(clickEnterFullscreenButton);
}

function openNextMovie()
{
    consoleWrite("");

    movieInfo.current++;
    if (movieInfo.current >= movieInfo.movies.length) {
        endTest();
        return;
    }

    var movie = movieInfo.movies[movieInfo.current];
    var url = movie.url;
    var container = document.getElementById('parent');

    // Remove the current media element, if any
    if (container.firstChild)
        container.removeChild(container.firstChild);

    var desc = "*** Creating &lt;" + movie.type  + "&gt; element with <em>\"" + url + "\"</em> "
                + (!movie.inline ? "not " : "") + "in the document, should " 
                + (!movie.supportsFS ? "<b>NOT</b> " : "") + "support fullscreen " + movie.description;
    consoleWrite(desc);

    // Create a new element, maybe insert it into the DOM
    mediaElement = document.createElement(movie.type);
    if (movie.inline)
        mediaElement = container.appendChild(mediaElement);
    addEventListeners();
    mediaElement.setAttribute('controls', 'controls'); 
    mediaElement.setAttribute('src', url); 

    if (!movie.inline)
        mediaElement.load();
}

function addEventListeners(elem)
{
    waitForEvent("error");
    waitForEvent("loadstart");
    waitForEvent("waiting");
    waitForEvent("ratechange");
    waitForEvent("durationchange");
    waitForEvent("pause");
    waitForEvent("play");
    waitForEvent("playing");

    waitForEvent('canplaythrough', canplaythrough);

    waitForEvent('webkitbeginfullscreen', beginfullscreen);
    waitForEvent('webkitendfullscreen', endfullscreen);
    waitForEvent('webkitfullscreenchange', fullscreenchange);
    waitForEvent('webkitfullscreenerror', fullscreenerror);
}
