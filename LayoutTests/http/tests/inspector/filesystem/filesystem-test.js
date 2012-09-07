var initialize_FileSystemTest = function()
{
    var nextCallbackId = 0;
    var callbacks = {};

    InspectorTest.MockEntry = function(url)
    {
        this.url = url;
    }

    InspectorTest.registerCallback = function(callback)
    {
        var callbackId = ++nextCallbackId;
        callbacks[callbackId] = callback;
        return "dispatchCallback.bind(null, " + callbackId + ")";
    };

    InspectorTest.dispatchCallback = function(packedArgs)
    {
        var args = JSON.parse(packedArgs);
        var callbackId = args.shift();
        var callback = callbacks[callbackId];
        delete callbacks[callbackId];
        callback.apply(null, args);
    };

    InspectorTest.createDirectory = function(path, callback)
    {
        InspectorTest.evaluateInPage("createDirectory(escape(\"" + escape(path) + "\"), " + InspectorTest.registerCallback(callback) + ")");
    };

    InspectorTest.createFile = function(path, callback)
    {
        InspectorTest.evaluateInPage("createFile(unescape(\"" + escape(path) + "\"), " + InspectorTest.registerCallback(callback) + ")");
    };

    InspectorTest.writeFile = function(path, content, callback)
    {
        InspectorTest.evaluateInPage("writeFile(unescape(\"" + escape(path) + "\"), unescape(\"" + escape(content) + "\"), " + InspectorTest.registerCallback(callback) + ")");
    };

    InspectorTest.clearFileSystem = function(callback)
    {
        InspectorTest.evaluateInPage("clearFileSystem(" + InspectorTest.registerCallback(callback) + ")");
    };

    InspectorTest.dumpFileSystemRootRequestResult = function(errorCode, backendRootEntry)
    {
        InspectorTest.addResult("errorCode: " + errorCode);
        if (backendRootEntry)
            InspectorTest.addResult("backendRootEntry.url: " + backendRootEntry.url);
        else
            InspectorTest.addResult("backendRootEntry: (null)");
        InspectorTest.addResult("");
    };

    InspectorTest.dumpDirectoryContentRequestResult = function(errorCode, entries)
    {
        InspectorTest.addResult("errorCode: " + errorCode);

        if (!entries) {
            InspectorTest.addResult("entries: (null)");
            InspectorTest.addResult("");
            return;
        }

        InspectorTest.addResult("entries:");
        for (var i = 0; i < entries.length; ++i) {
          InspectorTest.addResult("  " + i + ":");
          for (var j in entries[i])
            InspectorTest.addResult("    " + j + ": " + entries[i][j]);
        }
        InspectorTest.addResult("");
    };

    InspectorTest.dumpMetadataRequestResult = function(errorCode, metadata)
    {
        InspectorTest.addResult("errorCode: " + errorCode);
        if (metadata) {
            InspectorTest.addResult("metadata:");
            InspectorTest.addResult("  modificationTime: " + ("modificationTime" in metadata ? "(exists)" : "(null)"));
            InspectorTest.addResult("  size: " + ("size" in metadata ? metadata.size : "(null)"));
        } else {
            InspectorTest.addResult("metadata: (null)");
        }
        InspectorTest.addResult("");
    };

    InspectorTest.dumpFileContentRequestResult = function(errorCode, content, charset)
    {
        InspectorTest.addResult("errorCode: " + errorCode);

        if (content)
            InspectorTest.addResult("content: \"" + content + "\"");
        else
            InspectorTest.addResult("content: (null)");

        InspectorTest.addResult("charset: " + charset);
        InspectorTest.addResult("");
    };
}

function dispatchCallback()
{
    var args = JSON.stringify(Array.prototype.slice.call(arguments));
    testRunner.evaluateInWebInspector(999, "InspectorTest.dispatchCallback(unescape(\"" + escape(args) + "\"))");
}

function createDirectory(path, callback)
{
    webkitRequestFileSystem(TEMPORARY, 1, didGetFileSystem);

    function didGetFileSystem(fileSystem)
    {
        fileSystem.root.getDirectory(path, {create:true}, function(entry) {
            callback();
        });
    }
}

function createFile(path, callback)
{
    webkitRequestFileSystem(TEMPORARY, 1, didGetFileSystem);

    function didGetFileSystem(fileSystem)
    {
        fileSystem.root.getFile(path, {create:true}, function(entry) {
            callback();
        });
    }
}

function writeFile(path, content, callback)
{
    webkitRequestFileSystem(TEMPORARY, 1, didGetFileSystem);

    function didGetFileSystem(fileSystem)
    {
        fileSystem.root.getFile(path, {create:true}, didGetFileEntry);
    }

    function didGetFileEntry(fileEntry)
    {
        fileEntry.createWriter(didGetWriter);
    }

    function didGetWriter(writer)
    {
        writer.write(new Blob([content]));
        writer.onwrite = function() {
            if (writer.readyState === writer.DONE)
                callback();
        };
    }
}

function clearFileSystem(callback)
{
    webkitResolveLocalFileSystemURL("filesystem:" + location.origin + "/temporary/", didGetRoot, onError);

    function didGetRoot(root)
    {
        var reader = root.createReader();
        reader.readEntries(didReadEntries);

        var entries = [];
        function didReadEntries(newEntries)
        {
            if (newEntries.length === 0) {
                removeAll();
                return;
            }
            for (var i = 0; i < newEntries.length; ++i)
                entries.push(newEntries[i]);
            reader.readEntries(didReadEntries);
        }

        function removeAll()
        {
            if (entries.length === 0) {
                callback();
                return;
            }
            var entry = entries.shift();
            if (entry.isDirectory)
                entry.removeRecursively(removeAll);
            else
                entry.remove(removeAll);
        }
    }

    function onError()
    {
        // Assume the FileSystem is uninitialized and therefore empty.
        callback();
    }
}
