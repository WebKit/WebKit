module("Builder");

test("getNumberOfFailingTests from Leaks bot", 4, function() {
    var mockBuildbot = {};
    mockBuildbot.baseURL = 'http://example.com/';

    var builder = new Builder('test builder', mockBuildbot);

    var realGetResource = window.getResource;
    window.getResource = function(url, successCallback, errorCallback) {
        var mockXHR = {};
        mockXHR.responseText = JSON.stringify({
            steps: [
                {
                    name: 'layout-test',
                    isStarted: true,
                    results: [
                        2,
                        [
                            "2178 total leaks found!", 
                            "2 test cases (<1%) had incorrect layout", 
                            "2 test cases (<1%) crashed",
                        ],
                    ],
                },
            ],
        });

        successCallback(mockXHR);
    };

    // FIXME: It's lame to be modifying singletons like this. We should get rid
    // of our singleton usage entirely!
    var realPersistentCache = window.PersistentCache;
    window.PersistentCache = {
        contains: function() { return false; },
        set: function() { },
        get: function() { },
    };

    builder.getNumberOfFailingTests(1, function(failureCount, tooManyFailures) {
        equals(failureCount, 4);
        equals(tooManyFailures, false);
    });

    window.getResource = realGetResource;
    equals(window.getResource, realGetResource);
    window.PersistentCache = realPersistentCache;
    equals(window.PersistentCache, realPersistentCache);
});
