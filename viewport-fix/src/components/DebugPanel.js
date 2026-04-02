/**
 * src/components/DebugPanel.js
 * ─────────────────────────────────────────────────────────────
 * Renders live viewport metrics into a <tbody> element.
 * Completely optional — import only in development / demo builds.
 *
 * Usage:
 *   import { DebugPanel } from './components/DebugPanel.js';
 *   const panel = new DebugPanel(document.getElementById('debug-output'));
 *   // then pass measurements to it:
 *   panel.update(measurement);
 */

export class DebugPanel {
  /**
   * @param {HTMLElement} container - the element to render the panel into
   */
  constructor(container) {
    if (!container) throw new Error('[DebugPanel] container is required');
    
    container.innerHTML = `
      <div class="debug-panel">
        <div class="debug-panel__header">
          <span class="debug-panel__title">Live Viewport Metrics</span>
          <span class="badge-target"></span>
        </div>
        <table>
          <tbody class="output-target"></tbody>
        </table>
      </div>
    `;

    this._tbody = container.querySelector('.output-target');
    this._badge = container.querySelector('.badge-target');
  }

  /**
   * Re-render the panel with the latest measurement.
   * @param {import('../core/measure.js').ViewportMeasurement} m
   */
  update(m) {
    const vvp = window.visualViewport;

    this._tbody.innerHTML = rows([
      ['window.innerHeight',       `${window.innerHeight}px`],
      ['visualViewport.height',    vvp ? `${vvp.height.toFixed(2)}px` : 'N/A'],
      ['visualViewport.offsetTop', vvp ? `${vvp.offsetTop.toFixed(2)}px` : 'N/A'],
      ['screen.height',            `${screen.height}px`],
      ['--vh resolved (1%)',        `${(m.visual  / 100).toFixed(4)}px`],
      ['--svh resolved (1%)',       `${(m.stable / 100).toFixed(4)}px`],
      ['CSS svh support',          m.supportsSVH ? '✅ Yes' : '❌ No'],
      ['CSS dvh support',          m.supportsDVH ? '✅ Yes' : '❌ No'],
      ['visualViewport API',       m.hasVisualViewportAPI ? '✅ Yes' : '❌ No'],
      ['Last updated',             new Date().toLocaleTimeString()],
    ]);

    if (this._badge) {
      const allGood = m.supportsSVH && m.supportsDVH && m.hasVisualViewportAPI;
      this._badge.innerHTML = allGood
        ? '<span class="badge badge--ok">Full Support</span>'
        : '<span class="badge badge--warn">Partial — JS active</span>';
    }
  }
}

// ─── helpers ────────────────────────────────────────────────

function rows(pairs) {
  return pairs.map(([label, value]) => `
    <tr>
      <td>${label}</td>
      <td>${value}</td>
    </tr>
  `).join('');
}