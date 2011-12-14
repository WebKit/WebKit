description("Test for building blobs with multiple strings combined by BlobBuilder and sending them via XMLHttpRequest.");

var util = new HybridBlobTestUtil(runTests);

function runAppendTest(appendItems, opt_ending, opt_urlParameter, opt_type)
{
    var blob = util.appendAndCreateBlob(appendItems, opt_ending, opt_type);
    var urlParameter = opt_urlParameter;
    if (!opt_urlParameter)
        urlParameter = util.createUrlParameter(appendItems, null, opt_ending);
    if (opt_type != undefined)
        shouldBe("'" + blob.type + "'", "'" + opt_type + "'");
    util.uploadBlob(blob, urlParameter);
}

function runTests()
{
    var stringA = util.DataItem('A1234567|');
    var stringB = util.DataItem('B12345|');
    var stringC = util.DataItem('C12345678901|');

    var stringLF = util.DataItem('12345\n67890\n');
    var stringCRLF = util.DataItem('ABCDE\r\nFGHIJ\r\n');
    var stringCR = util.DataItem('abcde\rfghij\r');

    debug('* BlobBuilder.append(string)');
    runAppendTest([ stringA ]);
    runAppendTest([ stringA, stringB ]);
    runAppendTest([ stringA, stringB ], null, null, 'type/foo');

    debug('* BlobBuilder.append(blob)');
    runAppendTest([ [ stringA ] ]);
    runAppendTest([ [ stringA, stringB, stringA ] ]);
    runAppendTest([ [ stringA, stringC ] ], null, null, 'type/bar');

    debug('* BlobBuilder.append(string/blob)');
    runAppendTest([ [ [ stringA, stringB ], stringA ], stringC ]);
    runAppendTest([ [ stringA, stringB, stringA ], stringC, [ stringA, stringB, stringA ] ]);

    debug('* BlobBuilder.append(string/blob) - with recycled blob');
    var mixedArray = [ [ stringA, stringB, stringA ], stringC, [ stringA, stringB ] , stringA ];
    var blob = { 'type':'blob', 'value':util.appendAndCreateBlob(mixedArray) };
    var parameter = util.createUrlParameter([ mixedArray, stringC, mixedArray ]);
    runAppendTest([ blob, stringC, blob ], null, parameter);

    debug('* BlobBuilder.append(string) - with line-endings');
    runAppendTest([ stringLF ]);
    runAppendTest([ stringCR ]);
    runAppendTest([ stringCRLF ]);

    debug('* BlobBuilder.append(string, "native") - with line-endings');
    runAppendTest([ stringLF ], 'native');
    runAppendTest([ stringCR ], 'native');
    runAppendTest([ stringCRLF ], 'native');
    runAppendTest([ stringCRLF, stringLF, stringCRLF ], 'native');
}

if (window.eventSender)
    util.runTests();
else
    testFailed("This test is not interactive, please run using DumpRenderTree");

var successfullyParsed = true;
