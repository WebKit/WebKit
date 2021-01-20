var captionsButtonCoordinates = null;

function clickCCButton()
{
    if (!video || (video.nodeName != "VIDEO" && video.nodeName != "AUDIO")) {
        consoleWrite(`<br>*** ERROR: 'video' global = "${video} in clickCCButton`);
        endTest();
    }

    if (!captionsButtonCoordinates) {
        try {
            captionsButtonCoordinates = mediaControlsButtonCoordinates(video, "toggle-closed-captions-button");
        } catch (exception) {
            failTest(`clickCCButton failed with exception: "${exception.description}:`);
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

function trackMenuListByLabel(label)
{
    trackListElement = getTrackListElement();
    if (!trackListElement){
        failTest("Could not find the track list menu");
        return;
    }

    var trackLists = trackListElement.querySelectorAll("ul");
    var trackList = Array.prototype.find.call(trackLists, function(item) {
        return item.getAttribute('aria-labelledby') === label;
    });


    // Track list should have a <ul> with <li> children.
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

function captionTrackMenuList()
{
    return trackMenuListByLabel('webkitMediaControlsClosedCaptionsHeading');
}

function audioTrackMenuList()
{
    return trackMenuListByLabel('webkitMediaControlsAudioTrackHeading');
}

function indexOfMenuItemBeginningWith(title, trackMenuItems)
{
    for (i = 0; i < trackMenuItems.length; ++i) {
        if (trackMenuItems[i].textContent.indexOf(title) == 0)
            break;
    }
    return (i < trackMenuItems.length) ? i : -1;
}

function indexOfCaptionMenuItemBeginningWith(title)
{
    return indexOfMenuItemBeginningWith(title, captionTrackMenuList());
}

function indexOfAudioTrackMenuItemBeginningWith(title)
{
    return indexOfMenuItemBeginningWith(title, audioTrackMenuList());
}

function selectMenuItemFromList(title, trackMenuItems, index)
{
    consoleWrite("- click on '" + title + "' menu item.");
    var selectedTrackItem = trackMenuItems[index];
    var boundingRect = selectedTrackItem.getBoundingClientRect();
    var x = boundingRect.left + boundingRect.width / 2;
    var y = boundingRect.top + boundingRect.height / 2;
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function selectCaptionMenuItem(title)
{
    var index = indexOfCaptionMenuItemBeginningWith(title);
    if (index < 0) {
        failTest("Menu item " + title + " not found in track list menu.");
        return;
    }

    selectMenuItemFromList(title, captionTrackMenuList(), index);
}

function selectAudioTrackMenuItem(title)
{
    var index = indexOfAudioTrackMenuItemBeginningWith(title);
    if (index < 0) {
        failTest("Menu item " + title + " not found in track list menu.");
        return;
    }

    selectMenuItemFromList(title, audioTrackMenuList(), index);
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
    var trackListItems = captionTrackMenuList();
    consoleWrite("Track menu:");
    for (i = 0; i < trackListItems.length; i++) {
        var logString = i + ": \"" + trackListItems[i].textContent + "\"";
        if (trackListItems[i].className == "selected")
            logString += ", checked";
        consoleWrite(logString);
    }
}
