export function clamp(x: number, min: number, max: number) {
  return Math.max(min, Math.min(max, x));
}

// Framerate-independent lerp smoothing, as sourced from the following talk
// by Freya Holmér: https://youtu.be/LSNQuFEDOyQ?si=VqUxBF2r7mfnuba8
export function filerp(current: number, target: number, r: number, dt: number): number {
  return (current - target) * Math.pow(r, dt) + target;
}

export type Falsy = null | undefined | false | 0 | -0 | 0n | "";

export function assert<T>(cond: T | Falsy, msg?: string, soft = false): asserts cond is T {
  if (!cond) {
    if (soft) {
      console.error(msg ?? "Assertion failed");
    } else {
      throw new Error(msg ?? "Assertion failed");
    }
  }
}

export function must<T>(val: T | Falsy, msg?: string): T {
  assert(val, msg);
  return val;
}
