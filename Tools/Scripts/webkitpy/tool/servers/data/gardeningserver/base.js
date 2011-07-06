var base = base || {};

(function() {
  base.joinPath = function(parent, child) {
    return parent + '/' + child;
  }

  base.filterTree = function(tree, is_leaf, predicate) {
    var filtered_tree = {};

    function walkSubtree(subtree, directory) {
      for (var child_name in subtree) {
        var child = subtree[child_name];
        var child_path = base.joinPath(directory, child_name);
        if (is_leaf(child)) {
          if (predicate(child))
            filtered_tree[child_path] = child;
          continue;
        }
        walkSubtree(child, child_path);
      }
    }

    walkSubtree(tree, '');
    return filtered_tree;
  }
})();
