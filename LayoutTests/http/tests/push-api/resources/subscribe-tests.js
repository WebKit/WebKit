const VALID_SERVER_KEY = "BA1Hxzyi1RUM1b5wjxsn7nGxAszw2u61m164i3MrAIxHF6YK5h4SDYic-dRuU_RCPCfA5aq9ojSwk5Y2EmClBPs";

async function testServiceWorkerPermissionState(registration, expected)
{
    let promise = new Promise(resolve => navigator.serviceWorker.onmessage = resolve);
    registration.active.postMessage(['permissionState']);
    let event = await promise;
    let result = event.data;

    if (result == expected)
        log(`PASS: service worker permissionState was ${expected}`);
    else
        log(`FAIL: service worker permissionState should be ${expected}, but was ${result}`);
}

async function testDocumentPermissionState(registration, expected)
{
    let state = await registration.pushManager.permissionState();

    if (state == expected)
        log(`PASS: document permissionState was ${expected}`);
    else
        log(`FAIL: document permissionState should be ${expected}, but was ${state}`);
}

async function testServiceWorkerSubscribe(registration, domExceptionName)
{
    let expected = domExceptionName ? `error: ${domExceptionName}` : "successful";

    let promise = new Promise(resolve => { navigator.serviceWorker.onmessage = resolve; });
    registration.active.postMessage(['subscribe', VALID_SERVER_KEY]);
    let event = await promise;
    let result = event.data;

    if (result == expected)
        log(`PASS: service worker subscribe was ${expected}`);
    else
        log(`FAIL: service worker subscribe should be ${expected}, but was ${result}`);
}

async function testDocumentSubscribe(registration, domExceptionName)
{
    let expected = domExceptionName ? `error: ${domExceptionName}` : "successful";
    let result = null;

    let subscription = null;
    try {
        // With default permissions, PushManager should request permission from TestController.
        // TestController always calls WKNotificationPermissionRequestAllow so this should succeed.
        subscription = await registration.pushManager.subscribe({
            userVisibleOnly: true,
            applicationServerKey: VALID_SERVER_KEY
        });
        result = 'successful';
    } catch (e) {
        // Layout tests aren't connected to webpushd, so subscribe is successful if it gets to the
        // point where we attempt to communicate with webpushd (an AbortError).
        if (e.name == 'AbortError')
            result = 'successful';
        else
            result = 'error: ' + (e ? e.name : null);
    }

    if (subscription)
        await subscription.unsubscribe();

    if (result == expected)
        log(`PASS: document subscribe was ${expected}`);
    else
        log(`FAIL: document subscribe should be ${expected}, but was ${result}`);
}