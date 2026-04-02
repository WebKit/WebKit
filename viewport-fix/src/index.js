import { snapshot } from './core/measure.js';
import { writeCSSVars, clearCSSVars } from './core/cssVars.js';
import { createRAFScheduler, attachListeners } from './utils/scheduler.js';

let active = false;

/**
 * Initialises the viewport fix, writing CSS custom properties on every resize.
 * @param {{ target?: Element, onMeasure?: (m: object) => void }} [options]
 * @returns {() => void} Cleanup function that removes all listeners and clears CSS vars.
 *   Returns a no-op if called outside a browser environment, or if already initialised.
 */
export function initViewportFix({ target, onMeasure } = {}) {
  if (typeof window === 'undefined') return () => {};

  if (active) {
    console.warn('[viewport-fix] already initialised — call cleanup() before re-initialising');
    return () => {};
  }
  active = true;
  const el = target ?? document.documentElement;

  function measure() {
    const m = snapshot();
    writeCSSVars(m, el);
    onMeasure?.(m);
  }

  const { schedule, cancel } = createRAFScheduler(measure);

  let removeListeners;
  try {
    measure();
    removeListeners = attachListeners(schedule);
  } catch (err) {
    active = false;
    cancel();
    throw err;
  }

  return function cleanup() {
    active = false;
    cancel();
    removeListeners();
    clearCSSVars(el);
  };
}

export { snapshot } from './core/measure.js';
export { writeCSSVars, clearCSSVars } from './core/cssVars.js';
