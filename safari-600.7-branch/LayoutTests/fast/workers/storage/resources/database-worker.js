var window = {
    testRunner: {
        notifyDone: function() { postMessage("notifyDone"); }
    }
};
var testRunner = window.testRunner;

function log(s) {
    postMessage("log:" + s);
}

onmessage = function(event) {
    try {
        if (event.data.indexOf("importScripts:") == 0) {
            var scripts = event.data.substring("importScripts:".length).split(",");
            for (var i in scripts)
                scripts[i] = "../" + scripts[i];
            importScripts(scripts);
        } else if (event.data == "runTest")
            runTest(); // Must be defined by some imported script.
        else
            log("Received unexpected message: " + event.data);
    } catch (ex) {
        log("Worker caught exception: " + ex);
        window.testRunner.notifyDone();
    }
};

var DB_TEST_SUFFIX = "_worker";

function openDatabaseWithSuffix(name, version, description, size, callback)
{
    if (arguments.length > 4) {
        return openDatabase(name + DB_TEST_SUFFIX, version, description, size, callback);
    } else {
        return openDatabase(name + DB_TEST_SUFFIX, version, description, size);
    }
}

