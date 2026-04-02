export function createRAFScheduler(callback) {
  let rafId = null;

  function schedule() {
    if (typeof requestAnimationFrame === 'undefined') return;
    if (rafId !== null) cancelAnimationFrame(rafId);
    rafId = requestAnimationFrame(() => {
      callback();
      rafId = null;
    });
  }

  function cancel() {
    if (rafId !== null) {
      cancelAnimationFrame(rafId);
      rafId = null;
    }
  }

  return { schedule, cancel };
}

export function attachListeners(handler) {
  if (typeof window === 'undefined') return () => {};
  const opts = { passive: true };
  let pendingOrientationTimer = null;

  const onResize = handler;

  const onOrientationChange = () => {
    if (pendingOrientationTimer !== null) clearTimeout(pendingOrientationTimer);
    pendingOrientationTimer = setTimeout(() => {
      handler();
      pendingOrientationTimer = null;
    }, 200);
  };

  window.addEventListener('resize', onResize, opts);
  window.addEventListener('orientationchange', onOrientationChange, opts);

  let vvpCleanup = null;
  const vvp = window.visualViewport ?? null;
  if (vvp) {
    vvp.addEventListener('resize', handler, opts);
    vvp.addEventListener('scroll', handler, opts);

    vvpCleanup = () => {
      vvp.removeEventListener('resize', handler);
      vvp.removeEventListener('scroll', handler);
    };
  }

  return function cleanup() {
    if (pendingOrientationTimer !== null) {
      clearTimeout(pendingOrientationTimer);
      pendingOrientationTimer = null;
    }
    window.removeEventListener('resize', onResize);
    window.removeEventListener('orientationchange', onOrientationChange);
    vvpCleanup?.();
  };
}