// META: script=/common/gc.js

if (window.testRunner)
    testRunner.setAllowStorageQuotaIncrease(false);

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

promise_test(async (test) => {
    const cache = await self.caches.open("add");
    const response399ko = new Response(new ArrayBuffer(399 * 1024));
    await cache.put("test", response399ko);
    return promise_rejects_dom(test, "QuotaExceededError", cache.add("../preload/resources/square.png"));
}, "Testing that cache.add checks against quota");

promise_test(async (test) => {
    await doCleanup();
    const cache = await self.caches.open("add2");
    const response380ko = new Response(new ArrayBuffer(380 * 1024));
    await cache.put("test", response380ko);
    return promise_rejects_dom(test, "QuotaExceededError", cache.addAll(["../preload/resources/square.png?", "../preload/resources/square.png?1", "../preload/resources/square.png?2"]));
}, "Testing that cache.addAll checks against quota");

promise_test(async (test) => {
    await doCleanup();
    // Fill cache storage with 399ko data.
    for (let i = 0; i < 399; i ++) {
        let response = new Response(new ArrayBuffer(1024));
        let cache = await caches.open("add_"+i);
        await cache.put("test", response);
        await caches.delete("add_"+i);
    }

    await garbageCollect();
    var newCache = await caches.open("add3");
    var retryAttempts = 10;
    var putSucceeded = false;
    while (retryAttempts-- && !putSucceeded) {
        // If any Cache object is collected, requesting to store (1ko + 1) data should succeed.
        let response = new Response(new ArrayBuffer(1025));
        await newCache.put("test", response).then(() => {
            putSucceeded = true;
        }).catch((e) => {
            assert_equals(e.name, "QuotaExceededError");
        });
        if (!putSucceeded)
            await new Promise(resolve => test.step_timeout(resolve, 100));
    }
    assert_true(putSucceeded);
}, "Testing that caches.delete will reduce usage");

done();
