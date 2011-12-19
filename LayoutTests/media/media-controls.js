
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

function textTrackDisplayElement(parentElement)
{
    var controlID = "-webkit-media-text-track-display";
    var displayElement = mediaControlsElement(internals.shadowRoot(parentElement).firstChild, controlID);
    if (!displayElement)
        throw "Failed to find media control element ID '" + controlID + "'";
    return displayElement;
}