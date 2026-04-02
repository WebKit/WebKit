/**
 * src/utils/scheduler.js
 * ─────────────────────────────────────────────────────────────
 * RAF-based scheduler and event-listener helpers.
 * Returns cleanup functions so callers can tear down cleanly.
 */

/**
 * Wrap a callback in requestAnimationFrame, cancelling any
 * pending frame before scheduling a new one.
 *
 * @param {() => void} callback
 * @returns {{ schedule: () => void, cancel: () => void }}
 */
export function createRAFScheduler(callback) {
  let rafId = null;

  function schedule() {
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

/**
 * Attach all viewport-related event listeners.
 * Returns a single cleanup() function that removes every one.
 *
 * @param {() => void} handler  - the function to call on every change
 * @returns {() => void}         - call this to remove all listeners
 */
export function attachListeners(handler) {
  const opts = { passive: true };

  // Store named references so removeEventListener works correctly.
  // Arrow functions passed inline cannot be removed — this was a bug
  // in the original flat implementation.
  const onResize            = () => handler();
  const onOrientationChange = () => setTimeout(handler, 200);
  // iOS fires orientationchange BEFORE the new dimensions are available,
  // so a 200 ms delay lets the layout settle first.

  window.addEventListener('resize', onResize, opts);
  window.addEventListener('orientationchange', onOrientationChange, opts);

  let vvpCleanup = null;
  if (window.visualViewport) {
    const onVVPResize = () => handler();
    const onVVPScroll = () => handler();
    window.visualViewport.addEventListener('resize', onVVPResize, opts);
    window.visualViewport.addEventListener('scroll', onVVPScroll, opts);

    vvpCleanup = () => {
      window.visualViewport.removeEventListener('resize', onVVPResize);
      window.visualViewport.removeEventListener('scroll', onVVPScroll);
    };
  }

  return function cleanup() {
    window.removeEventListener('resize', onResize);
    window.removeEventListener('orientationchange', onOrientationChange);
    vvpCleanup?.();
  };
}