TestPage.registerInitializer(function() {

window.frameTargetForURLContainingWhenReady = async function frameTargetForURLContainingWhenReady(substring, maxAttempts = 50) {
    for (let attempt = 0; attempt < maxAttempts; ++attempt) {
        let target = await frameTargetForURLContaining(substring);
        if (target && WI.domManager.frameTargetDocumentForTarget(target))
            return target;
        await new Promise((resolve) => setTimeout(resolve, 100));
    }
    return null;
};

window.childElement = async function childElement(parent, predicate) {
    await new Promise((resolve) => parent.getChildNodes(resolve));
    return parent.children?.find(predicate) ?? null;
};

window.descendFromPageBody = async function descendFromPageBody(pageDocument, ...path) {
    let node = pageDocument.body;
    for (let predicate of path) {
        if (!node)
            return null;
        node = await childElement(node, predicate);
    }
    return node;
};

window.treeContainingNode = function treeContainingNode(node, targetNames) {
    if (!node)
        return "no node";

    let root = node;
    while (root.parentNode)
        root = root.parentNode;

    if (root === WI.domManager._document)
        return "page";

    let target = root.owningTarget;
    if (!target)
        return "detached page subtree";

    return targetNames.get(target) ?? "unnamed frame target";
};

window.isFrameTargetWithDOM = function isFrameTargetWithDOM(target) {
    return target instanceof WI.FrameTarget && target.hasDomain("DOM");
};

window.simulateInspectorReopen = async function simulateInspectorReopen(maxAttempts = 50) {
    let frameTargets = WI.targets.filter(isFrameTargetWithDOM);

    for (let target of frameTargets)
        WI.domManager._cleanupFrameTarget(target);

    WI.domManager._documentUpdated();
    let pageDocument = await WI.domManager.requestDocument();

    for (let target of frameTargets)
        WI.domManager._initializeFrameTarget(target);

    for (let attempt = 0; attempt < maxAttempts; ++attempt) {
        if (frameTargets.every((target) => WI.domManager.frameTargetDocumentForTarget(target)))
            break;
        await new Promise((resolve) => setTimeout(resolve, 100));
    }

    return pageDocument;
};

window.evaluateInFrameTarget = async function evaluateInFrameTarget(target, expression) {
    let {result} = await target.RuntimeAgent.evaluate.invoke({expression, objectGroup: "test", returnByValue: true});
    return result.value;
};

window.pushNodeIntoFrameTargetTree = async function pushNodeIntoFrameTargetTree(target, expression) {
    let {result} = await target.RuntimeAgent.evaluate.invoke({expression, objectGroup: "test"});
    let {nodeId} = await target.DOMAgent.requestNode(result.objectId);
    await target.RuntimeAgent.releaseObjectGroup("test");
    return WI.domManager.nodeForIdInFrameTarget(nodeId, target);
};

});
