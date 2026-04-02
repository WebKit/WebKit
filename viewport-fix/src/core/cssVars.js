export function writeCSSVars(measurement, target) {
  const el = target ?? (typeof document !== 'undefined' ? document.documentElement : null);
  if (!el) return;
  const { visual, stable } = measurement;
  el.style.setProperty('--vh', px(visual / 100));
  el.style.setProperty('--svh', px(stable / 100));
}

export function clearCSSVars(target) {
  const el = target ?? (typeof document !== 'undefined' ? document.documentElement : null);
  if (!el) return;
  el.style.removeProperty('--vh');
  el.style.removeProperty('--svh');
}

function px(n) {
  return `${n.toFixed(4)}px`;
}