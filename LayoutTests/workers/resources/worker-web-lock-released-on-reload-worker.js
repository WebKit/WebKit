navigator.locks.request('worker-lock-123', function() {
  postMessage('lock acquired from worker');

  return new Promise(resolve => {
    // Don't release the lock.
  });
});
