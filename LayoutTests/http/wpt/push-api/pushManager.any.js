// META: title=PushManager tests
// META: global=window,serviceworker
// META: script=constants.js

let registration = null;
let pushManager = null;

let isServiceWorker = 'ServiceWorkerGlobalScope' in self && self instanceof self.ServiceWorkerGlobalScope;
if (isServiceWorker) {
    promise_test(async (test) => {
        registration = self.registration;
        pushManager = registration.pushManager;

        assert_true(!registration.active, "service worker should not yet be active");

        return promise_rejects_dom(test, "InvalidStateError", registration.pushManager.subscribe({
            userVisibleOnly: true,
            applicationServerKey: VALID_BASE64_SERVER_KEY
        }));
    }, "subscribe should fail if there is no active service worker");

    promise_test(async (test) => {
        if (!registration.active)
            return new Promise(resolve => self.addEventListener('activate', resolve));
    }, "wait for active service worker");
} else {
    promise_test(async (test) => {
        registration = await navigator.serviceWorker.getRegistration();
        if (registration) {
            await serviceWorkerRegistration.unregister();
        }
        registration = await navigator.serviceWorker.register("emptyWorker.js");
        pushManager = registration.pushManager;

        let serviceWorker = registration.installing;
        assert_true(!!serviceWorker, "service worker should be installing");

        return promise_rejects_dom(test, "InvalidStateError", registration.pushManager.subscribe({
            userVisibleOnly: true,
            applicationServerKey: VALID_BASE64_SERVER_KEY
        }));
    }, "subscribe should fail if there is no active service worker");

    promise_test(async (test) => {
        if (!registration.active) {
            let serviceWorker = registration.installing || registration.waiting;
            assert_true(!!serviceWorker, "registration should be associated with a service worker");

            return new Promise(resolve => {
                serviceWorker.addEventListener("statechange", () => {
                    if (serviceWorker.state === "activated")
                        resolve();
                });
            });
        }
    }, "wait for active service worker");
}

test((test) => assert_true(PushManager.supportedContentEncodings.includes("aes128gcm")), "aes128gcm should be supported");
test((test) => assert_true(Object.isFrozen(PushManager.supportedContentEncodings)), "supportedContentEncodings should be frozen");

promise_test(async (test) => {
    assert_equals(pushManager, registration.pushManager);
}, "pushManager should return same object");

promise_test(async (test) => {
    return promise_rejects_dom(test, "NotAllowedError", pushManager.subscribe({
        userVisibleOnly: false,
        applicationServerKey: VALID_BASE64_SERVER_KEY
    }));
}, "subscribe requires userVisibleOnly to be true");

promise_test(async (test) => {
    return promise_rejects_dom(test, "NotSupportedError", pushManager.subscribe({
        userVisibleOnly: true
    }));
}, "subscribe requires applicationServerKey");

promise_test(async (test) => {
    return promise_rejects_dom(test, "InvalidCharacterError", pushManager.subscribe({
        userVisibleOnly: true,
        applicationServerKey: VALID_BASE64_SERVER_KEY + "/"
    }));
}, "applicationServerKey string should be base64url-encoded");

promise_test(async (test) => {
    return promise_rejects_dom(test, "InvalidAccessError", pushManager.subscribe({
        userVisibleOnly: true,
        applicationServerKey: INVALID_SERVER_KEY
    }));
}, "applicationServerKey buffer should be a valid point on the P-256 curve");

promise_test(async (test) => {
    return promise_rejects_dom(test, "InvalidAccessError", pushManager.subscribe({
        userVisibleOnly: true,
        applicationServerKey: INVALID_BASE64_SERVER_KEY
    }));
}, "applicationServerKey string should be a valid point on the P-256 curve");

promise_test(async (test) => {
    // TODO: change this to make sure that subscription is valid once we fully implement subscribe.
    return promise_rejects_dom(test, "AbortError", pushManager.subscribe({
        userVisibleOnly: true,
        applicationServerKey: VALID_SERVER_KEY
    }));
}, "can subscribe with valid applicationServerKey buffer");

promise_test(async (test) => {
    // TODO: change this to make sure that subscription is valid once we fully implement subscribe.
    return promise_rejects_dom(test, "AbortError", pushManager.subscribe({
        userVisibleOnly: true,
        applicationServerKey: VALID_BASE64_SERVER_KEY
    }));
}, "can subscribe with valid applicationServerKey string");

if (!isServiceWorker)
    promise_test((test) => registration.unregister(), "unregister service worker");    
