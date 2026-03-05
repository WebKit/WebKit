function extractEventRegionFromLayerTree(output) {
    const startMarker = '(event region';
    const startIndex = output.indexOf(startMarker);

    if (startIndex === -1)
        return "no event region";

    let depth = 0;
    let endIndex = startIndex;

    for (let i = startIndex; i < output.length; i++) {
        if (output[i] === '(') {
            depth++;
        } else if (output[i] === ')') {
            depth--;
            if (depth === 0) {
                endIndex = i + 1;
                break;
            }
        }
    }

    return output.substring(startIndex, endIndex).trim();
}

function prepareForTest() {
    if (!window.internals)
        return;

    testRunner.dumpAsText();
    testRunner.waitUntilDone();
    testRunner.dontForceRepaint();
}

function dumpRegions() {
    if (!window.internals)
        return;

    var layerTree = internals.layerTreeAsText(document, internals.LAYER_TREE_INCLUDES_EVENT_REGION | internals.LAYER_TREE_INCLUDES_ROOT_LAYER_PROPERTIES);
    var resultString = extractEventRegionFromLayerTree(layerTree);

    document.documentElement.innerHTML = '';
    var resultsPre = document.createElement('pre');
    resultsPre.textContent = resultString;
    document.body.appendChild(resultsPre);
    testRunner.notifyDone();
}
