// META: title=PushSubscription tests
// META: global=window

const endpoint = 'https://www.webkit.org';
const expirationTime = 1633000000;
const key1Buffer = new Uint8Array([4, 13, 71, 199, 60, 162, 213, 21, 12, 213, 190, 112, 143, 27, 39, 238, 113, 177, 2, 204, 240, 218, 238, 181, 155, 94, 184, 139, 115, 43, 0, 140, 71, 23, 166, 10, 230, 30, 18, 13, 136, 156, 249, 212, 110, 83, 244, 66, 60, 39, 192, 229, 170, 189, 162, 52, 176, 147, 150, 54, 18, 96, 165, 4, 251]).buffer;
const key1Base64URL = 'BA1Hxzyi1RUM1b5wjxsn7nGxAszw2u61m164i3MrAIxHF6YK5h4SDYic-dRuU_RCPCfA5aq9ojSwk5Y2EmClBPs';
const key2Buffer = new Uint8Array([4, 37, 113, 178, 190, 205, 253, 227, 96, 85, 26, 175, 30, 208, 244, 205, 54, 108, 17, 206, 190, 85, 95, 137, 188, 183, 177, 134, 165, 51, 57, 23, 49, 104, 236, 226, 235, 224, 24, 89, 123, 211, 4, 121, 184, 110, 60, 143, 142, 206, 213, 119, 202, 89, 24, 126, 146, 70, 153, 13, 182, 130, 0, 139, 14]).buffer;
const key2Base64URL = 'BCVxsr7N_eNgVRqvHtD0zTZsEc6-VV-JvLexhqUzORcxaOzi6-AYWXvTBHm4bjyPjs7Vd8pZGH6SRpkNtoIAiw4';
const authBuffer = new Uint8Array([5, 48, 89, 50, 161, 199, 234, 190, 19, 182, 206, 201, 253, 164, 136, 130]).buffer;
const authBase64URL = 'BTBZMqHH6r4Tts7J_aSIgg';

function bytesFrom(buf) {
    return Array.from(new Uint8Array(buf));
}

let registration = null;

promise_test(async (test) => {
    registration = await navigator.serviceWorker.getRegistration();
    if (registration) {
        await serviceWorkerRegistration.unregister();
    }
    registration = await navigator.serviceWorker.register("emptyWorker.js");
    assert_true(!registration.active, "service worker should be installing");

    let serviceWorker = registration.installing || registration.waiting;
    assert_true(!!serviceWorker, "registration should be associated with a service worker");

    return new Promise(resolve => {
        serviceWorker.addEventListener("statechange", () => {
            if (serviceWorker.state === "activated")
                resolve();
        });
    });
}, "wait for active service worker");

promise_test(() => {
    assert_true(!!window.internals, "test requires internals");

    let subscription = window.internals.createPushSubscription(registration, endpoint, expirationTime, key1Buffer, key2Buffer, authBuffer);
    assert_true(subscription instanceof PushSubscription);

    assert_equals(subscription.endpoint, endpoint);
    assert_equals(subscription.expirationTime, expirationTime);
    assert_true(subscription.options === subscription.options, "options should always return same object");
    assert_equals(subscription.options.userVisibleOnly, true);
    assert_true(subscription.options.applicationServerKey === subscription.options.applicationServerKey, "applicationServerKey should return same object");
    assert_array_equals(bytesFrom(subscription.options.applicationServerKey), bytesFrom(key1Buffer));
    assert_array_equals(bytesFrom(subscription.getKey('p256dh')), bytesFrom(key2Buffer));
    assert_array_equals(bytesFrom(subscription.getKey('auth')), bytesFrom(authBuffer));

    obj = subscription.toJSON();
    assert_equals(Object.keys(obj).length, 3);
    assert_equals(obj.endpoint, endpoint);
    assert_equals(obj.expirationTime, expirationTime);
    assert_equals(Object.keys(obj.keys).length, 2);
    assert_equals(obj.keys.p256dh, key2Base64URL);
    assert_equals(obj.keys.auth, authBase64URL);

    return new Promise(resolve => resolve());
}, "PushSubscription getters");

promise_test((test) => registration.unregister(), "unregister service worker");
