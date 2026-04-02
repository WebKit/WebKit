export class DemoBoxes {
  constructor(container) {
    if (!container) throw new Error('[DemoBoxes] container is required');
    this._container = container;

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

    this._els = {};
    for (const id of ['vh', 'svh', 'dvh', 'js-svh', 'js-vh']) {
      this._els[id] = {
        val: this._container.querySelector(`#val-${id}`),
        tick: this._container.querySelector(`#tick-${id}`),
      };
    }

    this._probes = {
      vh: this._createProbe('100vh'),
      svh: this._createProbe('100svh'),
      dvh: this._createProbe('100dvh'),
    };
  }

  _createProbe(cssValue) {
    const probe = document.createElement('div');
    Object.assign(probe.style, {
      position: 'absolute',
      top: '-9999px',
      left: '-9999px',
      visibility: 'hidden',
      pointerEvents: 'none',
      height: cssValue
    });
    const parent = document.body ?? document.documentElement;
    parent.appendChild(probe);
    return probe;
  }

  update(m) {
    if (!this._container) return;
    const ref = m.screen;

    const entries = [
      { id: 'vh', h: this._probes.vh.getBoundingClientRect().height },
      { id: 'svh', h: this._probes.svh.getBoundingClientRect().height },
      { id: 'dvh', h: this._probes.dvh.getBoundingClientRect().height },
      { id: 'js-svh', h: m.stable },
      { id: 'js-vh', h: m.visual },
    ];

    for (const { id, h } of entries) {
      const { val: valEl, tick: tickEl } = this._els[id];

      const display = h > 0 ? `${Math.round(h)}px` : 'N/A';

      if (valEl) valEl.textContent = display;
      if (tickEl) tickEl.style.height = (h > 0 && ref > 0)
        ? `${Math.min(100, (h / ref) * 100).toFixed(1)}%`
        : '0%';
    }
  }

  destroy() {
    Object.values(this._probes).forEach(p => p.remove());
    if (this._container) this._container.innerHTML = '';
    this._probes = {};
    this._els = {};
    this._container = null;
  }
}