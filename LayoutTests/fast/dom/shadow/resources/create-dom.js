function createShadowRoot()
{
    return {'isShadowRoot': true,
            'children': Array.prototype.slice.call(arguments)};
}

// This function can take optional child elements, which might be a result of createShadowRoot(), as arguments[2:].
function createDOM(tagName, attributes)
{
    var element = document.createElement(tagName);
    for (var name in attributes)
        element.setAttribute(name, attributes[name]);
    var childElements = Array.prototype.slice.call(arguments, 2);
    for (var i = 0; i < childElements.length; ++i) {
        var child = childElements[i];
        if (child.isShadowRoot) {
            var shadowRoot;
            if (window.WebKitShadowRoot)
              shadowRoot = new WebKitShadowRoot(element);
            else
              shadowRoot = internals.ensureShadowRoot(element);
            for (var j = 0; j < child.children.length; ++j)
                shadowRoot.appendChild(child.children[j]);
        } else
            element.appendChild(child);
    }
    return element;
}
