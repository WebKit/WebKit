/**
 * src/core/cssVars.js
 * ─────────────────────────────────────────────────────────────
 * Writes viewport measurements as CSS custom properties.
 * Separated from measure.js so tests can verify writes independently.
 */

/**
 * Write --vh and --svh onto a target element (defaults to :root).
 *
 * --vh  → 1% of the VISUAL viewport  (follows keyboard, toolbar animations)
 * --svh → 1% of the STABLE viewport  (innerHeight — does not animate)
 *
 * Usage in CSS:
 *   height: calc(var(--svh) * 100);   /* stable full-height shell *\/
 *   height: calc(var(--vh)  * 100);   /* keyboard-aware form container *\/
 *
 * @param {{ visual: number, stable: number }} measurement
 * @param {HTMLElement} [target=document.documentElement]
 */
export function writeCSSVars(measurement, target = document.documentElement) {
  const { visual, stable } = measurement;
  target.style.setProperty('--vh',  px(visual  / 100));
  target.style.setProperty('--svh', px(stable / 100));
}

/**
 * Remove the custom properties (e.g. for cleanup in tests or SSR).
 *
 * @param {HTMLElement} [target=document.documentElement]
 */
export function clearCSSVars(target = document.documentElement) {
  target.style.removeProperty('--vh');
  target.style.removeProperty('--svh');
}

// ─── helpers ────────────────────────────────────────────────

/** Format a number as a CSS px value with 4 decimal places. */
function px(n) {
  return `${n.toFixed(4)}px`;
}