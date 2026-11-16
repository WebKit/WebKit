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

  const internalId = testRunner.getBackgroundFetchIdentifier();
  assert_equals(testRunner.lastAddedBackgroundFetchIdentifier(), internalId, "added");

  let counter = 0;
  while (testRunner.lastUpdatedBackgroundFetchIdentifier() != internalId && ++counter < 100)
    await new Promise(resolve => setTimeout(resolve, 20));
  assert_less_than(counter, 100);

  const channel = new MessageChannel();
  serviceWorkerRegistration.active.postMessage({ type:'waitForAbort', port:channel.port1 }, [channel.port1]);
  const abortPromise = new Promise(resolve => channel.port2.onmessage = (event) => resolve(event.data));

  assert_greater_than(internalId.length, 0);
  testRunner.abortBackgroundFetch(internalId);

  assert_equals(await abortPromise, id);

  counter = 0;
  while (testRunner.lastRemovedBackgroundFetchIdentifier() != internalId && ++counter < 100)
    await new Promise(resolve => setTimeout(resolve, 20));
  assert_less_than(counter, 100);
}, "background fetch abort should trigger an abort event");
