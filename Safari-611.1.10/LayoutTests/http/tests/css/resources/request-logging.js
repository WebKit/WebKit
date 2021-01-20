var ResourceLogging = {
    CallCommand: function(cmd)
    {
        try {
            var req = new XMLHttpRequest;
            req.open("GET", "/resources/network-simulator.php?test=" + this.testName + "&command=" + cmd, false);
            req.send(null);
            return req.responseText;
        } catch (ex) {
            return "";
        }
    },

    startTest: function()
    {
        if (window.testRunner) {
            testRunner.dumpAsText();
            testRunner.waitUntilDone();
        }
 
        window.setTimeout(this.endTest.bind(this), 0);
    },

    endTest: function()
    {
        this.getResourceLog();
        this.CallCommand("clear-resource-request-log");

        if (window.testRunner)
            testRunner.notifyDone();
    },

    getResourceLog: function()
    {
        var log = this.CallCommand("get-resource-request-log");
        var logLines = log.split('\n');
        logLines.sort();
        document.getElementById('result').innerText = logLines.join('\n');
    },

    start: function(testName)
    {
        this.testName = testName;
        this.CallCommand("start-resource-request-log");
        window.addEventListener('load', this.startTest.bind(this), false);
    },
};
