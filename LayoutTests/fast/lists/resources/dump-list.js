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

function filterListsWithReplacement(lists, processListItemFunction, postFilterFunction)
{
    processListItemFunction = processListItemFunction || dumpListItemAsHTML;
    for (var i = 0; i < lists.length; ++i) {
        var parentNode = lists[i].parentNode;
        var replacementNode = document.createElement("div");
        var result = processList(lists[i], processListItemFunction, 0);
        replacementNode.innerHTML = postFilterFunction ? postFilterFunction(result) : result;
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
    var marker = layoutTestController.markerTextForListItem(listItemElement);
    return marker ? indent(depth) + ' ' + marker + ' ' + listItemElement.innerText.trim() + '<br/>' : '';
}

function printPassedIfEmptyString(string)
{
    return string ? string : '<span class="pass">PASS</span><br/>';
}

function printFailedIfListItemMarkerNotEqualListItemText(listItemElement, depth)
{
    var marker = layoutTestController.markerTextForListItem(listItemElement);
    var expectedMarkerText = listItemElement.innerText.trim();
    if (marker === expectedMarkerText)
        return '';
    return '<span><span class="fail">FAIL</span> list marker should be ' + expectedMarkerText + '. Was ' + marker + '.</span><br/>';
}
