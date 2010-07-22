/**
 * There are three basic use cases of dumpAsMarkup
 *
 * 1. Dump the entire DOM when the page is loaded
 *    When this script is included but no method of Markup is called,
 *    it dumps the DOM of each frame loaded.
 *
 * 2. Dump the content of a specific element when the page is loaded
 *    When Markup.setNodeToDump is called with some element or the id of some element,
 *    it dumps the content of the specified element as supposed to the entire DOM tree.
 *
 * 3. Dump the content of a specific element multiple times while the page is loading
 *    Calling Markup.dump would dump the content of the specified element or the entire DOM.
 *    On the page load, it dumps the content of the current node (set by setNodeToDump)
 *    or the entire DOM if the node was never set.
 */


if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}

// Namespace
var Markup = {};

// The description of what this test is testing. Gets prepended to the dumped markup.
Markup.description = function(description)
{
    Markup._description = description;
}

// Dumps the markup for the given node (HTML element if no node is given).
// Over-writes the body's content with the markup in layout test mode. Appends
// a pre element when loaded manually, in order to aid debugging.
Markup.dump = function(opt_node)
{
    var node = opt_node || Markup._node || document.body.parentElement

    var markup = Markup._description ? Markup._description + '\n' : "";
    markup += Markup.get(node);

    var container;
    if (window.layoutTestController)
        container = document.body;
    else {
        // In non-layout test mode, append the results in a pre so that we don't
        // clobber the test itself. But when in layout test mode, we don't want
        // side effects from the test to be included in the results.
        container = document.createElement('pre');
        container.style.cssText = 'width:100%';
        document.body.appendChild(container);
    }

    // FIXME: Have this respect layoutTestController.dumpChildFramesAsText?
    // FIXME: Should we care about framesets?
    var iframes = node.getElementsByTagName('iframe');
    for (var i = 0; i < iframes.length; i++) {
        markup += '\n\nFRAME ' + i + ':\n'
        try {
            markup += Markup.get(iframes[i].contentDocument.body.parentElement);
        } catch (e) {
            markup += 'FIXME: Add method to layout test controller to get access to cross-origin frames.';
        }
    }

    container.innerText = markup;
}

Markup.waitUntilDone = function()
{
    window.removeEventListener('load', Markup.notifyDone, false);
}

Markup.notifyDone = function()
{
    Markup.dump();

    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

Markup.setNodeToDump = function(node)
{
  if (typeof node == "string")
    node = document.getElementById(node);
  Markup._node = node
}

// Returns the markup for the given node. To be used for cases where a test needs
// to get the markup but not clobber the whole page.
Markup.get = function(node, opt_depth)
{
    var depth = opt_depth || 0;

    var attrs = Markup._getAttributes(node);
    var attrsString = attrs.length ? ' ' + attrs.join(' ') : '';

    var nodeName = node.nodeName;
    var markup = '<' + nodeName + attrsString + '>';

    var isSpecialNode = Markup._FORBIDS_END_TAG[nodeName];
    var innerMarkup = '';
    switch (nodeName) {
        case '#text':
        case 'STYLE':
        case 'SCRIPT':
        case 'IFRAME':
        case 'TEXTAREA':
        case 'XMP':
            innerMarkup = nodeName == '#text' ? Markup._getMarkupForTextNode(node) : node.innerText;
            innerMarkup = innerMarkup.replace(/\n/g, '\n' + Markup._spaces(depth))
            isSpecialNode = true;
            break;
        
        default:
            for (var i = 0, len = node.childNodes.length; i < len; i++) {
                innerMarkup += Markup._getSelectionMarker(node, i);
                innerMarkup += Markup.get(node.childNodes[i], depth + 1);
            }
            innerMarkup += Markup._getSelectionMarker(node, i);
    }

    markup = '\n' + Markup._spaces(depth) + markup 
    if (!isSpecialNode)
        innerMarkup += '\n' + Markup._spaces(depth);

    markup += innerMarkup;
    
    if (!Markup._FORBIDS_END_TAG[nodeName])
        markup += '</' + nodeName + '>';

    return markup;
}

Markup._spaces = function(multiplier)
{
    return new Array(multiplier * 4 + 1).join(' ');
}

// FIXME: Is there a better way to do this than a hard coded list?
Markup._DUMP_AS_MARKUP_PROPERTIES = ['src', 'type', 'href', 'style', 'class', 'id', 'contentEditable'];

Markup._getAttributes = function(node)
{
    var props = [];
    if (!node.hasAttribute)
        return props;

    for (var i = 0; i < Markup._DUMP_AS_MARKUP_PROPERTIES.length; i++) {
        var attr = Markup._DUMP_AS_MARKUP_PROPERTIES[i];
        if (node.hasAttribute(attr)) {
            props.push(attr + '="' + node.getAttribute(attr) + '"');
        }
    }
    return props;
}

// This list should match all HTML elements that return TagStatusForbidden for endTagRequirement().
Markup._FORBIDS_END_TAG = {
    'META': 1,
    'HR': 1,
    'AREA': 1,
    'LINK': 1,
    'WBR': 1,
    'BR': 1,
    'BASE': 1,
    'TABLECOL': 1,
    'SOURCE': 1,
    'INPUT': 1,
    'PARAM': 1,
    'EMBED': 1,
    'FRAME': 1,
    'CANVAS': 1,
    'IMG': 1,
    'ISINDEX': 1,
    'BASEFONT': 1,
    'DATAGRIDCELL': 1,
    'DATAGRIDCOL': 1
}

Markup._getSelectionFromNode = function(node)
{
    return node.ownerDocument.defaultView.getSelection();
}

Markup._getMarkupForTextNode = function(node)
{
    innerMarkup = node.nodeValue;
    var startOffset, endOffset, startText, endText;

    var sel = Markup._getSelectionFromNode(node);
    if (node == sel.anchorNode && node == sel.focusNode) {
        if (sel.isCollapsed) {
            startOffset = sel.anchorOffset;
            startText = '<selection-caret>';
        } else {
            if (sel.focusOffset > sel.anchorOffset) {
                startOffset = sel.anchorOffset;
                endOffset = sel.focusOffset;
                startText = '<selection-anchor>';
                endText = '<selection-focus>';
            } else {
                startOffset = sel.focusOffset;
                endOffset = sel.anchorOffset;
                startText = '<selection-focus>';
                endText = '<selection-anchor>';                        
            }
        }
    } else if (node == sel.focusNode) {
        startOffset = sel.focusOffset;
        startText = '<selection-focus>';
    } else if (node == sel.anchorNode) {
        startOffset = sel.anchorOffset;
        startText = '<selection-anchor>';
    }
    
    if (startText && endText)
        innerMarkup = innerMarkup.substring(0, startOffset) + startText + innerMarkup.substring(startOffset, endOffset) + endText + innerMarkup.substring(endOffset);                       
    else if (startText)
        innerMarkup = innerMarkup.substring(0, startOffset) + startText + innerMarkup.substring(startOffset);

    return innerMarkup;
}

Markup._getSelectionMarker = function(node, index)
{
    var sel = Markup._getSelectionFromNode(node);;
    if (index == sel.anchorOffset && node == sel.anchorNode) {
        if (sel.isCollapsed)
            return '<selection-caret>';
        else
            return '<selection-anchor>';
    } else if (index == sel.focusOffset && node == sel.focusNode)
        return '<selection-focus>';

    return '';
}

window.addEventListener('load', Markup.notifyDone, false);
