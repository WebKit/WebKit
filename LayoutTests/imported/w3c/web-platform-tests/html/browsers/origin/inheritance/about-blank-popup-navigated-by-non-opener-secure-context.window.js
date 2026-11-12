// META: timeout=long
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/common/dispatcher/dispatcher.js
// META: script=resources/about-blank-navigation.js

// This document, which isn't a secure context, opens a popup on an HTTPS
// origin, which opens a popup of its own on the same origin. The second popup
// navigates its opener to about:blank. The about:blank document gets the
// initiator's origin, so whether it's a secure context follows from that origin
// and not from the popup's opener, this document.

promise_test(async t => {
  assert_false(isSecureContext, "This document isn't a secure context");

  const popup = await openPopupContext(t, hosts.HTTPS_ORIGIN);
  const initiator = new RemoteContext(token());
  await popup.execute_script(url => {
    // The initiator waits for this iframe to go away.
    document.body.append(document.createElement("iframe"));
    open(url);
  }, [executorURL(hosts.HTTPS_ORIGIN, initiator.context_id)]);
  // The popup's executor goes away with its document, so the initiator closes itself.
  t.add_cleanup(() => initiator.execute_script(() => window.close()));

  await initiator.execute_script(() => {
    opener.location.href = "about:blank";
  });
  await t.step_wait(() => initiator.execute_script(() => opener.length === 0), "The popup navigated to about:blank", 10000, 50);

  const aboutBlankIsSecureContext = await initiator.execute_script(() => {
    try {
      return opener.isSecureContext;
    } catch (e) {
      return e.name;
    }
  });
  assert_equals(aboutBlankIsSecureContext, true, "The about:blank document is a secure context");
}, "about:blank in a popup is a secure context when the initiator's origin is potentially trustworthy, even if its opener isn't one");
