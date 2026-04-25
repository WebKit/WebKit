function matchedNodesContainId(root, selector, expectedId) {
    var matchedNodes = root.querySelectorAll(selector);
    for (var i = 0; i < matchedNodes.length; ++i) {
        if (matchedNodes[i].id == expectedId)
            return true;
    }
    return false;
}
