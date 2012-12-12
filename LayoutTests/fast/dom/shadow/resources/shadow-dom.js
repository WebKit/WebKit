function createShadowRoot()
{
    return {'isShadowRoot': true,
            'children': Array.prototype.slice.call(arguments)};
}

function createShadowRootWithAttributes(attributes, children)
{
    return {'isShadowRoot': true,
            'attributes': attributes,
            'children': children};
}

// This function can take optional child elements, which might be a result of createShadowRoot(), as arguments[2:].
// You must enable SHADOW_DOM flag if you use this fucntion to host multiple ShadowRoots
// since window.internals does not have a function which can be used to host multiple shadow roots.
// FIXME: window.internals should have such function and remove the restriction.
function createDOM(tagName, attributes)
{
    var element = document.createElement(tagName);
    for (var name in attributes)
        element.setAttribute(name, attributes[name]);
    var childElements = Array.prototype.slice.call(arguments, 2);
    var shadowRootCount = 0;
    for (var i = 0; i < childElements.length; ++i) {
        var child = childElements[i];
        if (child.isShadowRoot) {
            ++shadowRootCount;
            var shadowRoot;
            if (element.webkitCreateShadowRoot)
                shadowRoot = element.webkitCreateShadowRoot(element);
            else
                shadowRoot = internals.createShadowRoot(element);
            if (child.attributes) {
                for (var attribute in child.attributes) {
                    // Shadow Root does not have setAttribute.
                    shadowRoot[attribute] = child.attributes[attribute];
                }
            }
            for (var j = 0; j < child.children.length; ++j)
                shadowRoot.appendChild(child.children[j]);
        } else
            element.appendChild(child);
    }
    return element;
}

function isShadowHost(node)
{
    return window.internals.oldestShadowRoot(node);
}

function isShadowRoot(node)
{
    // FIXME: window.internals should have internals.isShadowRoot(node).
    return node.nodeName == "#shadow-root";
}

function isIframeElement(element)
{
    return element && element.nodeName == 'IFRAME';
}

// You can spefify youngerShadowRoot by consecutive slashes.
// See LayoutTests/fast/dom/shadow/get-element-by-id-in-shadow-root.html for actual usages.
function getNodeInShadowTreeStack(path)
{
    var ids = path.split('/');
    var node = document.getElementById(ids[0]);
    for (var i = 1; node != null && i < ids.length; ++i) {
        if (isIframeElement(node)) {
            node = node.contentDocument.getElementById(ids[i]);
            continue;
        }
        if (isShadowRoot(node))
            node = internals.youngerShadowRoot(node);
        else if (internals.oldestShadowRoot(node))
            node = internals.oldestShadowRoot(node);
        else
            return null;
        if (ids[i] != '')
            node = node.getElementById(ids[i]);
    }
    return node;
}

function dumpNode(node)
{
    if (!node)
      return 'null';
    var output = '' + node;
    if (node.id)
        output += ' id=' + node.id;
    return output;
}

function innermostActiveElement(element)
{
    element = element || document.activeElement;
    if (isIframeElement(element)) {
        if (element.contentDocument.activeElement)
            return innermostActiveElement(element.contentDocument.activeElement);
        return element;
    }
    if (isShadowHost(element)) {
        var shadowRoot = window.internals.oldestShadowRoot(element);
        while (shadowRoot) {
            if (shadowRoot.activeElement)
                return innermostActiveElement(shadowRoot.activeElement);
            shadowRoot = window.internals.youngerShadowRoot(shadowRoot);
        }
    }
    return element;
}

function isInnermostActiveElement(id)
{
    var element = getNodeInShadowTreeStack(id);
    if (!element) {
        debug('FAIL: There is no such element with id: '+ from);
        return false;
    }
    if (element == innermostActiveElement())
        return true;
    debug('Expected innermost activeElement is ' + id + ', but actual innermost activeElement is ' + dumpNode(innermostActiveElement()));
    return false;
}

function shouldNavigateFocus(from, to, direction)
{
    debug('Should move from ' + from + ' to ' + to + ' in ' + direction);
    var fromElement = getNodeInShadowTreeStack(from);
    if (!fromElement) {
      debug('FAIL: There is no such element with id: '+ from);
      return;
    }
    fromElement.focus();
    if (!isInnermostActiveElement(from)) {
        debug('FAIL: Can not be focused: '+ from);
        return;
    }
    if (direction == 'forward')
        navigateFocusForward();
    else
        navigateFocusBackward();
    if (isInnermostActiveElement(to))
        debug('PASS');
    else
        debug('FAIL');
}

function navigateFocusForward()
{
    eventSender.keyDown('\t');
}

function navigateFocusBackward()
{
    eventSender.keyDown('\t', ['shiftKey']);
}

function testFocusNavigationFowrad(elements)
{
    for (var i = 0; i + 1 < elements.length; ++i)
        shouldNavigateFocus(elements[i], elements[i + 1], 'forward');
}

function testFocusNavigationBackward(elements)
{
    for (var i = 0; i + 1 < elements.length; ++i)
        shouldNavigateFocus(elements[i], elements[i + 1], 'backward');
}
