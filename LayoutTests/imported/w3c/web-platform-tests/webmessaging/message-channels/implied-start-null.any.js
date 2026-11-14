// META: title=Setting onmessage to a non-callable value implies start()

async_test(t => {
  const { port1, port2 } = new MessageChannel();
  port2.onmessage = null;
  port2.addEventListener("message", t.step_func_done(e => {
    assert_equals(e.data, "ping");
  }));
  port1.postMessage("ping");
}, "Setting onmessage to null enables the port message queue");

async_test(t => {
  const { port1, port2 } = new MessageChannel();
  port2.onmessage = 1;
  port2.addEventListener("message", t.step_func_done(e => {
    assert_equals(e.data, "ping");
  }));
  port1.postMessage("ping");
}, "Setting onmessage to a non-callable value enables the port message queue");

async_test(t => {
  const { port1, port2 } = new MessageChannel();
  port2.onmessage = () => {};
  port2.onmessage = null;
  port2.addEventListener("message", t.step_func_done(e => {
    assert_equals(e.data, "ping");
  }));
  port1.postMessage("ping");
}, "Clearing onmessage after setting it does not disable the port message queue");

async_test(t => {
  const { port1, port2 } = new MessageChannel();
  port2.onmessageerror = () => {};
  port2.addEventListener("message", t.unreached_func("onmessageerror must not enable the port message queue"));
  port1.postMessage("ping");
  t.step_timeout(t.step_func_done(), 100);
}, "Setting onmessageerror does not enable the port message queue");
