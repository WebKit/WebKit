
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
            testException("mediaElement.webkitEnterFullScreen()", "DOMException.INVALID_STATE_ERR");
        openNextMovie();
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
        beginfullscreen();
    else
        endfullscreen();
}

function beginfullscreen()
{
    testExpected("mediaElement.webkitDisplayingFullscreen", true);
    run("mediaElement.webkitExitFullScreen()");
}

function endfullscreen()
{
    setTimeout(openNextMovie, 10);
}

function canplaythrough()
{
    var movie = movieInfo.movies[movieInfo.current];

    consoleWrite("* event handler NOT triggered by a user gesture");

    if (movie.type == 'video') {
        testExpected("mediaElement.webkitSupportsFullscreen", movie.supportsFS);
        testExpected("mediaElement.webkitDisplayingFullscreen", false);
    } else {
        testExpected("mediaElement.webkitSupportsFullscreen", undefined);
        testExpected("mediaElement.webkitDisplayingFullscreen", undefined);
    }
    
    // Verify that we get an exception when trying to enter fullscreen since this isn't
    // called in response to a user gesture.
    if (movie.type == 'video')
        testException("mediaElement.webkitEnterFullScreen()", "DOMException.INVALID_STATE_ERR");

    // Click on the button
    if (window.layoutTestController)
        setTimeout(clickEnterFullscreenButton, 10);
    else
        openNextMovie();
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
}

