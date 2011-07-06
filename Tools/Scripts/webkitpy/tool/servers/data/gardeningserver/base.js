var base = base || {};

(function(){

base.joinPath = function(parent, child)
{
    if (parent.length == 0)
        return child;
    return parent + '/' + child;
}

base.filterTree = function(tree, isLeaf, predicate)
{
    var filteredTree = {};

    function walkSubtree(subtree, directory)
    {
        for (var childName in subtree) {
            var child = subtree[childName];
            var childPath = base.joinPath(directory, childName);
            if (isLeaf(child)) {
                if (predicate(child))
                    filteredTree[childPath] = child;
                continue;
            }
            walkSubtree(child, childPath);
        }
    }

    walkSubtree(tree, '');
    return filteredTree;
}

})();
