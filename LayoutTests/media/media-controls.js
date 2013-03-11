
function mediaControlsElement(first, id)
{
    for (var element = first; element; element = element.nextSibling) {
        // Not every element in the media controls has a shadow pseudo ID, eg. the
        // text nodes for the time values, so guard against exceptions.
        try {
            if (internals.shadowPseudoId(element) == id)
                return element;
        } catch (exception) { }

        if (element.firstChild) {
            var childElement = mediaControlsElement(element.firstChild, id);
            if (childElement)
                return childElement;
        }
    }

    return null;
}

function mediaControlsButtonCoordinates(element, id)
{
    var controlID = "-webkit-media-controls-" + id;
    var button = mediaControlsElement(internals.shadowRoot(element).firstChild, controlID);
    if (!button)
        throw "Failed to find media control element ID '" + id + "'";

    var buttonBoundingRect = button.getBoundingClientRect();
    var x = buttonBoundingRect.left + buttonBoundingRect.width / 2;
    var y = buttonBoundingRect.top + buttonBoundingRect.height / 2;
    return new Array(x, y);
}

function mediaControlsButtonDimensions(element, id)
{
    var controlID = "-webkit-media-controls-" + id;
    var button = mediaControlsElement(internals.shadowRoot(element).firstChild, controlID);
    if (!button)
        throw "Failed to find media control element ID '" + id + "'";

    var buttonBoundingRect = button.getBoundingClientRect();
    return new Array(buttonBoundingRect.width, buttonBoundingRect.height);
}

function textTrackDisplayElement(parentElement, id, cueNumber)
{
    var textTrackContainerID = "-webkit-media-text-track-container";
    var containerElement = mediaControlsElement(internals.shadowRoot(parentElement).firstChild, "-webkit-media-text-track-container");

    if (!containerElement)
        throw "Failed to find text track container element";

    if (!id)
        return containerElement;

    if (arguments[1] != 'cue')
        var controlID = "-webkit-media-text-track-" + arguments[1];
    else
        var controlID = arguments[1];

    var displayElement = mediaControlsElement(containerElement.firstChild, controlID);
    if (!displayElement)
        throw "No text track cue with display id '" + controlID + "' is currently visible";

    if (cueNumber) {
        for (i = 0; i < cueNumber; i++)
            displayElement = displayElement.nextSibling;

        if (!displayElement)
            throw "There are not " + cueNumber + " text track cues visible";
    }

    return displayElement;
}
