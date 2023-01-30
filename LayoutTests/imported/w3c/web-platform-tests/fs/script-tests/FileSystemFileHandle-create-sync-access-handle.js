'use strict';

// This script depends on the following scripts:
//    /fs/resources/messaging-helpers.js
//    /fs/resources/test-helpers.js

directory_test(async (t, root_dir) => {
  const dedicated_worker =
      create_dedicated_worker(t, kDedicatedWorkerMessageTarget);
  const file_handle =
      await root_dir.getFileHandle('sync-access-handle-file', {create: true});

  dedicated_worker.postMessage(
      {type: 'create-sync-access-handle', file_handle});

  const event_watcher = new EventWatcher(t, dedicated_worker, 'message');
  const message_event = await event_watcher.wait_for('message');
  const response = message_event.data;

  assert_true(response.success);
}, 'Attempt to create a sync access handle.');
