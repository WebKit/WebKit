function bindCallback(obj, callback, arg1, arg2, arg3)
{
    return function(arg) {
        if (arg == undefined)
            callback.apply(obj, [arg1, arg2, arg3]);
        else
            callback.apply(obj, [arg, arg1, arg2, arg3]);
    };
}

// Usage:
//   var helper = new JoinHelper;
//
//   helper.run(function() { /* do something that eventually calls helper.done(); */ });
//   helper.run(function() { /* do something that eventually calls helper.done(); */ });
//   ...
//   helper.join(joinCallback);
//
var JoinHelper = function()
{
    this.pendingOperations = [];
    this.pendingOperationCount = 0;
    this.joinCallback = null;

    this.run = function(operation)
    {
        this.pendingOperationCount++;
        operation();
    };

    // Call this when an operation is done.
    this.done = function()
    {
        this.pendingOperationCount--;
        if (this.pendingOperationCount == 0 && this.joinCallback)
            this.joinCallback();
    };

    // This eventually calls the joinCallback when helper.done() is called as many times as helper.run() is called.
    this.join = function(joinCallback)
    {
        if (this.pendingOperationCount == 0)
            joinCallback();
        else
            this.joinCallback = joinCallback;
    };
};

// Remove everything in the given directory.
function removeAllInDirectory(directory, successCallback, errorCallback) {
    var RemoveAllInDirectoryHelper = function(successCallback, errorCallback) {
        this.entriesCount = 0;
        this.done = false;
        this.reader = null;
        this.successCallback = successCallback;
        this.errorCallback = errorCallback;

        this.entryRemovedCallback = bindCallback(this, function(entry)
        {
            if (--this.entriesCount == 0 && this.successCallback && this.done) {
                this.successCallback();
                this.successCallback = null;
            }
        });

        this.entriesCallback = bindCallback(this, function(entries)
        {
            for (var i = 0; i < entries.length; ++i) {
                this.entriesCount++;
                if (entries[i].isDirectory)
                    entries[i].removeRecursively(this.entryRemovedCallback, this.errorCallback);
                else
                    entries[i].remove(this.entryRemovedCallback, this.errorCallback);
            }
            if (entries.length)
                this.reader.readEntries(this.entriesCallback, this.errorCallback);
            else if (this.entriesCount > 0)
                this.done = true;
            else if (this.successCallback)
                this.successCallback();
        });

        this.removeAllInDirectory = function(directory)
        {
            this.reader = directory.createReader();
            this.reader.readEntries(this.entriesCallback, this.errorCallback);
        };
    };

    var helper = new RemoveAllInDirectoryHelper(successCallback, errorCallback);
    helper.removeAllInDirectory(directory);
}
