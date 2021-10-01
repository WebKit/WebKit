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
