description("Test for slicing blobs created by BlobBuilder sending them via XMLHttpRequest.");

var util = new HybridBlobTestUtil(runTests);

function runSliceTest(appendItems, ranges)
{
    var blob = util.appendAndCreateBlob(appendItems);
    var range = {'start':0, 'length':-1};
    for (var i = 0; i < ranges.length; ++i) {
        blob = blob.slice(ranges[i].start, ranges[i].start + ranges[i].length);
        range.start += ranges[i].start;
    }
    range.length = ranges[ranges.length - 1].length;
    var urlParameter = util.createUrlParameter(appendItems, range);
    util.uploadBlob(blob, urlParameter);
}

function runTests()
{
    var stringA = util.DataItem('A1234567|');
    var stringB = util.DataItem('B12345|');
    var stringC = util.DataItem('C12345678901|');

    var mixedArray = [ [ stringA, stringB, stringA ], stringC, [ stringA, stringB ] , stringA ];

    debug('* BlobBuilder.append(data) and then apply Blob.slice()');
    runSliceTest(stringA, [ {'start': 3,  'length': 12} ]);
    runSliceTest([stringA, stringB], [ {'start': 3,  'length': 12} ]);
    runSliceTest(mixedArray, [ {'start': 13, 'length': 40} ]);
    runSliceTest(mixedArray, [ {'start': 13, 'length': 40} ]);
    runSliceTest(mixedArray, [ {'start': 5,  'length': 32} ]);

    debug('* BlobBuilder.append(data) and then apply nested Blob.slice()');
    runSliceTest(mixedArray, [ {'start': 5,  'length': 32}, {'start': 2, 'length': 9} ]);
    runSliceTest(mixedArray, [ {'start': 13, 'length': 40}, {'start': 5, 'length': 17} ]);
}

if (window.eventSender)
    util.runTests();
else
    testFailed("This test is not interactive, please run using DumpRenderTree");

var successfullyParsed = true;
