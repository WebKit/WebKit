
function mediaControlsElement(first, id)
{
    var controlID = "-webkit-media-controls-" + id;
    for (var element = first; element; element = element.nextSibling) {

        // Not every element in the media controls has a shadow pseudo ID, eg. the
        // text nodes for the time values, so guard against exceptions.
        try {
            if (internals.shadowPseudoId(element) == controlID)
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
    var button = mediaControlsElement(internals.shadowRoot(element).firstChild, id);
    if (!button)
        throw "Failed to find media control element ID '" + id + "'";

    var buttonBoundingRect = button.getBoundingClientRect();
    var x = buttonBoundingRect.left + buttonBoundingRect.width / 2;
    var y = buttonBoundingRect.top + buttonBoundingRect.height / 2;
    return new Array(x, y);
}
