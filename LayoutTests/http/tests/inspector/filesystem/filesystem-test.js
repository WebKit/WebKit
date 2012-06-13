var initialize_FileSystemTest = function()
{
    var nextCallbackId = 0;
    var callbacks = {};

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

    InspectorTest.dumpReadDirectoryResult = function(requestId, errorCode, entries)
    {
        InspectorTest.addResult("requestId: " + requestId);
        InspectorTest.addResult("errorCode: " + errorCode);

        if (!entries) {
            InspectorTest.addResult("entries: (null)");
            return;
        }

        InspectorTest.addResult("entries:");
        for (var i = 0; i < entries.length; ++i) {
          InspectorTest.addResult("  " + i + ":");
          for (var j in entries[i])
            InspectorTest.addResult("    " + j + ": " + entries[i][j]);
        }
    };
}

function dispatchCallback()
{
    var args = JSON.stringify(Array.prototype.slice.call(arguments));
    layoutTestController.evaluateInWebInspector(0, "InspectorTest.dispatchCallback(unescape(\"" + escape(args) + "\"))");
}

function createDirectory(path, callback)
{
    webkitRequestFileSystem(TEMPORARY, 1, function(fs) {
        fs.root.getDirectory(path, {create:true}, function(entry) {
            callback();
        });
    });
}

function createFile(path, callback)
{
    webkitRequestFileSystem(TEMPORARY, 1, function(fs) {
        fs.root.getFile(path, {create:true}, function(entry) {
            callback();
        });
    });
}
