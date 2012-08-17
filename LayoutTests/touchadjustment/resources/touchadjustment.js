/* function for finding the absolute bounds of a node */
function findAbsoluteBounds(node)
{
    var bounds = {left: 0, top: 0};
    bounds.width = node.clientWidth;
    bounds.height = node.clientHeight;
    do {
        bounds.left += node.offsetLeft;
        bounds.top += node.offsetTop;
    } while (node = node.offsetParent);
    return bounds;
}

function nodeToString(node)
{
    if (node === undefined)
        return 'undefined';
    if (node === null)
        return 'null';
    if (!node.nodeName)
        return 'not a node';
    if (node.nodeType == 3)
        return "'"+node.nodeValue+"'";
    return node.nodeName + (node.id ? ('#' + node.id) : '');
}

function shouldBeNode(adjustedNode, targetNode) {
    if (typeof targetNode == "string") {
        var adjustedNodeString = nodeToString(adjustedNode);
        if (targetNode == adjustedNodeString) {
            testPassed("adjusted node was " + targetNode + ".");
        }
        else {
            testFailed("adjusted node should be " + targetNode  + ". Was " + adjustedNodeString + ".");
        }
        return;
    }
    if (targetNode == adjustedNode) {
        testPassed("adjusted node was " + nodeToString(targetNode) + ".");
    }
    else {
        testFailed("adjusted node should be " + nodeToString(targetNode)  + ". Was " + nodeToString(adjustedNode) + ".");
    }
}

function testTouchPoint(touchpoint, targetNode, allowTextNodes)
{
    var adjustedNode = internals.touchNodeAdjustedToBestClickableNode(touchpoint.left, touchpoint.top, touchpoint.width, touchpoint.height, document);
    if (!allowTextNodes && adjustedNode && adjustedNode.nodeType == 3)
        adjustedNode = adjustedNode.parentNode;
    shouldBeNode(adjustedNode, targetNode);
}

function testTouchPointContextMenu(touchpoint, targetNode, allowTextNodes)
{
    var adjustedNode = internals.touchNodeAdjustedToBestContextMenuNode(touchpoint.left, touchpoint.top, touchpoint.width, touchpoint.height, document);
    if (!allowTextNodes && adjustedNode && adjustedNode.nodeType == 3)
        adjustedNode = adjustedNode.parentNode;
    shouldBeNode(adjustedNode, targetNode);
}

function adjustTouchPointContextMenu(touchpoint)
{
    var adjustedPoint = internals.touchPositionAdjustedToBestContextMenuNode(touchpoint.left, touchpoint.top, touchpoint.width, touchpoint.height, document);
    return adjustedPoint;
}

function touchPoint(x, y, radiusX, radiusY)
{
    if (!radiusY)
        radiusY = radiusX;
    var touchpoint = new Object();
    touchpoint.left = x - radiusX;
    touchpoint.top = y - radiusY;
    touchpoint.width = radiusX * 2;
    touchpoint.height = radiusY * 2;
    return touchpoint;
}

function offsetTouchPoint(bounds, relativePosition, touchOffset, touchRadiusX, touchRadiusY)
{
    if (!touchRadiusY)
        touchRadiusY = touchRadiusX;

    var touchpoint = {left : bounds.left, top: bounds.top };
 
    // Set center point for touch.
    switch (relativePosition) {
    case 'center':
        touchpoint.left += bounds.width / 2;
        touchpoint.top += bounds.height / 2;
        break;
    case 'left':
        touchpoint.left -= touchOffset;
        touchpoint.top += bounds.height / 2;
        break;
    case 'right':
        touchpoint.left += bounds.width + touchOffset;
        touchpoint.top +=  bounds.height / 2;
        break;
    case 'top-left':
        touchpoint.left -= touchOffset;
        touchpoint.top -= touchOffset;
        break;
    case 'top-right':
        touchpoint.left += bounds.width + touchOffset;
        touchpoint.top -= touchOffset;
        break;
    case 'bottom-left':
        touchpoint.left -= touchOffset;
        touchpoint.top += bounds.height + touchOffset;
        break;
    case 'bottom-right':
        touchpoint.left += bounds.width + touchOffset;
        touchpoint.top += bounds.height + touchOffset;
        break;
    case 'top':
        touchpoint.left += bounds.width / 2;
        touchpoint.top -= touchOffset;
        break;
    case 'bottom':
        touchpoint.left += bounds.width / 2;
        touchpoint.top += bounds.height + touchOffset;
    }
    // Adjust from touch center to top-left corner.
    touchpoint.left -= touchRadiusX;
    touchpoint.top -= touchRadiusY;

    touchpoint.width = 2 * touchRadiusX;
    touchpoint.height = 2 * touchRadiusY;

    return touchpoint;
}
