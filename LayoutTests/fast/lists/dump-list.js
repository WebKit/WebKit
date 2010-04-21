function indent(depth)
{
    var ret = new String();
    for (var i = 0; i < depth; ++i)
        ret += "-> ";
    return ret + " ";
}

function dumpListHelper(element, depth)
{
    var list = new String();

    for (var child = element.firstElementChild; child; child = child.nextElementSibling) {
        var marker = layoutTestController.markerTextForListItem(child);
        if (marker)
            list += indent(depth) + ' ' + marker + ' ' + child.innerText.trim() + '<br>';
        list += dumpListHelper(child, depth + 1);
    }

    return list;
}

function dumpList(list)
{
    return dumpListHelper(list, 0);
}
