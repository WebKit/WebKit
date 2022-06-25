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

done();
