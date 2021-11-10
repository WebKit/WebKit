// META: title=PushEvent tests
// META: global=serviceworker
// META: script=constants.js

test(() => {
    let event = new PushSubscriptionChangeEvent("pushsubscriptionchange");
    assert_equals(event.newSubscription, null, "new");
    assert_equals(event.oldSubscription, null, "old");
}, "PushSubscriptionChangeEvent without data");

test(() => {
    let event = new PushSubscriptionChangeEvent("pushsubscriptionchange", { newSubscription: null, oldSubscription: null });
    assert_equals(event.newSubscription, null, "new");
    assert_equals(event.oldSubscription, null, "old");
}, "PushSubscriptionChangeEvent without subscriptions");

let newSubscription = null;
let oldSubscription = null;

let activatePromise = new Promise(resolve => self.onactivate = resolve);
promise_test(async () => {
   return activatePromise;
}, "wait for active service worker");

promise_test(async() => {
    newSubscription = self.internals.createPushSubscription(ENDPOINT, EXPIRATION_TIME, VALID_SERVER_KEY, CLIENT_KEY_1, AUTH);
    oldSubscription = self.internals.createPushSubscription(ENDPOINT, EXPIRATION_TIME, VALID_SERVER_KEY, CLIENT_KEY_2, AUTH);

    assert_true(newSubscription instanceof PushSubscription);
    assert_true(oldSubscription instanceof PushSubscription);

    return new Promise(resolve => resolve());
}, "create subscriptions");

test(() => {
    let event = new PushSubscriptionChangeEvent("pushsubscriptionchange", { newSubscription });
    assert_equals(event.newSubscription, newSubscription, "new");
    assert_equals(event.oldSubscription, null, "old");
}, "PushSubscriptionChangeEvent with new subscription");

test(() => {
    let event = new PushSubscriptionChangeEvent("pushsubscriptionchange", { oldSubscription });
    assert_equals(event.newSubscription, null, "new");
    assert_equals(event.oldSubscription, oldSubscription, "old");
}, "PushSubscriptionChangeEvent with old subscription");

test(() => {
    let event = new PushSubscriptionChangeEvent("pushsubscriptionchange", { newSubscription, oldSubscription });
    assert_equals(event.newSubscription, newSubscription, "new");
    assert_equals(event.oldSubscription, oldSubscription, "old");
}, "PushSubscriptionChangeEvent with new and old subscription");
