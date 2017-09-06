// META: script=/service-workers/cache-storage/resources/test-helpers.js

var test_url = 'https://example.com/foo';
var test_body = 'Hello world!';

cache_test(function(cache) {
    var stream = new ReadableStream();
    stream.getReader();
    var response = new Response(stream);
    return cache.put(new Request(''), response).then(assert_unreached, (e) => {
        assert_throws(new TypeError, function() { throw e });
    });
}, 'Cache.put should throw if response stream is locked')

cache_test(function(cache) {
    var stream = new ReadableStream({start: (c) => {
        c.enqueue(new Uint8Array(1));
        c.enqueue(new Uint8Array(1));
        c.close();
    }});
    var reader = stream.getReader();
    return reader.read().then(() => {
        reader.releaseLock();
        var response = new Response(stream);
        return cache.put(new Request(''), response).then(assert_unreached, (e) => {
            assert_throws(new TypeError, function() { throw e });
        });
    });
}, 'Cache.put should throw if response stream is disturbed')

done();
