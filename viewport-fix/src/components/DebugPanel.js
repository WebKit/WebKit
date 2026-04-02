export class DebugPanel {
  constructor(container) {
    if (!container) throw new Error('[DebugPanel] container is required');
    this._container = container;

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

    this._valNodes = {};
    const pairs = [
      'window.innerHeight',
      'visualViewport.height',
      'visualViewport.offsetTop',
      'screen.height',
      '--vh resolved (1%)',
      '--svh resolved (1%)',
      'CSS svh support',
      'CSS dvh support',
      'visualViewport API',
      'Last updated'
    ];

    for (const label of pairs) {
      const tr = document.createElement('tr');
      const td1 = document.createElement('td');
      const td2 = document.createElement('td');
      td1.textContent = label;
      td2.textContent = '';
      tr.appendChild(td1);
      tr.appendChild(td2);
      this._tbody.appendChild(tr);
      this._valNodes[label] = td2;
    }

    this._badgeSpan = document.createElement('span');
    if (this._badge) this._badge.appendChild(this._badgeSpan);
  }

  update(m) {
    if (!this._container) return;

    this._valNodes['window.innerHeight'].textContent = `${m.stable}px`;
    this._valNodes['visualViewport.height'].textContent = m.hasVisualViewportAPI ? `${m.visual.toFixed(2)}px` : 'N/A';
    this._valNodes['visualViewport.offsetTop'].textContent = m.hasVisualViewportAPI ? `${m.offsetTop.toFixed(2)}px` : 'N/A';
    this._valNodes['screen.height'].textContent = `${m.screen}px`;
    this._valNodes['--vh resolved (1%)'].textContent = `${(m.visual / 100).toFixed(4)}px`;
    this._valNodes['--svh resolved (1%)'].textContent = `${(m.stable / 100).toFixed(4)}px`;
    this._valNodes['CSS svh support'].textContent = m.supportsSVH ? '✅ Yes' : '❌ No';
    this._valNodes['CSS dvh support'].textContent = m.supportsDVH ? '✅ Yes' : '❌ No';
    this._valNodes['visualViewport API'].textContent = m.hasVisualViewportAPI ? '✅ Yes' : '❌ No';
    this._valNodes['Last updated'].textContent = new Date().toLocaleTimeString();

    if (this._badge) {
      const allGood = m.supportsSVH && m.supportsDVH && m.hasVisualViewportAPI;
      this._badgeSpan.className = allGood ? 'badge badge--ok' : 'badge badge--warn';
      this._badgeSpan.textContent = allGood ? 'Full Support' : 'Partial — JS active';
    }
  }

  destroy() {
    if (this._container) this._container.innerHTML = '';
    this._valNodes = {};
    this._badgeSpan = null;
    this._container = null;
  }
}