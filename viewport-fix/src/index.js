/**
 * viewport-fix/src/index.js
 * ─────────────────────────────────────────────────────────────
 * Public API — this is the only file consumers import.
 *
 * Usage:
 *   import { initViewportFix } from './src/index.js';
 *   const cleanup = initViewportFix();
 *   // later:
 *   cleanup();
 */

import { snapshot }       from './core/measure.js';
import { writeCSSVars, clearCSSVars } from './core/cssVars.js';
import { createRAFScheduler, attachListeners } from './utils/scheduler.js';

/**
 * Initialise the viewport fix.
 *
 * 1. Takes an immediate measurement (no flash of wrong height).
 * 2. Writes --vh and --svh to :root (or a custom target).
 * 3. Attaches listeners that re-measure on resize / orientationchange.
 * 4. Returns a cleanup function for use in React useEffect, Vue onUnmounted, etc.
 *
 * @param {{ target?: HTMLElement, onMeasure?: (m: import('./core/measure.js').ViewportMeasurement) => void }} [options]
 * @returns {() => void} cleanup
 */
export function initViewportFix({ target, onMeasure } = {}) {
  const el = target ?? document.documentElement;

  const { schedule, cancel } = createRAFScheduler(() => {
    const m = snapshot();
    writeCSSVars(m, el);
    onMeasure?.(m);
  });

  // Synchronous first run — must happen before first paint
  const initial = snapshot();
  writeCSSVars(initial, el);
  onMeasure?.(initial);

  // Wire up event listeners — returns a named cleanup fn
  const removeListeners = attachListeners(schedule);

  return function cleanup() {
    cancel();
    removeListeners();
    clearCSSVars(el);
  };
}

// Re-export primitives for advanced use (e.g. SSR, testing)
export { snapshot } from './core/measure.js';
export { writeCSSVars, clearCSSVars } from './core/cssVars.js';
