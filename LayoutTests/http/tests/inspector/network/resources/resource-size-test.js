TestPage.registerInitializer(function() {

    // For values expected to be zero, avoid logging the actual value if it non-zero
    // as that may be different across implementations and could produce dynamic
    // failing results. If the expected value is non-zero test it normally.
    InspectorTest.gracefulExpectEquals = function(actual, expected, property, message)
    {
        if (!expected) {
            if (actual === expected)
                InspectorTest.pass(`${property} should be exactly 0 bytes.`);
            else
                InspectorTest.fail(`${property} should be exactly 0 bytes.`);
        } else
            InspectorTest.expectEqual(actual, expected, message);
    }

    // Request headers should at least include a UserAgent.
    // Response headers should at least include a Content-Type.
    const minimumHeadersSize = 20;

    // Parameters:
    //   - debug: dump metrics if set.
    //   - url: if using the default resource loader this is the url to fetch()
    //   - resourceLoader: provides a resource to test, must return a promise
    //   - statusCode: expected resource status code value
    //   - compressed: expected resource compressed value
    //   - responseSource: expected resource response source to ensure test is testing the load type it expects
    //   - headers: whether or not headers are expected
    //   - requestBodyTransferSize, responseBodyTransferSize: exact body transfer sizes
    //   - size: exact decoded body size whether or not there was anything transferred
    //   - extraChecks: extra checks to perform
    window.addResourceSizeTest = function(suite, {name, description, debug, url, statusCode, compressed, responseSource, headers, requestBodyTransferSize, responseBodyTransferSize, size, resourceLoader, extraChecks}) {
        suite.addTestCase({
            name, description,
            test(resolve, reject) {
                let promise;
                if (resourceLoader)
                    promise = resourceLoader();
                else {
                    InspectorTest.evaluateInPage(`fetch(${JSON.stringify(url)})`);
                    promise = Promise.all([
                        WebInspector.Frame.awaitEvent(WebInspector.Frame.Event.ResourceWasAdded),
                        WebInspector.Resource.awaitEvent(WebInspector.Resource.Event.LoadingDidFinish),
                    ]).then(([resourceWasAddedEvent, loadCompleteEvent]) => {
                        return resourceWasAddedEvent.data.resource;
                    });
                }

                promise.then((resource) => {
                    InspectorTest.assert(resource instanceof WebInspector.Resource, "Resource should be created.");
                    InspectorTest.expectEqual(resource.statusCode, statusCode, `statusCode should be ${statusCode}.`);
                    InspectorTest.expectEqual(resource.compressed, compressed, `compressed should be ${compressed}.`);
                    InspectorTest.expectEqual(resource.responseSource, responseSource, `responseSource should be ${String(responseSource)}.`);

                    if (debug) {
                        InspectorTest.log("----");
                        InspectorTest.log("resource.requestHeadersTransferSize: " + resource.requestHeadersTransferSize);
                        InspectorTest.log("resource.responseHeadersTransferSize: " + resource.responseHeadersTransferSize);
                        InspectorTest.log("resource.requestBodyTransferSize: " + resource.requestBodyTransferSize);
                        InspectorTest.log("resource.responseBodyTransferSize: " + resource.responseBodyTransferSize);
                        InspectorTest.log("resource.cachedResponseBodySize: " + resource.cachedResponseBodySize);
                        InspectorTest.log("----");
                        InspectorTest.log("resource.size: " + resource.size);
                        InspectorTest.log("resource.networkEncodedSize: " + resource.networkEncodedSize);
                        InspectorTest.log("resource.networkDecodedSize: " + resource.networkDecodedSize);
                        InspectorTest.log("resource.networkTotalTransferSize: " + resource.networkTotalTransferSize);
                        InspectorTest.log("resource.estimatedNetworkEncodedSize: " + resource.estimatedNetworkEncodedSize);
                        InspectorTest.log("resource.estimatedTotalTransferSize: " + resource.estimatedTotalTransferSize);
                        InspectorTest.log("----");
                    }

                    // Calculated values. Try to be graceful, if the size is expected to be zero but is non-zero don't log the actual value.
                    InspectorTest.gracefulExpectEquals(resource.estimatedNetworkEncodedSize, responseBodyTransferSize, "estimatedNetworkEncodedSize", `estimatedNetworkEncodedSize should be exactly ${responseBodyTransferSize} bytes.`);
                    InspectorTest.expectGreaterThanOrEqual(resource.estimatedTotalTransferSize, responseBodyTransferSize + (headers ? minimumHeadersSize : 0), `estimatedTotalTransferSize should be >= (encoded body size + headers).`);

                    // Exact decoded size should always be available.
                    InspectorTest.gracefulExpectEquals(resource.size, size, "size", `size should be exactly ${size} bytes.`);

                    // Exact transfer sizes if available.
                    InspectorTest.expectEqual(resource.networkEncodedSize, responseBodyTransferSize, `networkEncodedSize should be exactly ${responseBodyTransferSize} bytes.`);
                    InspectorTest.expectGreaterThanOrEqual(resource.networkTotalTransferSize, responseBodyTransferSize + (headers ? minimumHeadersSize : 0), `networkTotalTransferSize should be >= (encoded body size + headers).`);
                    InspectorTest.expectEqual(resource.requestBodyTransferSize, requestBodyTransferSize, `requestBodyTransferSize should be exactly ${requestBodyTransferSize} bytes.`);
                    InspectorTest.expectEqual(resource.responseBodyTransferSize, responseBodyTransferSize, `responseBodyTransferSize should be exactly ${responseBodyTransferSize} bytes.`);

                    // Exact header sizes if available. May vary between implementations so we check if empty / non-empty.
                    if (headers) {
                        InspectorTest.expectGreaterThan(resource.requestHeadersTransferSize, minimumHeadersSize, `requestHeadersTransferSize should be non-empty.`);
                        InspectorTest.expectGreaterThan(resource.responseHeadersTransferSize, minimumHeadersSize, `responseHeadersTransferSize should be non-empty.`);
                    } else {
                        InspectorTest.expectEqual(resource.requestHeadersTransferSize, 0, `requestHeadersTransferSize should be empty.`);
                        InspectorTest.expectEqual(resource.responseHeadersTransferSize, 0, `responseHeadersTransferSize should be empty.`);
                    }

                    // Test may include extra checks.
                    if (extraChecks)
                        extraChecks(resource);
                }).then(resolve, reject);
            }
        });
    }
});
