/**
 * src/components/DemoBoxes.js
 * ─────────────────────────────────────────────────────────────
 * Updates the visual height demo boxes and tick bars.
 * Uses a hidden probe element to measure CSS unit heights —
 * the probe is created once and cleaned up with destroy().
 */

export class DemoBoxes {
  /**
   * @param {HTMLElement} container - the element to render the boxes into
   */
  constructor(container) {
    if (!container) throw new Error('[DemoBoxes] container is required');
    this._container = container;
    this._probe = null;

    this._container.innerHTML = `
      <div class="demo-grid">
        ${['vh', 'svh', 'dvh', 'js-svh', 'js-vh'].map(id => `
          <div class="demo-card">
            <div class="demo-card__label">${id.toUpperCase()}</div>
            <div class="demo-card__box">
              <div class="tick-bar"><div class="tick-fill" id="tick-${id}"></div></div>
              <div class="val-display" id="val-${id}">—</div>
            </div>
          </div>
        `).join('')}
      </div>
    `;
  }

  /**
   * Update all demo boxes based on the latest measurement.
   * @param {import('../core/measure.js').ViewportMeasurement} m
   */
  update(m) {
    const ref = screen.height;

    const entries = [
      { id: 'vh',     h: this._probeUnit('100vh')  },
      { id: 'svh',    h: this._probeUnit('100svh') },
      { id: 'dvh',    h: this._probeUnit('100dvh') },
      { id: 'js-svh', h: m.stable                  },
      { id: 'js-vh',  h: m.visual                  },
    ];

    for (const { id, h } of entries) {
      const valEl  = this._container.querySelector(`#val-${id}`);
      const tickEl = this._container.querySelector(`#tick-${id}`);

      const display = h > 0 ? `${Math.round(h)}px` : 'N/A';

      if (valEl)  valEl.textContent = display;
      if (tickEl) tickEl.style.height = h > 0
        ? `${Math.min(100, (h / ref) * 100).toFixed(1)}%`
        : '0%';
    }
  }

  /** Remove the probe element from the DOM. Call in cleanup(). */
  destroy() {
    this._probe?.remove();
    this._probe = null;
  }

  // ─── private ──────────────────────────────────────────────

  /**
   * Measure the rendered height of a CSS value using a hidden probe element.
   * Returns 0 for unsupported units (e.g. svh on old browsers).
   *
   * @param {string} cssValue
   * @returns {number}
   */
  _probeUnit(cssValue) {
    if (!this._probe) {
      this._probe = document.createElement('div');
      Object.assign(this._probe.style, {
        position:      'absolute',
        top:           '-9999px',
        left:          '-9999px',
        visibility:    'hidden',
        pointerEvents: 'none',
      });
      document.body.appendChild(this._probe);
    }

    this._probe.style.height = cssValue;
    return this._probe.getBoundingClientRect().height;
  }
}