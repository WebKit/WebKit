if (this.importScripts) {
    importScripts('fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('file-writer-utils.js');
    function gc() {
        if (typeof GCController !== "undefined")
            GCController.collect();
        else {
            function gcRec(n) {
                if (n < 1)
                    return {};
                var temp = {i: "ab" + i + (i / 100000)};
                temp += "foo";
                gcRec(n-1);
            }
            for (var i = 0; i < 1000; i++)
                gcRec(10)
        }
    }

}

description("Test that a blob won't get garbage-collected while being written out by a FileWriter.");

var fileEntry;

function onTestSuccess() {
    testPassed("Successfully wrote blob.");
    cleanUp();
}

function tenXBlob(blob) {
    var bb = new WebKitBlobBuilder();
    for (var i = 0; i < 10; ++i) {
        bb.append(blob);
    }
    return bb.getBlob();
}

function startWrite(writer) {
    // Let's make it about a megabyte.
    var bb = new WebKitBlobBuilder();
    bb.append("lorem ipsum");
    var blob = tenXBlob(bb.getBlob());
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    writer.onerror = onError;
    writer.onwriteend = onTestSuccess;
    writer.write(blob);
}

function runTest(unusedFileEntry, fileWriter) {
    startWrite(fileWriter);
    gc();
}
var jsTestIsAsync = true;
setupAndRunTest(2*1024*1024, 'file-writer-gc-blob', runTest);
