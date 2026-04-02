const unitCache = new Map();

function supportsUnit(value) {
  if (unitCache.has(value)) return unitCache.get(value);
  const result = typeof CSS !== 'undefined' && !!CSS.supports?.('height', value);
  unitCache.set(value, result);
  return result;
}

export function snapshot() {
  const isBrowser = typeof window !== 'undefined';
  const vvp = isBrowser ? window.visualViewport : null;
  const innerHeight = isBrowser ? window.innerHeight : 0;

  const visual = vvp ? vvp.height : innerHeight;
  const offsetTop = vvp ? vvp.offsetTop : 0;

  return {
    visual,
    stable: innerHeight,
    screen: isBrowser && typeof screen !== 'undefined' ? screen.height : 0,
    offsetTop,
    hasVisualViewportAPI: !!vvp,
    supportsSVH: supportsUnit('1svh'),
    supportsDVH: supportsUnit('1dvh'),
  };
}

export function clearUnitCache() {
  unitCache.clear();
}