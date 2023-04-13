async function doTest(test, scope, message)
{
    var registration = await navigator.serviceWorker.getRegistration(scope);
    if (registration && registration.scope === scope)
        await registration.unregister();

    var registration = await navigator.serviceWorker.register("push-console-messages-worker.js", { scope : scope });
    activeWorker = registration.active;
    assert_equals(registration.active, null);

    activeWorker = registration.installing;
    await new Promise(resolve => {
        activeWorker.addEventListener('statechange', () => {
            if (activeWorker.state === "activated")
                resolve();
        });
    });
    const promise = new Promise((resolve, reject) => {
        navigator.serviceWorker.addEventListener("message", (event) => {
            resolve(event.data);
        });
        setTimeout(() => reject("did not get message early enough"), 5000);
    });

    activeWorker.postMessage({type:"PUSHEVENT", message});
    assert_equals(await promise, "PASS");
    await new Promise(resolve => setTimeout(resolve, 100));
}
