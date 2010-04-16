var window = {
    layoutTestController: {
        notifyDone: function() { postMessage("notifyDone"); }
    }
};
var layoutTestController = window.layoutTestController;

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
        window.layoutTestController.notifyDone();
    }
};

var DB_TEST_SUFFIX = "_worker";
