var HybridBlobTestUtil = function(testFunc, opt_filePaths) {
    this.testCallbackFunc = testFunc;

    this.testFilePaths = opt_filePaths;
    this.testFiles = [];
    this.testFileMap = {};
    this.fileInput = null;

    this.dragBaseDir = "";
    this.urlPathBaseDir = "";

    this.testUrl = "http://127.0.0.1:8000/resources/post-and-verify-hybrid.cgi";

    this.uploadBlob = function(blob, urlParameter)
    {
        var xhr = new XMLHttpRequest();
        xhr.open("POST", this.testUrl + "?" + urlParameter, false);

        var passed;
        var message;
        try {
            xhr.send(blob);
            if (xhr.responseText == "OK") {
                passed = true;
                message = "Expected response data received: " + xhr.responseText;
            } else {
                passed = false;
                message = "Unexpected response data received: " + xhr.responseText;
            }
        } catch (ex) {
            passed = false;
            message = "Unexpected exception thrown: " + ex;
        }

        if (passed)
            testPassed(message);
        else
            testFailed(message + ' params:' + urlParameter);
    };

    this.onInputFileChange = function()
    {
        var files = document.getElementById("file").files;
        this.testFiles = new Array();
        for (var i = 0; i < files.length; i++) {
            this.testFiles.push(files[i]);
            this.testFileMap[testFilePaths[i]] = files[i];
        }
        this.testCallbackFunc();
    };

    this.runTestsWithDrag = function()
    {
        this.fileInput = document.createElement("input");
        this.fileInput.id = "file";
        this.fileInput.type = "file";
        this.fileInput.style.width = "100px";
        this.fileInput.style.height = "100px";
        this.fileInput.multiple = true;
        var obj = this;
        this.fileInput.addEventListener("change", function() { obj.onInputFileChange() }, false);
        document.body.insertBefore(this.fileInput, document.body.firstChild);

        if (this.dragBaseDir) {
            var filePaths = [];
            for (var i = 0; i < this.testFilePaths.length; ++i) {
                filePaths.push(this.dragBaseDir + this.testFilePaths[i]);
            }
            this.testFilePaths = filePaths;
        }

        eventSender.beginDragWithFiles(this.testFilePaths);
        eventSender.mouseMoveTo(10, 10);
        eventSender.mouseUp();
    };

    this.runTests = function() {
        if (!this.testCallbackFunc)
            testFailed("Test body function is not initialized.");
        else if (this.testFilePaths)
            this.runTestsWithDrag();
        else
            this.testCallbackFunc();
    };

    this.FileItem = function(path)
    {
        var item = new Object();
        item.type = 'file';
        item.path = path;
        item.value = this.testFileMap[path];
        return item;
    }

    this.DataItem = function(data)
    {
        var item = new Object();
        item.type = 'data';
        item.value = data;
        return item;
    }

    this.createItemParamArray = function(items, itemParams, opt_ending)
    {
        for (var i = 0; i < items.length; i++) {
            if (items[i] instanceof Array)
                this.createItemParamArray(items[i], itemParams, opt_ending);
            else if (items[i].type == "data") {
                var data = items[i].value;
                if (opt_ending)
                    data = data.replace(/\r\n/g, "[NL]").replace(/[\n\r]/g, "[NL]");
                itemParams.push("data:" + escape(data));
            } else if (items[i].type == "file")
                itemParams.push("file:" + this.urlPathBaseDir + items[i].path);
        }
    };

    this.createUrlParameter = function(items, opt_range, opt_ending)
    {
        var itemParams = [];
        this.createItemParamArray(items, itemParams, opt_ending);
        var urlParameter = "items=" + itemParams.join(",");
        if (opt_range) {
            urlParameter += "&start=" + opt_range.start;
            urlParameter += "&length=" + opt_range.length;
        }
        if (opt_ending)
            urlParameter += "&convert_newlines=1";
        return urlParameter;
    };

    this.appendAndCreateBlob = function(items, opt_ending, opt_type)
    {
        var builder = [];
        for (var i = 0; i < items.length; i++) {
            if (items[i] instanceof Array) {
                // If it's an array, combine its elements and append as a blob.
                builder.push(this.appendAndCreateBlob(items[i], opt_ending, opt_type));
            } else
                builder.push(items[i].value);
        }
        var options = {};
        if (opt_ending)
            options.endings = opt_ending;
        if (opt_type)
            options.type = opt_type;
        return new Blob(builder, options);
    };
};
