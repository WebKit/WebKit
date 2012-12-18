var captionsButtonCoordinates;

function clickCCButton()
{
    eventSender.mouseMoveTo(captionsButtonCoordinates[0], captionsButtonCoordinates[1]);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function startTrackMenuTest(testFunction)
{
    if (window.eventSender) {
        consoleWrite("<br>*** Set the user language preference.");
        run("internals.setUserPreferredLanguages(['en'])");

        try {
            captionsButtonCoordinates = mediaControlsButtonCoordinates(video, "toggle-closed-captions-button");
        } catch (exception) {
            failTest(exception.description);
            return;
        }
        clickCCButton();
        window.setTimeout(testFunction, 100);
    }
}

function getTrackListElement()
{
  var trackListElement;
  try {
      trackListElement = mediaControlsElement(internals.shadowRoot(video).firstChild, "-webkit-media-controls-closed-captions-track-list");
  } catch (exception) {
      failTest(exception.description);
      return null;
  }
  return trackListElement;
}
