// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/service-workers/service-worker/resources/test-helpers.sub.js

// Under COEP: credentialless, a cross-origin no-cors request doesn't include
// credentials, so its response doesn't need Cross-Origin-Resource-Policy. This
// also holds when a service worker's race-network-and-fetch-handler route gets
// the response from the network.
// https://fetch.spec.whatwg.org/#cross-origin-embedder-policy-allows-credentials

const scope = "resources/race-network-and-fetch-handler-frame.html";

for (const credentials of ["include", "same-origin", "omit"]) {
  promise_test(async test => {
    const registration = await service_worker_unregister_and_register(test,
      "resources/race-network-and-fetch-handler-sw.js", scope);
    test.add_cleanup(() => registration.unregister());
    await wait_for_state(test, registration.installing, "activated");

    const frame = await with_iframe(scope);
    test.add_cleanup(() => frame.remove());
    assert_not_equals(frame.contentWindow.navigator.serviceWorker.controller,
      null);

    const response = await frame.contentWindow.fetch(
      get_host_info().HTTPS_REMOTE_ORIGIN + "/common/square.png?" + token(),
      { mode: "no-cors", credentials });
    assert_equals(response.type, "opaque");
  }, `fetch() of a cross-origin no-cors resource without CORP, credentials "${credentials}"`);
}
