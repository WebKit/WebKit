// META: title=PushSubscription tests
// META: global=window,serviceworker
// META: script=constants.js

let subscription = null;

if (GLOBAL.isWorker()) {
    let activatePromise = new Promise(resolve => self.onactivate = resolve);
    promise_test(async () => {
       return activatePromise;
    }, "wait for active service worker");
}

promise_test(() => {
    assert_true(!!self.internals, "test requires internals");

    subscription = self.internals.createPushSubscription(ENDPOINT, EXPIRATION_TIME, VALID_SERVER_KEY, CLIENT_KEY_1, AUTH);
    assert_true(subscription instanceof PushSubscription);

    assert_equals(subscription.endpoint, ENDPOINT);
    assert_equals(subscription.expirationTime, EXPIRATION_TIME);
    assert_true(subscription.options === subscription.options, "options should always return same object");
    assert_equals(subscription.options.userVisibleOnly, true);
    assert_true(subscription.options.applicationServerKey === subscription.options.applicationServerKey, "applicationServerKey should return same object");
    assert_array_equals(bytesFrom(subscription.options.applicationServerKey), bytesFrom(VALID_SERVER_KEY));
    assert_array_equals(bytesFrom(subscription.getKey('p256dh')), bytesFrom(CLIENT_KEY_1));
    assert_array_equals(bytesFrom(subscription.getKey('auth')), bytesFrom(AUTH));

    obj = subscription.toJSON();
    assert_equals(Object.keys(obj).length, 3);
    assert_equals(obj.endpoint, ENDPOINT);
    assert_equals(obj.expirationTime, EXPIRATION_TIME);
    assert_equals(Object.keys(obj.keys).length, 2);
    assert_equals(obj.keys.p256dh, CLIENT_BASE64_KEY_1);
    assert_equals(obj.keys.auth, BASE64_AUTH);

    return new Promise(resolve => resolve());
}, "PushSubscription getters");

promise_test(async () => {
    let result = await subscription.unsubscribe();
    assert_false(result, "subscription that is not associated with a service worker registration should already be unsubscribed");
    return new Promise(resolve => resolve());
}, "unsubscribing from subscription not associated with a registration");
