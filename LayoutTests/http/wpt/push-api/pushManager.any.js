// META: title=PushManager tests
// META: global=window,serviceworker

const INVALID_SERVER_KEY = new Uint8Array(new Array(65).fill(4));
const VALID_SERVER_KEY = new Uint8Array([4, 13, 71, 199, 60, 162, 213, 21, 12, 213, 190, 112, 143, 27, 39, 238, 113, 177, 2, 204, 240, 218, 238, 181, 155, 94, 184, 139, 115, 43, 0, 140, 71, 23, 166, 10, 230, 30, 18, 13, 136, 156, 249, 212, 110, 83, 244, 66, 60, 39, 192, 229, 170, 189, 162, 52, 176, 147, 150, 54, 18, 96, 165, 4, 251]);

const INVALID_BASE64_SERVER_KEY = "BAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQ";
const VALID_BASE64_SERVER_KEY = "BA1Hxzyi1RUM1b5wjxsn7nGxAszw2u61m164i3MrAIxHF6YK5h4SDYic-dRuU_RCPCfA5aq9ojSwk5Y2EmClBPs";

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
    return promise_rejects_dom(test, "NotAllowedError", pushManager.subscribe({
        userVisibleOnly: true,
        applicationServerKey: VALID_SERVER_KEY
    }));
}, "can subscribe with valid applicationServerKey buffer");

promise_test(async (test) => {
    // TODO: change this to make sure that subscription is valid once we fully implement subscribe.
    return promise_rejects_dom(test, "NotAllowedError", pushManager.subscribe({
        userVisibleOnly: true,
        applicationServerKey: VALID_BASE64_SERVER_KEY
    }));
}, "can subscribe with valid applicationServerKey string");

if (!isServiceWorker)
    promise_test((test) => registration.unregister(), "unregister service worker");    
