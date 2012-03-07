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

function isShadowRoot(node)
{
    // FIXME: window.internals should have internals.isShadowRoot(node).
    return node.host;
}

// You can spefify youngerShadowRoot by consecutive slashes.
// See LayoutTests/fast/dom/shadow/get-element-by-id-in-shadow-root.html for actual usages.
function getElementInShadowTreeStack(path)
{
    var ids = path.split('/');
    var node = document.getElementById(ids[0]);
    for (var i = 1; node != null && i < ids.length; ++i) {
        if (isShadowRoot(node))
            node = internals.youngerShadowRoot(node);
        else
            node = internals.oldestShadowRoot(node);
        if (ids[i] != '')
            node = internals.getElementByIdInShadowRoot(node, ids[i]);
    }
    return node;
}

