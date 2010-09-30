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

// FIXME: replace this code with the equivalent File API method once it fully supports removeRecursively.
function removeRecursively(directory, successCallback, errorCallback) {
    var RemoveRecursiveHelper = function(successCallback, errorCallback) {
        this.removeDirMap = {};
        this.successCallback = successCallback;
        this.errorCallback = errorCallback;

        this.removeRecursively = function(directory)
        {
            this.removeRecursivelyInternal(directory);
        };

        this.hasMoreEntries = function(hash)
        {
            for (k in hash)
                return true;
            return false;
        };

        this.bind = function(callback, arg1, arg2, arg3)
        {
            var obj = this;
            return function(arg) {
                if (arg == undefined)
                    callback.apply(obj, [arg1, arg2, arg3]);
                else
                    callback.apply(obj, [arg, arg1, arg2, arg3]);
            };
        };

        this.removeDirectory = function(directory, parentDirectory)
        {
            if (directory.fullPath != '/') {
                // Ok to remove the parent directory.
                directory.remove(this.bind(this.entryRemovedCallback, parentDirectory, directory), this.bind(this.ErrorCallback));
            } else
                delete this.removeDirMap[directory.fullPath];
            if (!this.hasMoreEntries(this.removeDirMap) && this.successCallback)
                this.successCallback();
        };

        this.entryRemovedCallback = function(directory, entry)
        {
            if (entry.isDirectory)
                delete this.removeDirMap[entry.fullPath];

            if (directory) {
                var dirInfo = this.removeDirMap[directory.fullPath];
                if (--dirInfo.entries == 0 && dirInfo.hasMore == false)
                    this.removeDirectory(directory, dirInfo.parentDirectory);
            }
        };

        this.removeRecursivelyCallback = function(entries, directory)
        {
            for (var i = 0; i < entries.length; ++i) {
                this.removeDirMap[directory.fullPath].entries++;
                if (entries[i].isDirectory)
                    this.removeRecursivelyInternal(entries[i], directory);
                else {
                    var entry = entries[i];
                    entry.remove(this.bind(this.entryRemovedCallback, directory, entry), this.bind(this.errorCallback));
                }
            }
            if (entries.length) {
                this.removeDirMap[directory.fullPath].reader.readEntries(this.bind(this.removeRecursivelyCallback, directory), this.bind(this.errorCallback));
            } else {
                var dirInfo = this.removeDirMap[directory.fullPath];
                dirInfo.hasMore = false;
                if (dirInfo.entries == 0)
                    this.removeDirectory(directory, dirInfo.parentDirectory);
            }
        };

        this.removeRecursivelyInternal = function(directory, parentDirectory)
        {
            directoryReader = directory.createReader();
            this.removeDirMap[directory.fullPath] = {
                hasMore: true,
                parentDirectory: parentDirectory,
                entries: 0,
                reader: directoryReader,
            };
            directoryReader.readEntries(this.bind(this.removeRecursivelyCallback, directory), this.bind(this.errorCallback));
        };
    };

    var helper = new RemoveRecursiveHelper(successCallback, errorCallback);
    helper.removeRecursively(directory);
}
