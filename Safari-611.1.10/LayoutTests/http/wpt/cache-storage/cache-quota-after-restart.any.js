var response1 = new Response(new ArrayBuffer(1 * 1024));
var response30ko = new Response(new ArrayBuffer(30 * 1024));
var response400 = new Response(new ArrayBuffer(400 * 1024));
let cache;

promise_test(async (test) => {
    if (!window.testRunner)
        return Promise.reject("Test requires internals");

    testRunner.setAllowStorageQuotaIncrease(true);

    cache = await self.caches.open("temp1");
    await cache.put("400-v1", response400.clone());
    await cache.put("1", response1.clone());
    // quota should be equal to 801ko now.
    await cache.put("400-v2", response400.clone());

    testRunner.setAllowStorageQuotaIncrease(false);

    return promise_rejects_dom(test, "QuotaExceededError", cache.put("30ko", response30ko.clone()), "put should fail");
}, 'Increasing quota');

promise_test(async (test) => {
    if (!window.internals)
        return Promise.reject("Test requires internals");

    // Space used is around 801ko. After rounding, quota should be 840ko.
    internals.updateQuotaBasedOnSpaceUsage();

    return cache.put("30ko1", response30ko.clone());
}, 'After network process restart, verify quota is computed according space being used');

promise_test(async (test) => {
    if (!window.internals)
        return Promise.reject("Test requires internals");

    // Space used is around 831ko. After rounding, quota should be 840ko.
    internals.updateQuotaBasedOnSpaceUsage();

    return promise_rejects_dom(test, "QuotaExceededError", cache.put("30ko2", response30ko.clone()), "put should fail");
}, 'After network process restart, verify quota is computed according space being used and does not increase');

done();
