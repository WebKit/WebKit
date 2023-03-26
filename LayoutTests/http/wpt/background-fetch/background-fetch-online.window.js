// META: script=/service-workers/service-worker/resources/test-helpers.sub.js
// META: script=/background-fetch/resources/utils.js

'use strict';

function backgroundFetchDownloaded(id)
{
  const state = JSON.parse(testRunner.backgroundFetchState(id));
  return state.downloaded;
}

function backgroundFetchIsPaused(id)
{
  const state = JSON.parse(testRunner.backgroundFetchState(id));
  return state.isPaused;
}

promise_test(async t => {
  if (!window.testRunner)
    return;

  const serviceWorkerRegistration = await service_worker_unregister_and_register(t, "sw.js", ".");
  add_completion_callback(() => serviceWorkerRegistration.unregister());
  await wait_for_state(t, serviceWorkerRegistration.installing, 'activated');

  assert_true(navigator.onLine, "onLine");

  const offlinePromise = new Promise(resolve => window.onoffline = resolve);
  testRunner.setOnLineOverride(false);
  await offlinePromise;

  const id = uniqueId();
  await serviceWorkerRegistration.backgroundFetch.fetch(id, ['resources/trickle-range.py?ms=2&count=5000']);

  const internalId = testRunner.getBackgroundFetchIdentifier();
  assert_greater_than(internalId.length, 0);

  // Let's wait a bit to get some data.
  await new Promise(resolve => setTimeout(resolve, 500));

  assert_equals(backgroundFetchDownloaded(internalId), 0, "load is not started");

  const onlinePromise = new Promise(resolve => window.ononline = resolve);
  testRunner.setOnLineOverride(true);
  await onlinePromise;

  let cptr = 0;
  while(!backgroundFetchDownloaded(internalId) && ++cptr < 20)
    await new Promise(resolve => setTimeout(resolve, 500));
  assert_less_than(cptr, 20, "load is started");
}, "background fetch delayed until online");
