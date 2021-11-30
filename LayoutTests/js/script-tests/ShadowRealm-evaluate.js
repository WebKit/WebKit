description("Test to ensure correct behaviour of ShadowRealm.prototype.evaluate");

function assert_closed_opener(w, closed, opener) {
  assert_equals(w.closed, closed);
  assert_equals(w.opener, opener);
}

promise_test(async t => {
  const openee = window.open("", "greatname");
  const outerShadowRealm = new ShadowRealm();

  // setup realm nested in outerShadowRealm
  outerShadowRealm.evaluate("var innerRealm = new ShadowRealm();");
  outerShadowRealm.evaluate("var callInInner = innerRealm.evaluate(\"(cb) => cb()\");");

  const callInNestedRealm = outerShadowRealm.evaluate("(cb) => callInInner(cb)");

  // call close window two levels deep
  assert_closed_opener(openee, false, self);
  callInNestedRealm(() => openee.close());
  // openee.close()
  assert_closed_opener(openee, true, self);
}, "window.close() affects name targeting immediately");
