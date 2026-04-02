/**
 * tests/viewport-fix.test.js
 * ─────────────────────────────────────────────────────────────
 * Unit tests for core modules.
 * Run with: node --experimental-vm-modules node_modules/.bin/jest
 * Or:       npx vitest run
 */

import { snapshot }                  from '../src/core/measure.js';
import { writeCSSVars, clearCSSVars } from '../src/core/cssVars.js';
import { createRAFScheduler }         from '../src/utils/scheduler.js';

// ─── Mock browser globals ──────────────────────────────────────────────────

beforeEach(() => {
  // jsdom doesn't implement these — provide minimal stubs
  global.screen = { height: 844 };

  global.CSS = {
    supports: (prop, val) => ['1svh', '1dvh', '1lvh'].includes(val),
  };

  Object.defineProperty(window, 'innerHeight', {
    writable: true,
    configurable: true,
    value: 750,
  });

  // Simulate visualViewport API
  global.window.visualViewport = {
    height:    740,
    offsetTop: 0,
    addEventListener:    jest.fn(),
    removeEventListener: jest.fn(),
  };

  // RAF stub: execute callback synchronously
  global.requestAnimationFrame  = (cb) => { cb(); return 1; };
  global.cancelAnimationFrame   = jest.fn();
});

afterEach(() => {
  document.documentElement.style.removeProperty('--vh');
  document.documentElement.style.removeProperty('--svh');
  delete global.window.visualViewport;
});

// ─── snapshot() ────────────────────────────────────────────────────────────

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

// ─── writeCSSVars() ────────────────────────────────────────────────────────

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

// ─── clearCSSVars() ────────────────────────────────────────────────────────

describe('clearCSSVars()', () => {
  test('removes --vh and --svh from :root', () => {
    writeCSSVars({ visual: 740, stable: 750 });
    clearCSSVars();
    expect(document.documentElement.style.getPropertyValue('--vh')).toBe('');
    expect(document.documentElement.style.getPropertyValue('--svh')).toBe('');
  });
});

// ─── createRAFScheduler() ──────────────────────────────────────────────────

describe('createRAFScheduler()', () => {
  test('calls callback after schedule()', () => {
    const cb = jest.fn();
    const { schedule } = createRAFScheduler(cb);
    schedule();
    expect(cb).toHaveBeenCalledTimes(1);
  });

  test('cancel() prevents pending callback', () => {
    // Override rAF to NOT execute synchronously this time
    let pending = null;
    global.requestAnimationFrame  = (cb) => { pending = cb; return 42; };
    global.cancelAnimationFrame   = jest.fn((id) => { if (id === 42) pending = null; });

    const cb = jest.fn();
    const { schedule, cancel } = createRAFScheduler(cb);
    schedule();
    cancel();
    pending?.(); // if not cancelled, this would call cb
    expect(cb).not.toHaveBeenCalled();
  });

  test('scheduling twice cancels the first frame', () => {
    const calls = [];
    global.requestAnimationFrame = (cb) => { calls.push(cb); return calls.length; };
    global.cancelAnimationFrame  = jest.fn();

    const { schedule } = createRAFScheduler(jest.fn());
    schedule();
    schedule();
    expect(global.cancelAnimationFrame).toHaveBeenCalledTimes(1);
  });
});