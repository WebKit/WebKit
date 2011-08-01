// This function can take optional child elements as arguments[2:].
function createShadow(tagName, attributes)
{
    var element = document.createElement(tagName);
    for (var name in attributes)
        element.setAttribute(name, attributes[name]);
    var shadow = internals.ensureShadowRoot(element);
    var childElements = Array.prototype.slice.call(arguments, 2);
    for (var i = 0; i < childElements.length; ++i)
        shadow.appendChild(childElements[i]);
    return element;
}

// This function can take optional child elements as arguments[2:].
function createDom(tagName, attributes)
{
    var element = document.createElement(tagName);
    for (var name in attributes)
        element.setAttribute(name, attributes[name]);
    var childElements = Array.prototype.slice.call(arguments, 2);
    for (var i = 0; i < childElements.length; ++i)
        element.appendChild(childElements[i]);
    return element;
}
