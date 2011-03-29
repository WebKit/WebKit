function bindCallback(obj, callback, arg1, arg2, arg3)
{
    return function(arg) {
        if (arg == undefined)
            callback.apply(obj, [arg1, arg2, arg3]);
        else
            callback.apply(obj, [arg, arg1, arg2, arg3]);
    };
}

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
