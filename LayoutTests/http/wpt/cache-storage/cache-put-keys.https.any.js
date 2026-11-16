// META: script=/service-workers/cache-storage/resources/test-helpers.js

var test_url = 'https://example.com/foo';
var test_body = 'Hello world!';

cache_test(function(cache) {
    var cache_keys;
    var alternate_response_body = 'New body';
    var alternate_response = new Response(alternate_response_body, { statusText: 'New status' });
    return cache.put(new Request(test_url), new Response('Old body', { statusText: 'Old status' })).then(function() {
        return cache.keys();
    }).then(function(keys) {
        cache_keys = keys;
    }).then(function() {
        return cache.put(new Request(test_url), alternate_response);
    }).then(function() {
        return cache.keys();
    }).then(function(keys) {
        assert_request_array_equals(keys, cache_keys);
    }).then(function() {
        return cache.match(test_url);
    }).then(function(result) {
        if (self.internals && self.internals.fetchResponseSource)
            assert_equals(internals.fetchResponseSource(result), "DOM cache");
        assert_response_equals(result, alternate_response, 'Cache.put should replace existing ' + 'response with new response.');
        return result.text();
    }).then(function(body) {
        assert_equals(body, alternate_response_body, 'Cache put should store new response body.');
    });
}, 'Cache.put called twice with matching Requests - keys should remain the same');

done();
