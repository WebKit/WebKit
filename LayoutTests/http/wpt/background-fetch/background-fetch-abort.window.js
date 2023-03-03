// META: script=/service-workers/service-worker/resources/test-helpers.sub.js
// META: script=/background-fetch/resources/utils.js

'use strict';

promise_test(async t => {
  let serviceWorkerRegistration = await service_worker_unregister_and_register(t, "sw.js", ".");
  add_completion_callback(() => serviceWorkerRegistration.unregister());
  await wait_for_state(t, serviceWorkerRegistration.installing, 'activated');

  const id = uniqueId();
  await serviceWorkerRegistration.backgroundFetch.fetch(id, ['resources/trickle.py?ms=10&count=1000']);

  if (!window.testRunner)
    return;

  const channel = new MessageChannel();
  serviceWorkerRegistration.active.postMessage({ type:'waitForAbort', port:channel.port1 }, [channel.port1]);
  const abortPromise = new Promise(resolve => channel.port2.onmessage = (event) => resolve(event.data));

  const internalId = testRunner.getBackgroundFetchIdentifier();
  assert_greater_than(internalId.length, 0);
  testRunner.abortBackgroundFetch(internalId);

  assert_equals(await abortPromise, id);
}, "background fetch abort should trigger an abort event");
