/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
 */

if (!Node) {
    // MSIE doesn't define Node, so provide a compatibility object
    var Node = { TEXT_NODE: 3 }
}

if (!highlighted) {
    var highlighted = 0;
    var highlightedclass = "";
    var highlightedover = 0;
}

function doToggle(node, event) {
    var deep = event.altKey || event.ctrlKey;

    if (node.nodeType == Node.TEXT_NODE)
        node = node.parentNode;

    var toggle = node.nextSibling;
    while (toggle && toggle.tagName != "UL")
        toggle = toggle.nextSibling;

    if (toggle) {
        if (deep) {
            var direction = toggleDisplay(toggle, node);
            changeChildren(toggle, direction);
        } else {
            toggleDisplay(toggle, node);
        }
    }
    /* avoid problems with default actions on links (mozilla's
     * ctrl/shift-click defaults, for instance */
    event.preventBubble();
    event.preventDefault();
    return false;
}

function changeChildren(node, direction) {
    var item = node.firstChild;
    while (item) {
        /* find the LI inside the UL I got */
        while (item && item.tagName != "LI")
            item = item.nextSibling;
        if (!item)
            return;

        /* got it, now find the first A */
        var child = item.firstChild;
        while (child && child.tagName != "A")
            child = child.nextSibling;
        if (!child) {
            return
        }
        var bullet = child;

        /* and check if it has its own sublist */
        var sublist = item.firstChild;
        while (sublist && sublist.tagName != "UL")
            sublist = sublist.nextSibling;
        if (sublist) {
            if (direction && isClosed(sublist)) {
                openNode(sublist, bullet);
            } else if (!direction && !isClosed(sublist)) {
                closeNode(sublist, bullet);
            }
            changeChildren(sublist, direction)
        }
        item = item.nextSibling;
    }
}

function openNode(node, bullet) {
    node.style.display = "block";
    bullet.className = "b b_open";
}

function closeNode(node, bullet) {
    node.style.display = "none";
    bullet.className = "b b_closed";
}

function isClosed(node) {
    /* XXX we should in fact check our *computed* style, not the display
     * attribute of the current node, which may be inherited and not
     * set. However, this really only matters when changing the default
     * appearance of the tree through a parent style. */
    return node.style.display == "none";
}

function toggleDisplay(node, bullet) {
    if (isClosed(node)) {
        openNode(node, bullet);
        return true;
    }

    closeNode(node, bullet);
    return false;
}

function duplicated(element) {
    var allsumm= document.getElementsByTagName("span");
    if (highlighted) {
        for (i = 0;i < allsumm.length; i++) {
            if (allsumm.item(i).id == highlighted) {
                allsumm.item(i).className = highlightedclass;
            }
        }
        if (highlighted == element) {
            highlighted = 0;
            return;
        }
    } 
    highlighted = element;
    var elem = document.getElementById(element);
    highlightedclass = elem.className;
    for (var i = 0;i < allsumm.length; i++) {
        if (allsumm.item(i).id == element) {
            allsumm.item(i).className = "summ_h";
        }
    }
}

function duplicatedover(element) {
    if (!highlighted) {
        highlightedover = 1;
        duplicated(element);
    }
}

function duplicatedout(element) {
    if (highlighted == element && highlightedover) {
        highlightedover = 0;
        duplicated(element);
    }
}
