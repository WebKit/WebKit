if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}

// Namespace
var Markup = {};

/**
 * Dumps the markup for the given node (HTML element if no node is given).
 * Over-writes the body's content with the markup in layout test mode. Appends
 * a pre element when loaded manually, in order to aid debugging.
 */
Markup.dump = function(opt_node)
{
    // Allow for using Markup.dump as an onload handler.
    if (opt_node instanceof Event)
        opt_node = undefined;

    var markup = Markup.get(opt_node);
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
    for (var i = 0; i < window.frames.length; i++) {
        markup += '\n\nFRAME ' + i + ':\n'
        try {
            markup += Markup.get(window.frames[i].contentDocument.body.parentElement);
        } catch (e) {
            markup += 'FIXME: Add method to layout test controller to get access to cross-origin frames.';
        }
    }

    container.innerText = markup;

    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

Markup.waitUntilDone = function()
{
    window.removeEventListener('load', Markup.dump, false);
}

Markup.notifyDone = function()
{
    Markup.dump();
}

/**
 * Returns the markup for the given node. To be used for cases where a test needs
 * to get the markup but not clobber the whole page.
 */
Markup.get = function(opt_node, opt_depth)
{
    var node = opt_node || document.body.parentElement
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

Markup._getMarkupForTextNode = function(node)
{
    innerMarkup = node.nodeValue;
    var startOffset, endOffset, startText, endText;

    var sel = window.getSelection();
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
    var sel = window.getSelection();
    if (index == sel.anchorOffset && node == sel.anchorNode) {
        if (sel.isCollapsed)
            return '<selection-caret>';
        else
            return '<selection-anchor>';
    } else if (index == sel.focusOffset && node == sel.focusNode)
        return '<selection-focus>';

    return '';
}

window.addEventListener('load', Markup.dump, false);
