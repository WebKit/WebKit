description("Test for building blobs with multiple files combined by BlobBuilder and sending them via XMLHttpRequest.  (This test requires eventSender.beginDragWithFiles)");

var testFilePaths = [
    "nonexistent",
    "empty.txt",
    "file-for-drag-to-send.txt",
    "file-for-drag-to-send2.txt",
    "abe.png"
];

var util = new HybridBlobTestUtil(runTests, testFilePaths);
util.dragBaseDir = "../resources/";
util.urlPathBaseDir = "../local/resources/";

function runHybridBlobTest(fileIndexOrStrings, opt_range)
{
    var blob = util.appendAndCreateBlob(fileIndexOrStrings);
    var urlParameter = util.createUrlParameter(fileIndexOrStrings, opt_range);
    if (opt_range)
        blob = blob.slice(opt_range.start, opt_range.start + opt_range.length);
    util.uploadBlob(blob, urlParameter);
}

function runTests()
{
    // Syntax sugar.
    var F = function(path) { return util.FileItem(path); }
    var D = function(data) { return util.DataItem(data); }

    debug('* BlobBuilder.append(file) - single file');
    for (var i = 0; i < testFilePaths.length; i++)
        runHybridBlobTest(F(testFilePaths[i]));

    debug('* BlobBuilder.append(file) - multiple files');
    runHybridBlobTest([ F('nonexistent'),
                        F('empty.txt')]);
    runHybridBlobTest([ F('empty.txt'),
                        F('file-for-drag-to-send.txt') ]);
    runHybridBlobTest([ F('empty.txt'),
                        F('file-for-drag-to-send.txt'),
                        F('file-for-drag-to-send2.txt') ]);
    runHybridBlobTest([ F('file-for-drag-to-send2.txt'),
                        F('file-for-drag-to-send2.txt') ]);

    debug('* BlobBuilder.append(mixed)');
    runHybridBlobTest([ [ F('empty.txt') ] ]);
    runHybridBlobTest([ [ F('empty.txt'),
                          F('file-for-drag-to-send.txt') ],
                        F('file-for-drag-to-send2.txt') ]);
    runHybridBlobTest([ D('abcde'),
                        F('file-for-drag-to-send2.txt'),
                        D('|123|456|') ]);
    runHybridBlobTest([ F('empty.txt'),
                        D('13579'),
                       [ F('file-for-drag-to-send2.txt'),
                         F('file-for-drag-to-send.txt') ],
                        F('file-for-drag-to-send.txt'),
                        D('A_B_C_D_E')]);

    debug('* BlobBuilder.append(mixed) - with Blob.slice()');
    runHybridBlobTest([ F('file-for-drag-to-send.txt') ],
                      { 'start': 5, 'length': 10 });
    runHybridBlobTest([ [ F('file-for-drag-to-send2.txt') ] ],
                      { 'start': 3, 'length': 17 });
    runHybridBlobTest([ F('file-for-drag-to-send.txt'),
                        F('file-for-drag-to-send2.txt') ],
                      { 'start': 3, 'length': 17 });
    runHybridBlobTest([ F('file-for-drag-to-send2.txt'),
                        D('abcde'),
                        F('file-for-drag-to-send2.txt') ],
                      { 'start': 3, 'length': 17 });
    runHybridBlobTest([ F('file-for-drag-to-send2.txt'),
                        D('abcde'),
                        F('file-for-drag-to-send2.txt') ],
                      { 'start': 33, 'length': 90 });
    runHybridBlobTest([ F('file-for-drag-to-send2.txt'),
                        F('file-for-drag-to-send.txt'),
                        F('file-for-drag-to-send2.txt') ],
                      { 'start': 33, 'length': 42 });
    runHybridBlobTest([ F('empty.txt'),
                        F('file-for-drag-to-send.txt'),
                        F('file-for-drag-to-send2.txt'),
                        F('abe.png') ],
                      { 'start': 20, 'length': 3000 });

    testRunner.notifyDone();
}

if (window.eventSender) {
    testRunner.waitUntilDone();
    util.runTests();
} else
    testFailed("This test is not interactive, please run using DumpRenderTree");

var successfullyParsed = true;
