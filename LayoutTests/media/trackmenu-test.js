var captionsButtonCoordinates = null;

function clickCCButton()
{
    if (!captionsButtonCoordinates) {
        try {
            captionsButtonCoordinates = mediaControlsButtonCoordinates(video, "toggle-closed-captions-button");
        } catch (exception) {
            failTest(exception.description);
            return;
        }
    }

    eventSender.mouseMoveTo(captionsButtonCoordinates[0], captionsButtonCoordinates[1]);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function startTrackMenuTest(testFunction)
{
    internals.suspendAnimations();

    if (window.eventSender) {
        consoleWrite("<br>*** Set the user language preference.");
        run("internals.setUserPreferredLanguages(['en'])");

        clickCCButton();
        window.setTimeout(testFunction, 100);
    }
}

function getTrackListElement()
{
  var trackListElement;
  try {
      trackListElement = mediaControlsElement(internals.shadowRoot(video).firstChild, "-webkit-media-controls-closed-captions-container");
  } catch (exception) {
      failTest(exception.description);
      return null;
  }

  return trackListElement;
}

function trackMenuList()
{
    trackListElement = getTrackListElement();
    if (!trackListElement){
        failTest("Could not find the track list menu");
        return;
    }

    // Track list should have a <ul> with <li> children.
    var trackList = trackListElement.querySelector("ul");
    if (!trackList) {
        failTest("Could not find a child ul element in track list menu");
        return;
    }
    var trackListItems = trackList.querySelectorAll("li");
    if (!trackListItems) {
        failTest("Could not find child li elements in track list menu");
        return;
    }

    return trackListItems;
}

function indexOfMenuItemBeginningWith(title)
{
    var trackMenuItems = trackMenuList();
    for (i = 0; i < trackMenuItems.length; ++i) {
        if (trackMenuItems[i].textContent.indexOf(title) == 0)
            break;
    }
    return (i < trackMenuItems.length) ? i : -1;
}

function selectCaptionMenuItem(title)
{
    var trackMenuItems = trackMenuList();
    var index = indexOfMenuItemBeginningWith(title);
    if (index < 0) {
        failTest("Menu item " + title + " not found in track list menu.");
        return;
    }

    consoleWrite("- click on '" + title + "' menu item.");
    var selectedTrackItem = trackMenuItems[index];
    var boundingRect = selectedTrackItem.getBoundingClientRect();
    var x = boundingRect.left + boundingRect.width / 2;
    var y = boundingRect.top + boundingRect.height / 2;
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function showTrackMenu()
{
    clickCCButton();
}

function hideTrackMenu()
{
    if (!window.eventSender)
        return;

    eventSender.mouseMoveTo(1, 1);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function listTrackMenu()
{
    var trackListItems = trackMenuList();
    consoleWrite("Track menu:");
    for (i = 0; i < trackListItems.length; i++) {
        var logString = i + ": \"" + trackListItems[i].textContent + "\"";
        if (trackListItems[i].className == "selected")
            logString += ", checked";
        consoleWrite(logString);
    }
}
