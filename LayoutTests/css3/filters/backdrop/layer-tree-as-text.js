const addLayerTree = () => {
    if (!window.internals)
        return;
    const layerTree = internals.layerTreeAsText(document, internals.LAYER_TREE_INCLUDES_CONTENT_LAYERS);
    const output = document.createElement("pre");
    output.textContent = layerTree;
    document.body.insertBefore(output, document.body.firstChild);
};

const addLayerTreeAndFinish = () => {
    addLayerTree();
    testRunner.notifyDone();
};

if (window.internals) {
    window.addEventListener("load", () => {
        if (window.doNotAutomaticallyCallLayerTree != undefined && window.doNotAutomaticallyCallLayerTree) {
            testRunner.waitUntilDone();
            return;
        }
        addLayerTree();
    }, false);

    testRunner.dumpAsText();
}
