// META: title=PushEvent tests
// META: global=serviceworker

test(() => {
    let event = new PushEvent("push");
    assert_equals(event.data, null, "1");

    event = new PushEvent("push", { });
    assert_equals(event.data, null, "2");
}, "PushEvent without data");

test(() => {
    let event = new PushEvent("push", { data : "" });
    assert_not_equals(event.data, null);
    assert_equals(event.data.text(), "");

    const value = { test : 1 };
    const stringValue = JSON.stringify(value);
    const encoder = new TextEncoder();

    for (let value of [stringValue, encoder.encode(stringValue)]) {
        event = new PushEvent("push", { data : value });
        assert_true(event instanceof PushEvent);
        assert_true(event instanceof ExtendableEvent);

        const data = event.data;
        assert_true(data instanceof PushMessageData);
        assert_equals(data.text(), stringValue);
        assert_true(data.blob() instanceof Blob);
        assert_equals(data.json().test, 1);
        assert_equals(data.arrayBuffer().byteLength, stringValue.length);
    }
}, "PushEvent with data");

test(() => {
    let event = new PushEvent("push", { data : new ArrayBuffer() });
    assert_not_equals(event.data, null);
    assert_equals(event.data.text(), "");

    const value = "potato";
    const decoder = new TextDecoder();
    const encoder = new TextEncoder();
    const buffer = encoder.encode(value);
    event = new PushEvent("push", { data : buffer });

    assert_false(event.data.arrayBuffer() === buffer, "test 1");
    assert_false(event.data.arrayBuffer() === event.data.arrayBuffer(), "test 2");
    assert_equals(decoder.decode(event.data.arrayBuffer()), value);
}, "PushEvent arrayBuffer handling");

test(() => {
    const event = new PushEvent("push", { data : "{test : 1}" });
    assert_throws_dom("SyntaxError", () => event.data.json());
}, "PushEvent with bad json");

let activatePromise = new Promise(resolve => self.onactivate = resolve);
promise_test(async () => {
   return activatePromise;
}, "Wait for activate promise");

promise_test(async () => {
    const promise = self.internals.schedulePushEvent("test");
    const event = await new Promise(resolve => self.onpush = resolve);
    assert_equals(event.data.text(), "test");
    assert_true(await promise);
}, "Simulating firing of a push event");

promise_test(async () => {
    if (!self.internals)
        return;

    let resolveWaitUntilPromise;
    const waitUntilPromise = new Promise(resolve => resolveWaitUntilPromise = resolve);

    let isPushEventPromiseResolved = false;
    const promise = internals.schedulePushEvent("test");
    promise.then(() => isPushEventPromiseResolved = true);

    const event = await new Promise(resolve => self.onpush = (event) => {
        event.waitUntil(waitUntilPromise);
        resolve(event);
    });
    assert_equals(event.data.text(), "test");

    await new Promise(resolve => self.setTimeout(resolve, 100));
    assert_false(isPushEventPromiseResolved);

    resolveWaitUntilPromise();
    assert_true(await promise);
}, "Simulating firing of a push event - successful waitUntil");

promise_test(async () => {
    if (!self.internals)
        return;

    const promise = internals.schedulePushEvent("test");
    const event = await new Promise(resolve => self.onpush = (event) => {
        event.waitUntil(Promise.resolve());
        event.waitUntil(Promise.reject('error'));
        resolve(event);
    });
    assert_equals(event.data.text(), "test");
    assert_false(await promise);
}, "Simulating firing of a push event - unsuccessful waitUntil");
