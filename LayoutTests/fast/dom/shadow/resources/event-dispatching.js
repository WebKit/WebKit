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
    var eventType = event.type
    if (!eventRecords[eventType]) {
        eventRecords[eventType] = []
    }
    var eventString = '';
    if (event.currentTarget)
        eventString += ' @' + event.currentTarget.id;
    if (event.target)
        eventString += ' (target: ' + event.target.id + ')';
    if (event.relatedTarget)
        eventString += ' (related: ' + event.relatedTarget.id + ')';
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
        node.addEventListener('mouseover', recordEvent, false);
        node.addEventListener('mouseout', recordEvent, false);
        node.addEventListener('click', recordEvent, false);
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

