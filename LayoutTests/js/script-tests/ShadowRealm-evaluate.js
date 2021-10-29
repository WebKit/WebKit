description("Test to ensure correct behaviour of ShadowRealm.prototype.evaluate");

function assert_closed_opener(w, closed, opener) {
  assert_equals(w.closed, closed);
  assert_equals(w.opener, opener);
}

promise_test(async t => {
  const openee = window.open("", "greatname");
  const shadowRealm = new ShadowRealm();
  const callInRealm = shadowRealm.evaluate("(cb) => cb()");
  assert_closed_opener(openee, false, self);
  callInRealm(() => openee.close());
  assert_closed_opener(openee, true, self);
}, "window.close() affects name targeting immediately");
