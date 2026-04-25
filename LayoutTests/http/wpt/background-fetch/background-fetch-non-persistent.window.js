// META: script=/service-workers/service-worker/resources/test-helpers.sub.js
// META: script=/background-fetch/resources/utils.js

'use strict';

promise_test(async t => {
  let serviceWorkerRegistration = await service_worker_unregister_and_register(t, "sw.js", ".");
  add_completion_callback(() => serviceWorkerRegistration.unregister());
  await wait_for_state(t, serviceWorkerRegistration.installing, 'activated');

  const channel = new MessageChannel();
  serviceWorkerRegistration.active.postMessage({ type:'waitForSuccess', record:'resources/trickle.py?ms=10&count=1', port:channel.port1 }, [channel.port1]);
  const successPromise = new Promise(resolve => channel.port2.onmessage = (event) => resolve(event.data));

  await serviceWorkerRegistration.backgroundFetch.fetch(uniqueId(), ['resources/trickle.py?ms=10&count=1']);

  assert_equals(await successPromise, "TEST_TRICKLE\n");
}, "background fetch response should be available in ephemeral sessions");
