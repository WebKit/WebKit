TestPage.registerInitializer(() => {
    ProtocolTest.CallingContextTree = {};

    ProtocolTest.CallingContextTree.createFromProtocolMessageObject = function(messageObject) {
        const target = null;
        let tree = new WI.CallingContextTree(target);
        for (let stackTrace of messageObject.params.samples.stackTraces)
            tree.updateTreeWithStackTrace(stackTrace);
        return tree;
    };

    ProtocolTest.CallingContextTree.matchesStackTrace = function(tree, stackTrace) {
        // StackTrace should have top frame first in the array and bottom frame last.
        // We don't look for a match that traces down the tree from the root; instead,
        // we match by looking at all the leafs, and matching while walking up the tree
        // towards the root. If we successfully make the walk, we've got a match that
        // suffices for a particular test. A successful match doesn't mean we actually
        // walk all the way up to the root; it just means we didn't fail while walking
        // in the direction of the root.

        function buildLeafLinkedLists(treeNode, listParent, result) {
            let linkedListNode = {
                name: treeNode.name,
                url: treeNode.url,
                parent: listParent,
            };
            if (treeNode.hasChildren()) {
                treeNode.forEachChild((childTreeNode) => {
                    buildLeafLinkedLists(childTreeNode, linkedListNode, result);
                });
            } else
                result.push(linkedListNode);
        }
        let leaves = [];
        buildLeafLinkedLists(tree._root, null, leaves);

        outer:
        for (let node of leaves) {
            for (let stackNode of stackTrace) {
                for (let propertyName of Object.getOwnPropertyNames(stackNode)) {
                    if (stackNode[propertyName] !== node[propertyName])
                        continue outer;
                }
                node = node.parent;
            }
            return true;
        }
        return false;
    };
});
