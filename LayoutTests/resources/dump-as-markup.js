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
 *    Calling Markup.dump would dump the content of the element set by setNodeToDump or the entire DOM.
 *    Optionally specify the node to dump and the description for each call of dump.
 */

if (window.layoutTestController)
    layoutTestController.dumpAsText();

// Namespace
// FIXME: Rename dump-as-markup.js to dump-dom.js and Markup to DOM.
var Markup = {};

// The description of what this test is testing. Gets prepended to the dumped markup.
Markup.description = function(description)
{
    Markup._test_description = description;
}

// Dumps the markup for the given node (HTML element if no node is given).
// Over-writes the body's content with the markup in layout test mode. Appends
// a pre element when loaded manually, in order to aid debugging.
Markup.dump = function(opt_node, opt_description)
{
    if (typeof opt_node == 'string')
        opt_node = document.getElementById(opt_node);

    var node = opt_node || document
    var markup = "";

    Markup._dumpCalls++;

    if (Markup._dumpCalls > 1 || opt_description) {
        if (!opt_description)
            opt_description = "Dump of markup " + Markup._dumpCalls
        if (Markup._dumpCalls > 1)
            markup += '\n';
        markup += '\n' + opt_description + ':\n';
    } else
        Markup._firstCallDidNotHaveDescription = true;

    markup += Markup.get(node);

    if (!Markup._container) {
        Markup._container = document.createElement('pre');
        Markup._container.style.width = '100%';
    }

    if (Markup._dumpCalls == 2 && Markup._firstCallDidNotHaveDescription) {
        var wrapper = Markup._container.getElementsByClassName('dump-as-markup-span')[0];
        wrapper.insertBefore(document.createTextNode('\nDump of markup 1:\n'), wrapper.firstChild);
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

    if (Markup._test_description && Markup._dumpCalls == 1)
        Markup._container.appendChild(document.createTextNode(Markup._test_description + '\n'))

    var wrapper = document.createElement('span');
    wrapper.className = 'dump-as-markup-span';
    wrapper.appendChild(document.createTextNode(markup));
    Markup._container.appendChild(wrapper);
}

Markup.noAutoDump = function()
{
    window.removeEventListener('load', Markup.notifyDone, false);
}

Markup.waitUntilDone = function()
{
    if (window.layoutTestController)
        layoutTestController.waitUntilDone();
    Markup.noAutoDump();
}

Markup.notifyDone = function()
{
    // Need to waitUntilDone or some tests won't finish appending the markup before the text is dumped.
    if (window.layoutTestController)
        layoutTestController.waitUntilDone();

    // If dump has already been called, don't bother to dump again
    if (!Markup._dumpCalls)
        Markup.dump();

    // In non-layout test mode, append the results in a pre so that we don't
    // clobber the test itself. But when in layout test mode, we don't want
    // side effects from the test to be included in the results.
    if (window.layoutTestController)
        document.body.innerHTML = '';

    document.body.appendChild(Markup._container);

    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

Markup.get = function(node)
{
    if (!node.firstChild)
        return '| ';

    // Don't print any markup for the root node.
    var markup = '';
    for (var i = 0, len = node.childNodes.length; i < len; i++) {
        markup += Markup._get(node.childNodes[i], 0);
    }
    return markup.substring(1);
}

// Returns the markup for the given node. To be used for cases where a test needs
// to get the markup but not clobber the whole page.
Markup._get = function(node, depth)
{
    var str = Markup._indent(depth);

    switch (node.nodeType) {
    case 10:
        str += '<!DOCTYPE ' + node.nodeName;
        if (node.publicId || node.systemId) {
            str += ' "' + node.publicId + '"';
            str += ' "' + node.systemId + '"';
        }
        str += '>';
        break;

    case 8:
        try {
            str += '<!-- ' + node.nodeValue + ' -->';
        } catch (e) {
            str += '<!--  -->';
        }
        break;

    case 7:
        str += '<?' + node.nodeName + node.nodeValue + '>';
        break;

    case 4:
        str += '<![CDATA[ ' + node.nodeValue + ' ]]>';
        break;

    case 3:
        str += '"' + Markup._getMarkupForTextNode(node) + '"';
        break;

    case 1:
        str += "<";
        str += Markup._namespace(node)

        if (node.localName && node.namespaceURI && node.namespaceURI != null)
            str += node.localName;
        else
            str += Markup._toAsciiLowerCase(node.nodeName);

        str += '>';

        if (node.attributes) {
            var attrNames = [];
            var attrPos = {};
            for (var j = 0; j < node.attributes.length; j += 1) {
                if (node.attributes[j].specified) {
                    var name = Markup._namespace(node.attributes[j])
                    name += node.attributes[j].localName || node.attributes[j].nodeName;
                    attrNames.push(name);
                    attrPos[name] = j;
                }
            }
            if (attrNames.length > 0) {
              attrNames.sort();
              for (var j = 0; j < attrNames.length; j += 1) {
                str += Markup._indent(depth + 1) + attrNames[j];
                str += '="' + node.attributes[attrPos[attrNames[j]]].nodeValue + '"';
              }
            }
        }
        break;
    }

    for (var i = 0, len = node.childNodes.length; i < len; i++) {
        var selection = Markup._getSelectionMarker(node, i);
        if (selection)
            str += Markup._indent(depth + 1) + selection;

        str += Markup._get(node.childNodes[i], depth + 1);
    }
    
    var selection = Markup._getSelectionMarker(node, i);
    if (selection)
        str += Markup._indent(depth + 1) + selection;

    return str;
}

Markup._namespace = function(node)
{
    if (Markup._NAMESPACE_URI_MAP[node.namespaceURI])
        return Markup._NAMESPACE_URI_MAP[node.namespaceURI] + ' ';
    return '';
}

Markup._dumpCalls = 0

Markup._indent = function(depth)
{
    return "\n| " + new Array(depth * 2 + 1).join(' ');
}

Markup._toAsciiLowerCase = function (str) {
  var output = "";
  for (var i = 0, len = this.length; i < len; ++i) {
    if (str.charCodeAt(i) >= 0x41 && str.charCodeAt(i) <= 0x5A)
      output += String.fromCharCode(str.charCodeAt(i) + 0x20)
    else
      output += str.charAt(i);
  }
  return output;
}

Markup._NAMESPACE_URI_MAP = {
    "http://www.w3.org/2000/svg": "svg",
    "http://www.w3.org/1998/Math/MathML": "math",
    "http://www.w3.org/XML/1998/namespace": "xml",
    "http://www.w3.org/2000/xmlns/": "xmlns",
    "http://www.w3.org/1999/xlink": "xlink"
}

Markup._getSelectionFromNode = function(node)
{
    return node.ownerDocument.defaultView.getSelection();
}

Markup._SELECTION_FOCUS = '<#selection-focus>';
Markup._SELECTION_ANCHOR = '<#selection-anchor>';
Markup._SELECTION_CARET = '<#selection-caret>';

Markup._getMarkupForTextNode = function(node)
{
    innerMarkup = node.nodeValue;
    var startOffset, endOffset, startText, endText;

    var sel = Markup._getSelectionFromNode(node);
    // Firefox doesn't have a sel in a display:none iframe.
    // https://bugs.webkit.org/show_bug.cgi?id=43655
    if (sel) {
        if (node == sel.anchorNode && node == sel.focusNode) {
            if (sel.isCollapsed) {
                startOffset = sel.anchorOffset;
                startText = Markup._SELECTION_CARET;
            } else {
                if (sel.focusOffset > sel.anchorOffset) {
                    startOffset = sel.anchorOffset;
                    endOffset = sel.focusOffset;
                    startText = Markup._SELECTION_ANCHOR;
                    endText = Markup._SELECTION_FOCUS;
                } else {
                    startOffset = sel.focusOffset;
                    endOffset = sel.anchorOffset;
                    startText = Markup._SELECTION_FOCUS;
                    endText = Markup._SELECTION_ANCHOR;
                }
            }
        } else if (node == sel.focusNode) {
            startOffset = sel.focusOffset;
            startText = Markup._SELECTION_FOCUS;
        } else if (node == sel.anchorNode) {
            startOffset = sel.anchorOffset;
            startText = Markup._SELECTION_ANCHOR;
        }
    }
    
    if (startText && endText)
        innerMarkup = innerMarkup.substring(0, startOffset) + startText + innerMarkup.substring(startOffset, endOffset) + endText + innerMarkup.substring(endOffset);                       
    else if (startText)
        innerMarkup = innerMarkup.substring(0, startOffset) + startText + innerMarkup.substring(startOffset);

    return innerMarkup;
}

Markup._getSelectionMarker = function(node, index)
{
    if (node.nodeType != 1)
        return '';

    var sel = Markup._getSelectionFromNode(node);;

    // Firefox doesn't have a sel in a display:none iframe.
    // https://bugs.webkit.org/show_bug.cgi?id=43655
    if (!sel)
        return '';

    if (index == sel.anchorOffset && node == sel.anchorNode) {
        if (sel.isCollapsed)
            return Markup._SELECTION_CARET;
        else
            return Markup._SELECTION_ANCHOR;
    } else if (index == sel.focusOffset && node == sel.focusNode)
        return Markup._SELECTION_FOCUS;

    return '';
}

window.addEventListener('load', Markup.notifyDone, false);
