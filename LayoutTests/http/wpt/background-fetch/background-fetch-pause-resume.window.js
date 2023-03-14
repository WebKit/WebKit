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
  let serviceWorkerRegistration = await service_worker_unregister_and_register(t, "sw.js", ".");
  add_completion_callback(() => serviceWorkerRegistration.unregister());
  await wait_for_state(t, serviceWorkerRegistration.installing, 'activated');

  const id = uniqueId();
  await serviceWorkerRegistration.backgroundFetch.fetch(id, ['resources/trickle-range.py?ms=2&count=5000']);

  if (!window.testRunner)
    return;

  const internalId = testRunner.getBackgroundFetchIdentifier();
  assert_greater_than(internalId.length, 0);

  // Let's wait a bit to get some data.
  let cptr = 0;
  while(!backgroundFetchDownloaded(internalId) && ++cptr < 20)
    await new Promise(resolve => setTimeout(resolve, 500));
  assert_less_than(cptr, 20, "load is started");

  assert_false(backgroundFetchIsPaused(internalId), "not paused1");
  testRunner.pauseBackgroundFetch(internalId);
  assert_true(backgroundFetchIsPaused(internalId), "paused");

  let previousDownloaded;
  let downloaded = backgroundFetchDownloaded(internalId);
  cptr = 0;
  do {
    previousDownloaded = downloaded;
    await new Promise(resolve => setTimeout(resolve, 100));
    downloaded = backgroundFetchDownloaded(internalId);
  } while (downloaded != previousDownloaded && ++cptr < 20);
  assert_less_than(cptr, 20, "load is paused");

  testRunner.resumeBackgroundFetch(internalId);
  assert_false(backgroundFetchIsPaused(internalId), "not paused2");

  cptr = 0;
  do {
    previousDownloaded = downloaded;
    await new Promise(resolve => setTimeout(resolve, 100));
    downloaded = backgroundFetchDownloaded(internalId);
  } while (downloaded <= previousDownloaded && ++cptr < 20);
  assert_less_than(cptr, 20, "load is resumed");
}, "background fetch pause/resume should be able to recover if range requests are supported");
