
import { snapshot, clearUnitCache }       from '../src/core/measure.js';
import { writeCSSVars, clearCSSVars }    from '../src/core/cssVars.js';
import { createRAFScheduler, attachListeners } from '../src/utils/scheduler.js';
import { initViewportFix }               from '../src/index.js';


beforeEach(() => {
  global.screen = { height: 844 };

  global.CSS = {
    supports: (prop, val) => prop === 'height' && ['1svh', '1dvh', '1lvh'].includes(val),
  };

  Object.defineProperty(window, 'innerHeight', {
    writable: true,
    configurable: true,
    value: 750,
  });

  global.window.visualViewport = {
    height:    740,
    offsetTop: 0,
    addEventListener:    vi.fn(),
    removeEventListener: vi.fn(),
  };

  let rafId = 0;
  let pendingRAFs = new Map();
  global.requestAnimationFrame  = (cb) => { 
    const id = ++rafId;
    pendingRAFs.set(id, cb);
    return id; 
  };
  global.cancelAnimationFrame   = vi.fn((id) => {
    pendingRAFs.delete(id);
  });
  global.runPendingRAF = () => {
    const queue = Array.from(pendingRAFs.values());
    pendingRAFs.clear();
    queue.forEach(cb => cb());
  };
});

afterEach(() => {
  document.documentElement.style.removeProperty('--vh');
  document.documentElement.style.removeProperty('--svh');
  delete global.window.visualViewport;
  clearUnitCache();
  vi.restoreAllMocks();
});


describe('snapshot()', () => {
  test('uses visualViewport.height for visual when API is present', () => {
    const m = snapshot();
    expect(m.visual).toBe(740);
  });

  test('falls back to innerHeight when visualViewport is absent', () => {
    delete global.window.visualViewport;
    const m = snapshot();
    expect(m.visual).toBe(750);
    expect(m.hasVisualViewportAPI).toBe(false);
  });

  test('always uses innerHeight for stable', () => {
    const m = snapshot();
    expect(m.stable).toBe(750);
  });

  test('reports CSS unit support correctly', () => {
    const m = snapshot();
    expect(m.supportsSVH).toBe(true);
    expect(m.supportsDVH).toBe(true);
  });

  test('reports hasVisualViewportAPI correctly', () => {
    expect(snapshot().hasVisualViewportAPI).toBe(true);
    delete global.window.visualViewport;
    expect(snapshot().hasVisualViewportAPI).toBe(false);
  });
});


describe('writeCSSVars()', () => {
  test('sets --vh to visual / 100 as px string', () => {
    writeCSSVars({ visual: 740, stable: 750 });
    const val = document.documentElement.style.getPropertyValue('--vh');
    expect(val).toBe('7.4000px');
  });

  test('sets --svh to stable / 100 as px string', () => {
    writeCSSVars({ visual: 740, stable: 750 });
    const val = document.documentElement.style.getPropertyValue('--svh');
    expect(val).toBe('7.5000px');
  });

  test('writes to a custom target element', () => {
    const el = document.createElement('div');
    writeCSSVars({ visual: 600, stable: 600 }, el);
    expect(el.style.getPropertyValue('--vh')).toBe('6.0000px');
  });
});


describe('clearCSSVars()', () => {
  test('removes --vh and --svh from :root', () => {
    writeCSSVars({ visual: 740, stable: 750 });
    clearCSSVars();
    expect(document.documentElement.style.getPropertyValue('--vh')).toBe('');
    expect(document.documentElement.style.getPropertyValue('--svh')).toBe('');
  });
});


describe('createRAFScheduler()', () => {
  test('calls callback after schedule() and runPendingRAF()', () => {
    const cb = vi.fn();
    const { schedule } = createRAFScheduler(cb);
    schedule();
    expect(cb).not.toHaveBeenCalled();
    global.runPendingRAF();
    expect(cb).toHaveBeenCalledTimes(1);
  });

  test('cancel() prevents pending callback', () => {
    const cb = vi.fn();
    const { schedule, cancel } = createRAFScheduler(cb);
    schedule();
    cancel();
    global.runPendingRAF();
    expect(cb).not.toHaveBeenCalled();
  });

  test('scheduling twice cancels the first frame', () => {
    const { schedule } = createRAFScheduler(vi.fn());
    schedule(); // returns 1
    schedule(); // returns 2, should cancel 1
    expect(global.cancelAnimationFrame).toHaveBeenCalledWith(1);
  });
});


describe('attachListeners()', () => {
  test('attaches listeners to window and visualViewport', () => {
    const handler = vi.fn();
    const cleanup = attachListeners(handler);

    // Initial listeners
    expect(window.visualViewport.addEventListener).toHaveBeenCalledWith('resize', handler, expect.any(Object));
    expect(window.visualViewport.addEventListener).toHaveBeenCalledWith('scroll', handler, expect.any(Object));

    cleanup();
    expect(window.visualViewport.removeEventListener).toHaveBeenCalledWith('resize', handler);
    expect(window.visualViewport.removeEventListener).toHaveBeenCalledWith('scroll', handler);
  });

  test('orientationchange uses a timeout', () => {
    vi.useFakeTimers();
    const handler = vi.fn();
    const cleanup = attachListeners(handler);

    // Simulate event
    const event = new Event('orientationchange');
    window.dispatchEvent(event);

    expect(handler).not.toHaveBeenCalled();
    vi.advanceTimersByTime(200);
    expect(handler).toHaveBeenCalled();
    vi.useRealTimers();
    cleanup();
  });
});


describe('initViewportFix()', () => {
  test('takes an immediate measurement and writes vars', () => {
    const cleanup = initViewportFix();
    expect(document.documentElement.style.getPropertyValue('--vh')).toBe('7.4000px');
    cleanup();
  });

  test('is idempotent with a warning', () => {
    const spyWarn = vi.spyOn(console, 'warn').mockImplementation(() => {});
    const spyAdd = vi.spyOn(window, 'addEventListener');
    const c1 = initViewportFix();
    const c2 = initViewportFix();
    expect(spyWarn).toHaveBeenCalledWith(expect.stringContaining('already initialised'));
    expect(spyAdd).toHaveBeenCalledTimes(2); // resize, orientationchange
    c1();
    c2();
    spyWarn.mockRestore();
    spyAdd.mockRestore();
  });

  test('cleanup removes vars and listeners', () => {
    const cleanup = initViewportFix();
    cleanup();
    expect(document.documentElement.style.getPropertyValue('--vh')).toBe('');
  });
});
