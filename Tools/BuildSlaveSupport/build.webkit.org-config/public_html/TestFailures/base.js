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

base.keys = function(dictionary)
{
    var keys = [];
    $.each(dictionary, function(key, value) {
        keys.push(key);
    });
    return keys;
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

base.RequestTracker = function(requestsInFlight, callback, args)
{
    this._requestsInFlight = requestsInFlight;
    this._callback = callback;
    this._args = args || [];
};

base.RequestTracker.prototype.requestComplete = function()
{
    --this._requestsInFlight;
    if (!this._requestsInFlight)
        this._callback.apply(null, this._args);
};

base.CallbackIterator = function(callback, listOfArgumentArrays)
{
    this._callback = callback;
    this._nextIndex = 0;
    this._listOfArgumentArrays = listOfArgumentArrays;
};

base.CallbackIterator.prototype.hasNext = function()
{
    return this._nextIndex < this._listOfArgumentArrays.length;
};

base.CallbackIterator.prototype.hasPrevious = function()
{
    return this._nextIndex - 2 >= 0;
};

base.CallbackIterator.prototype.callNext = function()
{
    if (!this.hasNext())
        return;
    var args = this._listOfArgumentArrays[this._nextIndex];
    this._nextIndex++;
    this._callback.apply(null, args);
};

base.CallbackIterator.prototype.callPrevious = function()
{
    if (!this.hasPrevious())
        return;
    var args = this._listOfArgumentArrays[this._nextIndex - 2];
    this._nextIndex--;
    this._callback.apply(null, args);
};

base.AsynchronousCache = function(fetch)
{
    this._fetch = fetch;
    this._dataCache = {};
    this._callbackCache = {};
};

base.AsynchronousCache.prototype.get = function(key, callback)
{
    var self = this;

    if (self._dataCache[key]) {
        // FIXME: Consider always calling callback asynchronously.
        callback(self._dataCache[key]);
        return;
    }

    if (key in self._callbackCache) {
        self._callbackCache[key].push(callback);
        return;
    }

    self._callbackCache[key] = [callback];

    self._fetch.call(null, key, function(data) {
        self._dataCache[key] = data;

        var callbackList = self._callbackCache[key];
        delete self._callbackCache[key];

        callbackList.forEach(function(cachedCallback) {
            cachedCallback(data);
        });
    });
};

})();
