description("Tests to ensure that event dispatching behaves as the Shadow DOM spec describes.");

var defaultPaddingSize = 40;

function moveMouseOver(element)
{
    if (!window.eventSender || !window.internals)
        return;

    var x = element.offsetLeft + element.offsetWidth / 2;
    var y;
    if (element.hasChildNodes() || window.internals.shadowRoot(element))
        y = element.offsetTop + defaultPaddingSize / 2;
    else
        y = element.offsetTop + element.offsetHeight / 2;
    eventSender.mouseMoveTo(x, y);
}

function touchLocation(node)
{
    var x = node.offsetLeft + 5;
    var y = node.offsetTop + defaultPaddingSize + 5;
    eventSender.addTouchPoint(x, y);
    eventSender.touchStart();
    eventSender.leapForward(100);
    eventSender.touchEnd();
    eventSender.cancelTouchPoint(0);
}

function selectTextNode(node)
{
    getSelection().setBaseAndExtent(node, 0, node, node.length);
}

function dragMouse(node) {
    var x = node.offsetLeft + 5;
    var y = node.offsetTop + defaultPaddingSize + 5;

    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.leapForward(100);
    eventSender.mouseMoveTo(x + 100, y + 100);
    eventSender.mouseUp();
    eventSender.mouseMoveTo(x, y);
}

function scrollMouseWheel(node) {
    var x = node.offsetLeft + 5;
    var y = node.offsetTop + defaultPaddingSize + 5;
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseScrollBy(0, 120);
}

var eventRecords = {};

function clearEventRecords()
{
    eventRecords = {};
}

function dispatchedEvent(eventType)
{
    var events = eventRecords[eventType];
    if (!events)
        return [];
    return events;
}

function recordEvent(event)
{
    var eventType = event.type;
    if (!eventRecords[eventType]) {
        eventRecords[eventType] = [];
    }
    var eventString = '';
    if (event.currentTarget)
        eventString += ' @' + event.currentTarget.id;
    if (event.target)
        eventString += ' (target: ' + event.target.id + ')';
    if (event.relatedTarget)
        eventString += ' (related: ' + event.relatedTarget.id + ')';
    if (event.touches)
        eventString += ' (touches: ' + dumpTouchList(event.touches) + ')';
    if (event.targetTouches)
        eventString += ' (targetTouches: ' + dumpTouchList(event.targetTouches) + ')';
    if (event.changedTouches)
        eventString += ' (changedTouches: ' + dumpTouchList(event.changedTouches) + ')';
    if (event.eventPhase == 1)
        eventString += '(capturing phase)';
    if (event.target && event.currentTarget && event.target.id == event.currentTarget.id)
        shouldBe("event.eventPhase", "2", true);
    eventRecords[eventType].push(eventString);
}

function dumpNode(node)
{
    var output = node.nodeName + "\t";
    if (node.id)
        output += ' id=' + node.id;
    if (node.className)
        output += ' class=' + node.className;
    return output;
}

function dumpTouchList(touches) {
    var ids = [];
    for (var i = 0; i < touches.length; ++i) {
        if (touches.item(i).target && touches.item(i).target.id)
            ids.push(touches.item(i).target.id);
    }
    ids.sort();
    var result = '';
    for (i = 0; i < ids.length; ++i) {
         result += ids[i];
         if (i != ids.length - 1)
             result += ', ';
    }
    return result;
}

function dumpComposedShadowTree(node, indent)
{
    indent = indent || "";
    var output = indent + dumpNode(node) + "\n";
    var child;
    for (child = internals.firstChildByWalker(node); child; child = internals.nextSiblingByWalker(child))
         output += dumpComposedShadowTree(child, indent + "\t");
    return output;
}

function addEventListeners(nodes)
{
    for (var i = 0; i < nodes.length; ++i) {
        var node = getNodeInShadowTreeStack(nodes[i]);
        node.addEventListener('click', recordEvent, false);
        node.addEventListener('dragstart', recordEvent, false);
        node.addEventListener('mouseout', recordEvent, false);
        node.addEventListener('mouseover', recordEvent, false);
        node.addEventListener('mousewheel', recordEvent, false);
        node.addEventListener('touchstart', recordEvent, false);
        node.addEventListener('gesturetap', recordEvent, false);
        // <content> might be an inactive insertion point, so style it also.
        if (node.tagName == 'DIV' || node.tagName == 'DETAILS' || node.tagName == 'SUMMARY' || node.tagName == 'CONTENT')
            node.setAttribute('style', 'padding-top: ' + defaultPaddingSize + 'px;');
    }
}

function debugDispatchedEvent(eventType)
{
    debug('\n  ' + eventType);
    var events = dispatchedEvent(eventType);
    for (var i = 0; i < events.length; ++i)
        debug('    ' + events[i])
}

function sortDispatchedEvent(eventType)
{
    dispatchedEvent(eventType).sort();
}

function moveMouse(oldElementId, newElementId)
{
    clearEventRecords();
    debug('\n' + 'Moving mouse from ' + oldElementId + ' to ' + newElementId);
    moveMouseOver(getNodeInShadowTreeStack(oldElementId));

    clearEventRecords();
    moveMouseOver(getNodeInShadowTreeStack(newElementId));

    debugDispatchedEvent('mouseout');
    debugDispatchedEvent('mouseover');
}

function showSandboxTree()
{
    var sandbox = document.getElementById('sandbox');
    sandbox.offsetLeft;
    debug('\n\nComposed Shadow Tree will be:\n' + dumpComposedShadowTree(sandbox));
}
