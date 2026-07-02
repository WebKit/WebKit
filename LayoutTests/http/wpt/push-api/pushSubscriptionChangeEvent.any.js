// META: title=PushEvent tests
// META: global=serviceworker
// META: script=constants.js

let activatePromise = new Promise(resolve => self.onactivate = resolve);
promise_test(() => {
   return activatePromise;
}, "wait for active service worker");

let newSubscription = null;
let oldSubscription = null;

function assertSubscriptionsAreEqual(a, b, reason)
{
    if (!a || !b)
        assert_equals(a, b, reason);
    else
        assert_equals(JSON.stringify(a.toJSON()), JSON.stringify(b.toJSON()), reason);
}

promise_test(() => {
    newSubscription = self.internals.createPushSubscription(ENDPOINT, EXPIRATION_TIME, VALID_SERVER_KEY, CLIENT_KEY_1, AUTH);
    oldSubscription = self.internals.createPushSubscription(ENDPOINT, EXPIRATION_TIME, VALID_SERVER_KEY, CLIENT_KEY_2, AUTH);

    assert_true(newSubscription instanceof PushSubscription);
    assert_true(oldSubscription instanceof PushSubscription);

    return new Promise(resolve => resolve());
}, "create subscriptions");

promise_test(async() => {
    self.internals.schedulePushSubscriptionChangeEvent(null, null);
    let event = await new Promise(resolve => self.onpushsubscriptionchange = resolve);
    assertSubscriptionsAreEqual(event.newSubscription, null, "new");
    assertSubscriptionsAreEqual(event.oldSubscription, null, "old");
}, "PushSubscriptionChangeEvent without subscriptions");

promise_test(async() => {
    self.internals.schedulePushSubscriptionChangeEvent(newSubscription, null);
    let event = await new Promise(resolve => self.onpushsubscriptionchange = resolve);
    assertSubscriptionsAreEqual(event.newSubscription, newSubscription, "new");
    assertSubscriptionsAreEqual(event.oldSubscription, null, "old");
}, "PushSubscriptionChangeEvent with new subscription");

promise_test(async() => {
    self.internals.schedulePushSubscriptionChangeEvent(null, oldSubscription);
    let event = await new Promise(resolve => self.onpushsubscriptionchange = resolve);
    assertSubscriptionsAreEqual(event.newSubscription, null, "new");
    assertSubscriptionsAreEqual(event.oldSubscription, oldSubscription, "old");
}, "PushSubscriptionChangeEvent with old subscription");

promise_test(async() => {
    self.internals.schedulePushSubscriptionChangeEvent(newSubscription, oldSubscription);
    let event = await new Promise(resolve => self.onpushsubscriptionchange = resolve);
    assertSubscriptionsAreEqual(event.newSubscription, newSubscription, "new");
    assertSubscriptionsAreEqual(event.oldSubscription, oldSubscription, "old");
}, "PushSubscriptionChangeEvent with new and old subscription");
