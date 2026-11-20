// META: title=XMLHttpRequest: open() and send() during the request error steps

async_test(t => {
  const client = new XMLHttpRequest();
  client.onerror = t.step_func(() => {
    client.onerror = t.unreached_func("error event for the second request");
    client.onload = t.step_func_done(() => {
      assert_equals(client.responseText, "hello");
    });
    client.open("GET", "data:text/plain,hello");
    client.send();
  });
  client.open("GET", "invalid-protocol://example.com");
  client.send();
}, "open() and send() in an error event listener start a new request");

async_test(t => {
  const client = new XMLHttpRequest();
  client.onerror = t.step_func(() => {
    client.onerror = t.step_func_done();
    client.open("GET", "invalid-protocol://example.com");
    client.send();
  });
  client.open("GET", "invalid-protocol://example.com");
  client.send();
}, "open() and send() in an error event listener start a new request, which can fail too");
