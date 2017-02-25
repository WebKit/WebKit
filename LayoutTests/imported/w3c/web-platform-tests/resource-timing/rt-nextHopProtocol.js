promise_test(function(t) {
    return loadResources(1).then(function([entry]) {
        assert_equals(entry.nextHopProtocol, "http/1.1");
    });
}, "nextHopProtocol is expected to be 'http1/1'", {timeout: 3000});
