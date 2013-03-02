function indent(depth)
{
    var ret = new String();
    for (var i = 0; i < depth; ++i)
        ret += "-> ";
    return ret + " ";
}

function dumpList(list)
{
    return processList(list, dumpListItemAsHTML, 0);
}

function filterListsWithReplacement(lists, processListItemFunction)
{
    processListItemFunction = processListItemFunction || dumpListItemAsHTML;
    for (var i = 0; i < lists.length; ++i) {
        var parentNode = lists[i].parentNode;
        var replacementNode = document.createElement("div");
        var result = processList(lists[i], processListItemFunction, 0);
        replacementNode.innerHTML = result;
        parentNode.replaceChild(replacementNode, lists[i]);
    }
}

function processList(element, processListItemFunction, depth)
{
    var result = "";
    for (var child = element.firstElementChild; child; child = child.nextElementSibling) {
        result += processListItemFunction(child, depth);
        result += processList(child, processListItemFunction, depth + 1);
    }
    return result;
}

function dumpListItemAsHTML(listItemElement, depth)
{
    var marker = internals.markerTextForListItem(listItemElement);
    return marker ? indent(depth) + ' ' + marker + ' ' + listItemElement.innerText.trim() + '<br/>' : '';
}

function testListItemMarkerEqualsListItemText(listItemElement, depth)
{
    return testListItemMarkerEquals(internals.markerTextForListItem(listItemElement), listItemElement.innerText.trim());
}

function testListItemMarkerEquals(actualMarkerText, expectedMarkerText)
{
    if (actualMarkerText === expectedMarkerText)
        return '<span><span class="pass">PASS</span> list marker is ' + expectedMarkerText + '.</span><br/>';
    return '<span><span class="fail">FAIL</span> list marker should be ' + expectedMarkerText + '. Was ' + actualMarkerText + '.</span><br/>';
}
