function checkRegistration(registration, expected, name)
{
    if (registration === expected) {
        log("PASS: " + name + " is as expected");
        return;
    }
    if (registration === null) {
        log("FAIL: " + name + " is null");
        return;
    }

    if (expected === null) {
        log("FAIL: " + name + " is not null");
        return;
    }
    log("FAIL: " + name + " is not as expected. Its scope is " + registration.scope + " expected scope is " + expected.scope);
}

async function test()
{
    try {
        log("Registering service worker 0");
        var newRegistration0 = await navigator.serviceWorker.register("resources/service-worker-getRegistration-worker.js", { scope: "/test1" });
        log("Service worker 0 registered with scope " + newRegistration0.scope);

        var registration0 = await navigator.serviceWorker.getRegistration();
        checkRegistration(registration0, undefined, "registration0");

        var registration1 = await navigator.serviceWorker.getRegistration("/test1");
        checkRegistration(registration1, newRegistration0, "registration1");

        var registration2 = await navigator.serviceWorker.getRegistration("/test");
        checkRegistration(registration2, undefined, "registration2");

        var registration3 = await navigator.serviceWorker.getRegistration("/test1/test2/test3");
        checkRegistration(registration3, newRegistration0, "registration3");

        log("Registering service worker 1");
        var newRegistration1 = await navigator.serviceWorker.register("resources/service-worker-getRegistration-worker.js", { scope: "/test1/test2" });
        log("Service worker 1 registered with scope " + newRegistration1.scope);

        log("Registering service worker 2");
        var newRegistration2 = await navigator.serviceWorker.register("resources/service-worker-getRegistration-worker.js");
        log("Service worker 2 registered with scope " + newRegistration2.scope);

        var registration4 = await navigator.serviceWorker.getRegistration("/test1/test2");
        checkRegistration(registration4, newRegistration1, "registration4");

        var registration5 = await navigator.serviceWorker.getRegistration("/test1/test2-test3");
        checkRegistration(registration5, newRegistration1, "registration5");

        var registration6 = await navigator.serviceWorker.getRegistration();
        checkRegistration(registration6, newRegistration2, "registration6");

        var registration7 = await navigator.serviceWorker.getRegistration("");
        checkRegistration(registration7, newRegistration2, "registration7");

        var registration8 = await navigator.serviceWorker.getRegistration("foo");
        checkRegistration(registration8, newRegistration2, "registration8");

        var registration9 = await navigator.serviceWorker.getRegistration("/foo");
        checkRegistration(registration9, undefined, "registration9");

        await newRegistration1.unregister();
        var registration10 = await navigator.serviceWorker.getRegistration("/test1/test2");
        checkRegistration(registration10, newRegistration0, "registration10");
    } catch(e) {
        log("Got exception: " + e);
    }
    finishSWTest();
}

test();
