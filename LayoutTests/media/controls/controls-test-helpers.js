function statusForControlsElement(statusObject, elementName)
{
    if (statusObject.elements && statusObject.elements.length) {
        for (var i = 0; i < statusObject.elements.length; i++) {
            if (statusObject.elements[i].name == elementName)
                return statusObject.elements[i];
        }
    }
    return null;
}
