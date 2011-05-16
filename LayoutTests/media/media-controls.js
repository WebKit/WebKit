
function mediaControlsButtonCoordinates(element, id)
{
    var button;
    var controlsShadow = layoutTestController.shadowRoot(element).firstChild.firstChild;
    for (child = controlsShadow.firstChild; child; child = child.nextSibling) {
        if (layoutTestController.shadowPseudoId(child) == "-webkit-media-controls-" + id) {
            button = child;
            break;
        }
    }

    if (!button)
        throw "Failed to find button " + id;

    var buttonBoundingRect = button.getBoundingClientRect();
    var x = buttonBoundingRect.left + buttonBoundingRect.width / 2;
    var y = buttonBoundingRect.top + buttonBoundingRect.height / 2;
    return new Array(x, y);
}
