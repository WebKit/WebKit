function text(text) {
    return document.createTextNode(text);
}

function element(elementName, attributesOrChildNodes, childNodes) {
    var element = document.createElement(elementName);

    if (attributesOrChildNodes instanceof Array)
        childNodes = attributesOrChildNodes;
    else if (attributesOrChildNodes) {
        for (var attributeName in attributesOrChildNodes)
            element.setAttribute(attributeName, attributesOrChildNodes[attributeName]);
    }

    if (childNodes) {
        for (var i = 0; i < childNodes.length; i++) {
            if (typeof(childNodes[i]) === 'string')
                element.appendChild(document.createTextNode(childNodes[i]));
            else
                element.appendChild(childNodes[i]);
        }
    }
    return element;
}
