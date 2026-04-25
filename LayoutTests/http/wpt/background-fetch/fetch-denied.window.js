// META: script=/service-workers/service-worker/resources/test-helpers.sub.js
// META: script=/background-fetch/resources/utils.js

'use strict';

// Covers basic functionality provided by BackgroundFetchManager.fetch().
// https://wicg.github.io/background-fetch/#background-fetch-manager-fetch

promise_test(async t => {
  if (typeof test_driver === 'undefined') {
    await loadScript('/resources/testdriver.js');
    await loadScript('/resources/testdriver-vendor.js');
  }

  await test_driver.set_permission({name: 'background-fetch'}, 'denied');

  const serviceWorkerRegistration = await service_worker_unregister_and_register(t, "sw.js", ".");
  add_completion_callback(() => serviceWorkerRegistration.unregister());
  await wait_for_state(t, serviceWorkerRegistration.installing, 'activated');

  return promise_rejects_dom(t, 'NotAllowedError',
    serviceWorkerRegistration.backgroundFetch.fetch(uniqueId(), ['resources/feature-name.txt']),
    'fetch must reject if permission is denieds');
}, "fetch should fail with NotAllowedError if permission is denied");
