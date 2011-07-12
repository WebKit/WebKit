var base = base || {};

(function(){

base.endsWith = function(string, suffix)
{
    if (suffix.length > string.length)
        return false;
    var expectedIndex = string.length - suffix.length;
    return string.lastIndexOf(suffix) == expectedIndex;
};

base.joinPath = function(parent, child)
{
    if (parent.length == 0)
        return child;
    return parent + '/' + child;
};

base.trimExtension = function(url)
{
    var index = url.lastIndexOf('.');
    if (index == -1)
        return url;
    return url.substr(0, index);
}

base.uniquifyArray = function(array)
{
    var seen = {};
    var result = [];
    $.each(array, function(index, value) {
        if (seen[value])
            return;
        seen[value] = true;
        result.push(value);
    });
    return result;
};

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
};

base.probe = function(url, options)
{
    var scriptElement = document.createElement('script');
    scriptElement.addEventListener('load', function() {
        $(scriptElement).detach();
        if (options.success)
            options.success.call();
    }, false);
    scriptElement.addEventListener('error', function() {
        $(scriptElement).detach();
        if (options.error)
            options.error.call();
    }, false);
    scriptElement.src = url;
    document.head.appendChild(scriptElement);
};

// jQuery makes jsonp requests somewhat ugly (which is fair given that they're
// terrible for security). We use this wrapper to make our lives slightly easier.
base.jsonp = function(url, onsuccess)
{
    $.ajax({
        url: url,
        dataType: 'jsonp',
        success: onsuccess
    });
};

})();
