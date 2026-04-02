/**
 * src/core/measure.js
 * ─────────────────────────────────────────────────────────────
 * Raw viewport measurement — no side effects, no DOM writes.
 * Returns a plain object so callers can decide what to do with it.
 */

/**
 * @typedef {Object} ViewportMeasurement
 * @property {number} visual  - visualViewport.height (tracks keyboard + toolbar)
 * @property {number} stable  - window.innerHeight    (stable, ignores toolbar)
 * @property {number} screen  - screen.height         (physical screen)
 * @property {number} offsetTop - visualViewport.offsetTop (scroll offset)
 * @property {boolean} hasVisualViewportAPI
 * @property {boolean} supportsSVH  - CSS svh unit support
 * @property {boolean} supportsDVH  - CSS dvh unit support
 */

/**
 * Take a single snapshot of all viewport dimensions.
 * Pure function — reads DOM but writes nothing.
 *
 * @returns {ViewportMeasurement}
 */
export function snapshot() {
  const vvp = window.visualViewport;
  const visual  = vvp ? vvp.height    : window.innerHeight;
  const offsetTop = vvp ? vvp.offsetTop : 0;

  return {
    visual,
    stable:   window.innerHeight,
    screen:   screen.height,
    offsetTop,
    hasVisualViewportAPI: !!vvp,
    supportsSVH: supportsUnit('1svh'),
    supportsDVH: supportsUnit('1dvh'),
  };
}

/**
 * Probe whether the browser understands a given CSS unit string.
 * Result is memoised — the answer never changes mid-session.
 *
 * @param {string} value  e.g. '1svh'
 * @returns {boolean}
 */
const unitCache = new Map();
function supportsUnit(value) {
  if (unitCache.has(value)) return unitCache.get(value);
  const result = typeof CSS !== 'undefined' && !!CSS.supports?.('height', value);
  unitCache.set(value, result);
  return result;
}