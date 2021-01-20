// META: script=/service-workers/cache-storage/resources/test-helpers.js
// META: script=/common/get-host-info.sub.js

var test_url = 'https://example.com/foo';
var test_body = 'Hello world!';

if (window.testRunner)
    testRunner.setAllowStorageQuotaIncrease(false);

function getResponseBodySizeWithPadding(response)
{
    var cache;
    var request = new Request("temp");
    var clone = response.clone();
    return self.caches.open("test").then((c) => {
        cache = c;
        return cache.put(request, clone);
    }).then(() => {
        return self.caches.delete("temp");
    }).then(() => {
        return window.internals.responseSizeWithPadding(clone);
    });
}

promise_test(() => {
    if (!window.internals)
        return Promise.reject("Test requires internals");

    var response = new Response("");
    return getResponseBodySizeWithPadding(response).then(size => {
        assert_equals(size, 0, "zero size synthetic response");
        return getResponseBodySizeWithPadding(response.clone());
    }).then((size) => {
        assert_equals(size, 0, "zero size synthetic cloned response");

        response = new Response("a");
        return getResponseBodySizeWithPadding(response);
    }).then((size) => {
        assert_equals(size, 1, "non zero size synthetic response");
        return getResponseBodySizeWithPadding(response.clone());
    }).then((size) => {
        assert_equals(size, 1, "non zero size synthetic cloned response");
    })
}, "Testing synthetic response body size padding");

promise_test(() => {
    if (!window.internals)
        return Promise.reject("Test requires internals");

    var paddedSize;
    var response, responseClone;
    return fetch("").then(r => {
        response = r;
        responseClone = response.clone();
        return getResponseBodySizeWithPadding(response);
    }).then((size) => {
        paddedSize = size;
        return response.arrayBuffer();
    }).then((buffer) => {
        assert_equals(buffer.byteLength, paddedSize, "non opaque network response");
        return getResponseBodySizeWithPadding(responseClone);
    }).then((size) => {
        assert_equals(size, paddedSize, "non opaque network cloned response");
    });
}, "Testing non opaque response body size padding");

promise_test(() => {
    if (!window.internals)
        return Promise.reject("Test requires internals");

    var actualSize, paddedSize;
    var response, responseClone1;
    return fetch(get_host_info().HTTP_REMOTE_ORIGIN, {mode: "no-cors"}).then(r => {
        response = r;
    }).then(() => {
        return fetch(get_host_info().HTTP_ORIGIN);
    }).then(r => {
        return r.arrayBuffer();
    }).then((buffer) => {
        actualSize = buffer.byteLength;
    }).then(() => {
        responseClone1 = response.clone();
        return getResponseBodySizeWithPadding(response);
    }).then((size) => {
        paddedSize = size;
        return getResponseBodySizeWithPadding(responseClone1);
    }).then((size) => {
        assert_not_equals(size, actualSize, "padded size should be different from actual size");
        assert_equals(size, paddedSize, "opaque network cloned response");
    });
}, "Testing opaque response body size padding");

async function doCleanup()
{
    var cachesKeys = await self.caches.keys();
    for (let name of cachesKeys) {
        let cache = await self.caches.open(name);
        let keys = await cache.keys();
        for (let key of keys)
            await cache.delete(key);
    }
}

promise_test((test) => {
    var cache;
    var response1ko = new Response(new ArrayBuffer(1 * 1024));
    var response399ko = new Response(new ArrayBuffer(399 * 1024));

    return doCleanup().then(() => {
        return self.caches.open("temp1");
    }).then((c) => {
        cache = c;
        return cache.put("399ko", response399ko.clone());
    }).then(() => {
        return cache.put("1ko-v1", response1ko.clone());
    }).then(() => {
        return cache.put("1ko-v2", response1ko.clone()).then(assert_unreached, (e) => {
            assert_equals(e.name, "QuotaExceededError");
        });
    }).then(() => {
        return cache.delete("1ko-v1");
    }).then(() => {
        return cache.put("1ko-v2", response1ko.clone());
    }).then(() => {
        return cache.delete("399ko");
    }).then(() => {
        return cache.delete("1ko-v1");
    }).then(() => {
        return cache.delete("1ko-v2");
    });
}, 'Hitting cache quota for non opaque responses');

promise_test((test) => {
    if (!window.internals || !window.testRunner)
        return Promise.reject("Test requires internals");

    var cache;
    var response1ko = new Response(new ArrayBuffer(1 * 1024));
    var responsePadded = new Response(new ArrayBuffer(1 * 1024));
    var response200ko = new Response(new ArrayBuffer(200 * 1024));

    return doCleanup().then(() => {
        return self.caches.open("temp2");
    }).then((c) => {
        cache = c;
        return fetch(get_host_info().HTTP_REMOTE_ORIGIN, {mode: "no-cors"});
    }).then((r) => {
        responsePadded = r;
        internals.setResponseSizeWithPadding(responsePadded, 200 * 1024);
        return cache.put("200ko", response200ko.clone());
    }).then(() => {
        return cache.put("1ko-padded-to-200ko", responsePadded.clone());
    }).then(() => {
        return cache.put("1ko", response1ko.clone()).then(assert_unreached, (e) => {
            assert_equals(e.name, "QuotaExceededError");
        });
    }).then(() => {
        testRunner.setAllowStorageQuotaIncrease(true);
        return cache.put("1ko", response1ko.clone());
    }).then(() => {
        return cache.delete("1ko-padded-to-200ko");
    }).then(() => {
        return cache.put("1ko-v2", response1ko.clone());
    }).then(() => {
        return cache.put("1ko-v3", response1ko.clone());
    }).then(() => {
        return cache.delete("200ko");
    }).then(() => {
        return cache.delete("1ko-v2");
    }).then(() => {
        return cache.delete("1ko-v3");
    });
}, 'Hitting cache quota for padded responses');

done();
