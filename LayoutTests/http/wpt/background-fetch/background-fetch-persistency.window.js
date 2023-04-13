// META: script=/service-workers/service-worker/resources/test-helpers.sub.js
// META: script=/background-fetch/resources/utils.js

'use strict';

// Covers basic functionality provided by BackgroundFetchManager.fetch().
// https://wicg.github.io/background-fetch/#background-fetch-manager-fetch

promise_test(async t => {
  let serviceWorkerRegistration = await service_worker_unregister_and_register(t, "sw.js", ".");
  add_completion_callback(() => serviceWorkerRegistration.unregister());
  await wait_for_state(t, serviceWorkerRegistration.installing, 'activated');

  const uniqueId1 = uniqueId();
  const uniqueId2 = uniqueId();
  await serviceWorkerRegistration.backgroundFetch.fetch(uniqueId1, ['resources/trickle.py?ms=10&count=1000']);
  await serviceWorkerRegistration.backgroundFetch.fetch(uniqueId2, ['resources/trickle.py?ms=10&count=1000']);

  let ids = await serviceWorkerRegistration.backgroundFetch.getIds();
  assert_equals(ids.length, 2, "v1");
  assert_true(uniqueId1 == ids[0] || uniqueId1 == ids[1], "uniqueId1 v1");
  assert_true(uniqueId2 == ids[0] || uniqueId2 == ids[1], "uniqueId2 v1");

  if (window.internals)
    await internals.storeRegistrationsOnDisk();
  await new Promise(resolve => setTimeout(resolve, 100));

  if (window.testRunner)
    testRunner.terminateNetworkProcess();
  await fetch("").then(() => { }, () => { });

  const registrations = await navigator.serviceWorker.getRegistrations();
  assert_equals(registrations.length, 1);
  serviceWorkerRegistration = registrations[0];

  ids = await serviceWorkerRegistration.backgroundFetch.getIds();
  assert_equals(ids.length, 2, "v2");
  assert_true(uniqueId1 == ids[0] || uniqueId1 == ids[1], "uniqueId1 v2");
  assert_true(uniqueId2 == ids[0] || uniqueId2 == ids[1], "uniqueId2 v2");
}, "background fetches should be enumerable after network process crash");
