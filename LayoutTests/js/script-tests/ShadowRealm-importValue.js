description("Test to ensure correct behaviour of ShadowRealm.prototype.importValue");

function assert_closed_opener(w, closed, opener) {
  assert_equals(w.closed, closed);
  assert_equals(w.opener, opener);
}

promise_test(async t => {
  const openee = window.open("", "greatname");
  const outerShadowRealm = new ShadowRealm();

  const importedVal = await outerShadowRealm.importValue("./example-module.js", "value");
  assert_equals(importedVal, 6);
}, "can import module in a shadow realm");
