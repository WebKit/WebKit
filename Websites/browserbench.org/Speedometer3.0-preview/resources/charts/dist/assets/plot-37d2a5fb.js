import { c as csvParse, a as airportsString, f as flightsString } from "./flights-airports-9a9e6422.js";
function ascending$1(a2, b) {
  return a2 == null || b == null ? NaN : a2 < b ? -1 : a2 > b ? 1 : a2 >= b ? 0 : NaN;
}
function descending(a2, b) {
  return a2 == null || b == null ? NaN : b < a2 ? -1 : b > a2 ? 1 : b >= a2 ? 0 : NaN;
}
function bisector(f) {
  let compare1, compare2, delta;
  if (f.length !== 2) {
    compare1 = ascending$1;
    compare2 = (d, x) => ascending$1(f(d), x);
    delta = (d, x) => f(d) - x;
  } else {
    compare1 = f === ascending$1 || f === descending ? f : zero$1;
    compare2 = f;
    delta = f;
  }
  function left2(a2, x, lo = 0, hi = a2.length) {
    if (lo < hi) {
      if (compare1(x, x) !== 0)
        return hi;
      do {
        const mid2 = lo + hi >>> 1;
        if (compare2(a2[mid2], x) < 0)
          lo = mid2 + 1;
        else
          hi = mid2;
      } while (lo < hi);
    }
    return lo;
  }
  function right2(a2, x, lo = 0, hi = a2.length) {
    if (lo < hi) {
      if (compare1(x, x) !== 0)
        return hi;
      do {
        const mid2 = lo + hi >>> 1;
        if (compare2(a2[mid2], x) <= 0)
          lo = mid2 + 1;
        else
          hi = mid2;
      } while (lo < hi);
    }
    return lo;
  }
  function center2(a2, x, lo = 0, hi = a2.length) {
    const i = left2(a2, x, lo, hi - 1);
    return i > lo && delta(a2[i - 1], x) > -delta(a2[i], x) ? i - 1 : i;
  }
  return { left: left2, center: center2, right: right2 };
}
function zero$1() {
  return 0;
}
function number$4(x) {
  return x === null ? NaN : +x;
}
function* numbers(values2, valueof2) {
  if (valueof2 === void 0) {
    for (let value of values2) {
      if (value != null && (value = +value) >= value) {
        yield value;
      }
    }
  } else {
    let index2 = -1;
    for (let value of values2) {
      if ((value = valueof2(value, ++index2, values2)) != null && (value = +value) >= value) {
        yield value;
      }
    }
  }
}
const ascendingBisect = bisector(ascending$1);
const bisectRight = ascendingBisect.right;
bisector(number$4).center;
const bisect = bisectRight;
function length$1(array2) {
  return array2.length | 0;
}
function empty$1(length2) {
  return !(length2 > 0);
}
function arrayify$1(values2) {
  return typeof values2 !== "object" || "length" in values2 ? values2 : Array.from(values2);
}
function reducer(reduce) {
  return (values2) => reduce(...values2);
}
function cross(...values2) {
  const reduce = typeof values2[values2.length - 1] === "function" && reducer(values2.pop());
  values2 = values2.map(arrayify$1);
  const lengths = values2.map(length$1);
  const j = values2.length - 1;
  const index2 = new Array(j + 1).fill(0);
  const product = [];
  if (j < 0 || lengths.some(empty$1))
    return product;
  while (true) {
    product.push(index2.map((j2, i2) => values2[i2][j2]));
    let i = j;
    while (++index2[i] === lengths[i]) {
      if (i === 0)
        return reduce ? product.map(reduce) : product;
      index2[i--] = 0;
    }
  }
}
function cumsum(values2, valueof2) {
  var sum2 = 0, index2 = 0;
  return Float64Array.from(values2, valueof2 === void 0 ? (v) => sum2 += +v || 0 : (v) => sum2 += +valueof2(v, index2++, values2) || 0);
}
function variance(values2, valueof2) {
  let count = 0;
  let delta;
  let mean2 = 0;
  let sum2 = 0;
  if (valueof2 === void 0) {
    for (let value of values2) {
      if (value != null && (value = +value) >= value) {
        delta = value - mean2;
        mean2 += delta / ++count;
        sum2 += delta * (value - mean2);
      }
    }
  } else {
    let index2 = -1;
    for (let value of values2) {
      if ((value = valueof2(value, ++index2, values2)) != null && (value = +value) >= value) {
        delta = value - mean2;
        mean2 += delta / ++count;
        sum2 += delta * (value - mean2);
      }
    }
  }
  if (count > 1)
    return sum2 / (count - 1);
}
function deviation(values2, valueof2) {
  const v = variance(values2, valueof2);
  return v ? Math.sqrt(v) : v;
}
function extent$1(values2, valueof2) {
  let min2;
  let max2;
  if (valueof2 === void 0) {
    for (const value of values2) {
      if (value != null) {
        if (min2 === void 0) {
          if (value >= value)
            min2 = max2 = value;
        } else {
          if (min2 > value)
            min2 = value;
          if (max2 < value)
            max2 = value;
        }
      }
    }
  } else {
    let index2 = -1;
    for (let value of values2) {
      if ((value = valueof2(value, ++index2, values2)) != null) {
        if (min2 === void 0) {
          if (value >= value)
            min2 = max2 = value;
        } else {
          if (min2 > value)
            min2 = value;
          if (max2 < value)
            max2 = value;
        }
      }
    }
  }
  return [min2, max2];
}
class Adder {
  constructor() {
    this._partials = new Float64Array(32);
    this._n = 0;
  }
  add(x) {
    const p = this._partials;
    let i = 0;
    for (let j = 0; j < this._n && j < 32; j++) {
      const y = p[j], hi = x + y, lo = Math.abs(x) < Math.abs(y) ? x - (hi - y) : y - (hi - x);
      if (lo)
        p[i++] = lo;
      x = hi;
    }
    p[i] = x;
    this._n = i + 1;
    return this;
  }
  valueOf() {
    const p = this._partials;
    let n = this._n, x, y, lo, hi = 0;
    if (n > 0) {
      hi = p[--n];
      while (n > 0) {
        x = hi;
        y = p[--n];
        hi = x + y;
        lo = y - (hi - x);
        if (lo)
          break;
      }
      if (n > 0 && (lo < 0 && p[n - 1] < 0 || lo > 0 && p[n - 1] > 0)) {
        y = lo * 2;
        x = hi + y;
        if (y == x - hi)
          hi = x;
      }
    }
    return hi;
  }
}
class InternMap extends Map {
  constructor(entries, key = keyof) {
    super();
    Object.defineProperties(this, { _intern: { value: /* @__PURE__ */ new Map() }, _key: { value: key } });
    if (entries != null)
      for (const [key2, value] of entries)
        this.set(key2, value);
  }
  get(key) {
    return super.get(intern_get(this, key));
  }
  has(key) {
    return super.has(intern_get(this, key));
  }
  set(key, value) {
    return super.set(intern_set(this, key), value);
  }
  delete(key) {
    return super.delete(intern_delete(this, key));
  }
}
class InternSet extends Set {
  constructor(values2, key = keyof) {
    super();
    Object.defineProperties(this, { _intern: { value: /* @__PURE__ */ new Map() }, _key: { value: key } });
    if (values2 != null)
      for (const value of values2)
        this.add(value);
  }
  has(value) {
    return super.has(intern_get(this, value));
  }
  add(value) {
    return super.add(intern_set(this, value));
  }
  delete(value) {
    return super.delete(intern_delete(this, value));
  }
}
function intern_get({ _intern, _key }, value) {
  const key = _key(value);
  return _intern.has(key) ? _intern.get(key) : value;
}
function intern_set({ _intern, _key }, value) {
  const key = _key(value);
  if (_intern.has(key))
    return _intern.get(key);
  _intern.set(key, value);
  return value;
}
function intern_delete({ _intern, _key }, value) {
  const key = _key(value);
  if (_intern.has(key)) {
    value = _intern.get(key);
    _intern.delete(key);
  }
  return value;
}
function keyof(value) {
  return value !== null && typeof value === "object" ? value.valueOf() : value;
}
function identity$8(x) {
  return x;
}
function group(values2, ...keys) {
  return nest(values2, identity$8, identity$8, keys);
}
function groups(values2, ...keys) {
  return nest(values2, Array.from, identity$8, keys);
}
function rollup(values2, reduce, ...keys) {
  return nest(values2, identity$8, reduce, keys);
}
function index(values2, ...keys) {
  return nest(values2, identity$8, unique, keys);
}
function unique(values2) {
  if (values2.length !== 1)
    throw new Error("duplicate key");
  return values2[0];
}
function nest(values2, map2, reduce, keys) {
  return function regroup(values3, i) {
    if (i >= keys.length)
      return reduce(values3);
    const groups2 = new InternMap();
    const keyof2 = keys[i++];
    let index2 = -1;
    for (const value of values3) {
      const key = keyof2(value, ++index2, values3);
      const group2 = groups2.get(key);
      if (group2)
        group2.push(value);
      else
        groups2.set(key, [value]);
    }
    for (const [key, values4] of groups2) {
      groups2.set(key, regroup(values4, i));
    }
    return map2(groups2);
  }(values2, 0);
}
function permute(source, keys) {
  return Array.from(keys, (key) => source[key]);
}
function sort$1(values2, ...F) {
  if (typeof values2[Symbol.iterator] !== "function")
    throw new TypeError("values is not iterable");
  values2 = Array.from(values2);
  let [f] = F;
  if (f && f.length !== 2 || F.length > 1) {
    const index2 = Uint32Array.from(values2, (d, i) => i);
    if (F.length > 1) {
      F = F.map((f2) => values2.map(f2));
      index2.sort((i, j) => {
        for (const f2 of F) {
          const c2 = ascendingDefined$1(f2[i], f2[j]);
          if (c2)
            return c2;
        }
      });
    } else {
      f = values2.map(f);
      index2.sort((i, j) => ascendingDefined$1(f[i], f[j]));
    }
    return permute(values2, index2);
  }
  return values2.sort(compareDefined(f));
}
function compareDefined(compare = ascending$1) {
  if (compare === ascending$1)
    return ascendingDefined$1;
  if (typeof compare !== "function")
    throw new TypeError("compare is not a function");
  return (a2, b) => {
    const x = compare(a2, b);
    if (x || x === 0)
      return x;
    return (compare(b, b) === 0) - (compare(a2, a2) === 0);
  };
}
function ascendingDefined$1(a2, b) {
  return (a2 == null || !(a2 >= a2)) - (b == null || !(b >= b)) || (a2 < b ? -1 : a2 > b ? 1 : 0);
}
function groupSort(values2, reduce, key) {
  return (reduce.length !== 2 ? sort$1(rollup(values2, reduce, key), ([ak, av], [bk, bv]) => ascending$1(av, bv) || ascending$1(ak, bk)) : sort$1(group(values2, key), ([ak, av], [bk, bv]) => reduce(av, bv) || ascending$1(ak, bk))).map(([key2]) => key2);
}
const e10 = Math.sqrt(50), e5 = Math.sqrt(10), e2 = Math.sqrt(2);
function tickSpec(start2, stop, count) {
  const step = (stop - start2) / Math.max(0, count), power = Math.floor(Math.log10(step)), error = step / Math.pow(10, power), factor = error >= e10 ? 10 : error >= e5 ? 5 : error >= e2 ? 2 : 1;
  let i1, i2, inc;
  if (power < 0) {
    inc = Math.pow(10, -power) / factor;
    i1 = Math.round(start2 * inc);
    i2 = Math.round(stop * inc);
    if (i1 / inc < start2)
      ++i1;
    if (i2 / inc > stop)
      --i2;
    inc = -inc;
  } else {
    inc = Math.pow(10, power) * factor;
    i1 = Math.round(start2 / inc);
    i2 = Math.round(stop / inc);
    if (i1 * inc < start2)
      ++i1;
    if (i2 * inc > stop)
      --i2;
  }
  if (i2 < i1 && 0.5 <= count && count < 2)
    return tickSpec(start2, stop, count * 2);
  return [i1, i2, inc];
}
function ticks(start2, stop, count) {
  stop = +stop, start2 = +start2, count = +count;
  if (!(count > 0))
    return [];
  if (start2 === stop)
    return [start2];
  const reverse2 = stop < start2, [i1, i2, inc] = reverse2 ? tickSpec(stop, start2, count) : tickSpec(start2, stop, count);
  if (!(i2 >= i1))
    return [];
  const n = i2 - i1 + 1, ticks2 = new Array(n);
  if (reverse2) {
    if (inc < 0)
      for (let i = 0; i < n; ++i)
        ticks2[i] = (i2 - i) / -inc;
    else
      for (let i = 0; i < n; ++i)
        ticks2[i] = (i2 - i) * inc;
  } else {
    if (inc < 0)
      for (let i = 0; i < n; ++i)
        ticks2[i] = (i1 + i) / -inc;
    else
      for (let i = 0; i < n; ++i)
        ticks2[i] = (i1 + i) * inc;
  }
  return ticks2;
}
function tickIncrement(start2, stop, count) {
  stop = +stop, start2 = +start2, count = +count;
  return tickSpec(start2, stop, count)[2];
}
function tickStep(start2, stop, count) {
  stop = +stop, start2 = +start2, count = +count;
  const reverse2 = stop < start2, inc = reverse2 ? tickIncrement(stop, start2, count) : tickIncrement(start2, stop, count);
  return (reverse2 ? -1 : 1) * (inc < 0 ? 1 / -inc : inc);
}
function max(values2, valueof2) {
  let max2;
  if (valueof2 === void 0) {
    for (const value of values2) {
      if (value != null && (max2 < value || max2 === void 0 && value >= value)) {
        max2 = value;
      }
    }
  } else {
    let index2 = -1;
    for (let value of values2) {
      if ((value = valueof2(value, ++index2, values2)) != null && (max2 < value || max2 === void 0 && value >= value)) {
        max2 = value;
      }
    }
  }
  return max2;
}
function maxIndex(values2, valueof2) {
  let max2;
  let maxIndex2 = -1;
  let index2 = -1;
  if (valueof2 === void 0) {
    for (const value of values2) {
      ++index2;
      if (value != null && (max2 < value || max2 === void 0 && value >= value)) {
        max2 = value, maxIndex2 = index2;
      }
    }
  } else {
    for (let value of values2) {
      if ((value = valueof2(value, ++index2, values2)) != null && (max2 < value || max2 === void 0 && value >= value)) {
        max2 = value, maxIndex2 = index2;
      }
    }
  }
  return maxIndex2;
}
function min$1(values2, valueof2) {
  let min2;
  if (valueof2 === void 0) {
    for (const value of values2) {
      if (value != null && (min2 > value || min2 === void 0 && value >= value)) {
        min2 = value;
      }
    }
  } else {
    let index2 = -1;
    for (let value of values2) {
      if ((value = valueof2(value, ++index2, values2)) != null && (min2 > value || min2 === void 0 && value >= value)) {
        min2 = value;
      }
    }
  }
  return min2;
}
function minIndex(values2, valueof2) {
  let min2;
  let minIndex2 = -1;
  let index2 = -1;
  if (valueof2 === void 0) {
    for (const value of values2) {
      ++index2;
      if (value != null && (min2 > value || min2 === void 0 && value >= value)) {
        min2 = value, minIndex2 = index2;
      }
    }
  } else {
    for (let value of values2) {
      if ((value = valueof2(value, ++index2, values2)) != null && (min2 > value || min2 === void 0 && value >= value)) {
        min2 = value, minIndex2 = index2;
      }
    }
  }
  return minIndex2;
}
function quickselect(array2, k2, left2 = 0, right2 = Infinity, compare) {
  k2 = Math.floor(k2);
  left2 = Math.floor(Math.max(0, left2));
  right2 = Math.floor(Math.min(array2.length - 1, right2));
  if (!(left2 <= k2 && k2 <= right2))
    return array2;
  compare = compare === void 0 ? ascendingDefined$1 : compareDefined(compare);
  while (right2 > left2) {
    if (right2 - left2 > 600) {
      const n = right2 - left2 + 1;
      const m = k2 - left2 + 1;
      const z = Math.log(n);
      const s2 = 0.5 * Math.exp(2 * z / 3);
      const sd = 0.5 * Math.sqrt(z * s2 * (n - s2) / n) * (m - n / 2 < 0 ? -1 : 1);
      const newLeft = Math.max(left2, Math.floor(k2 - m * s2 / n + sd));
      const newRight = Math.min(right2, Math.floor(k2 + (n - m) * s2 / n + sd));
      quickselect(array2, k2, newLeft, newRight, compare);
    }
    const t = array2[k2];
    let i = left2;
    let j = right2;
    swap(array2, left2, k2);
    if (compare(array2[right2], t) > 0)
      swap(array2, left2, right2);
    while (i < j) {
      swap(array2, i, j), ++i, --j;
      while (compare(array2[i], t) < 0)
        ++i;
      while (compare(array2[j], t) > 0)
        --j;
    }
    if (compare(array2[left2], t) === 0)
      swap(array2, left2, j);
    else
      ++j, swap(array2, j, right2);
    if (j <= k2)
      left2 = j + 1;
    if (k2 <= j)
      right2 = j - 1;
  }
  return array2;
}
function swap(array2, i, j) {
  const t = array2[i];
  array2[i] = array2[j];
  array2[j] = t;
}
function greatest(values2, compare = ascending$1) {
  let max2;
  let defined2 = false;
  if (compare.length === 1) {
    let maxValue;
    for (const element of values2) {
      const value = compare(element);
      if (defined2 ? ascending$1(value, maxValue) > 0 : ascending$1(value, value) === 0) {
        max2 = element;
        maxValue = value;
        defined2 = true;
      }
    }
  } else {
    for (const value of values2) {
      if (defined2 ? compare(value, max2) > 0 : compare(value, value) === 0) {
        max2 = value;
        defined2 = true;
      }
    }
  }
  return max2;
}
function quantile$1(values2, p, valueof2) {
  values2 = Float64Array.from(numbers(values2, valueof2));
  if (!(n = values2.length) || isNaN(p = +p))
    return;
  if (p <= 0 || n < 2)
    return min$1(values2);
  if (p >= 1)
    return max(values2);
  var n, i = (n - 1) * p, i0 = Math.floor(i), value0 = max(quickselect(values2, i0).subarray(0, i0 + 1)), value1 = min$1(values2.subarray(i0 + 1));
  return value0 + (value1 - value0) * (i - i0);
}
function quantileSorted(values2, p, valueof2 = number$4) {
  if (!(n = values2.length) || isNaN(p = +p))
    return;
  if (p <= 0 || n < 2)
    return +valueof2(values2[0], 0, values2);
  if (p >= 1)
    return +valueof2(values2[n - 1], n - 1, values2);
  var n, i = (n - 1) * p, i0 = Math.floor(i), value0 = +valueof2(values2[i0], i0, values2), value1 = +valueof2(values2[i0 + 1], i0 + 1, values2);
  return value0 + (value1 - value0) * (i - i0);
}
function mean(values2, valueof2) {
  let count = 0;
  let sum2 = 0;
  if (valueof2 === void 0) {
    for (let value of values2) {
      if (value != null && (value = +value) >= value) {
        ++count, sum2 += value;
      }
    }
  } else {
    let index2 = -1;
    for (let value of values2) {
      if ((value = valueof2(value, ++index2, values2)) != null && (value = +value) >= value) {
        ++count, sum2 += value;
      }
    }
  }
  if (count)
    return sum2 / count;
}
function median(values2, valueof2) {
  return quantile$1(values2, 0.5, valueof2);
}
function* flatten(arrays) {
  for (const array2 of arrays) {
    yield* array2;
  }
}
function merge(arrays) {
  return Array.from(flatten(arrays));
}
function mode(values2, valueof2) {
  const counts = new InternMap();
  if (valueof2 === void 0) {
    for (let value of values2) {
      if (value != null && value >= value) {
        counts.set(value, (counts.get(value) || 0) + 1);
      }
    }
  } else {
    let index2 = -1;
    for (let value of values2) {
      if ((value = valueof2(value, ++index2, values2)) != null && value >= value) {
        counts.set(value, (counts.get(value) || 0) + 1);
      }
    }
  }
  let modeValue;
  let modeCount = 0;
  for (const [value, count] of counts) {
    if (count > modeCount) {
      modeCount = count;
      modeValue = value;
    }
  }
  return modeValue;
}
function range$1(start2, stop, step) {
  start2 = +start2, stop = +stop, step = (n = arguments.length) < 2 ? (stop = start2, start2 = 0, 1) : n < 3 ? 1 : +step;
  var i = -1, n = Math.max(0, Math.ceil((stop - start2) / step)) | 0, range2 = new Array(n);
  while (++i < n) {
    range2[i] = start2 + i * step;
  }
  return range2;
}
function sum(values2, valueof2) {
  let sum2 = 0;
  if (valueof2 === void 0) {
    for (let value of values2) {
      if (value = +value) {
        sum2 += value;
      }
    }
  } else {
    let index2 = -1;
    for (let value of values2) {
      if (value = +valueof2(value, ++index2, values2)) {
        sum2 += value;
      }
    }
  }
  return sum2;
}
function reverse(values2) {
  if (typeof values2[Symbol.iterator] !== "function")
    throw new TypeError("values is not iterable");
  return Array.from(values2).reverse();
}
function identity$7(x) {
  return x;
}
var top = 1, right = 2, bottom = 3, left = 4, epsilon$2 = 1e-6;
function translateX(x) {
  return "translate(" + x + ",0)";
}
function translateY(y) {
  return "translate(0," + y + ")";
}
function number$3(scale) {
  return (d) => +scale(d);
}
function center(scale, offset2) {
  offset2 = Math.max(0, scale.bandwidth() - offset2 * 2) / 2;
  if (scale.round())
    offset2 = Math.round(offset2);
  return (d) => +scale(d) + offset2;
}
function entering() {
  return !this.__axis;
}
function axis(orient, scale) {
  var tickArguments = [], tickValues = null, tickFormat2 = null, tickSizeInner = 6, tickSizeOuter = 6, tickPadding = 3, offset2 = typeof window !== "undefined" && window.devicePixelRatio > 1 ? 0 : 0.5, k2 = orient === top || orient === left ? -1 : 1, x = orient === left || orient === right ? "x" : "y", transform = orient === top || orient === bottom ? translateX : translateY;
  function axis2(context) {
    var values2 = tickValues == null ? scale.ticks ? scale.ticks.apply(scale, tickArguments) : scale.domain() : tickValues, format2 = tickFormat2 == null ? scale.tickFormat ? scale.tickFormat.apply(scale, tickArguments) : identity$7 : tickFormat2, spacing = Math.max(tickSizeInner, 0) + tickPadding, range2 = scale.range(), range0 = +range2[0] + offset2, range1 = +range2[range2.length - 1] + offset2, position2 = (scale.bandwidth ? center : number$3)(scale.copy(), offset2), selection2 = context.selection ? context.selection() : context, path = selection2.selectAll(".domain").data([null]), tick = selection2.selectAll(".tick").data(values2, scale).order(), tickExit = tick.exit(), tickEnter = tick.enter().append("g").attr("class", "tick"), line = tick.select("line"), text2 = tick.select("text");
    path = path.merge(path.enter().insert("path", ".tick").attr("class", "domain").attr("stroke", "currentColor"));
    tick = tick.merge(tickEnter);
    line = line.merge(tickEnter.append("line").attr("stroke", "currentColor").attr(x + "2", k2 * tickSizeInner));
    text2 = text2.merge(tickEnter.append("text").attr("fill", "currentColor").attr(x, k2 * spacing).attr("dy", orient === top ? "0em" : orient === bottom ? "0.71em" : "0.32em"));
    if (context !== selection2) {
      path = path.transition(context);
      tick = tick.transition(context);
      line = line.transition(context);
      text2 = text2.transition(context);
      tickExit = tickExit.transition(context).attr("opacity", epsilon$2).attr("transform", function(d) {
        return isFinite(d = position2(d)) ? transform(d + offset2) : this.getAttribute("transform");
      });
      tickEnter.attr("opacity", epsilon$2).attr("transform", function(d) {
        var p = this.parentNode.__axis;
        return transform((p && isFinite(p = p(d)) ? p : position2(d)) + offset2);
      });
    }
    tickExit.remove();
    path.attr("d", orient === left || orient === right ? tickSizeOuter ? "M" + k2 * tickSizeOuter + "," + range0 + "H" + offset2 + "V" + range1 + "H" + k2 * tickSizeOuter : "M" + offset2 + "," + range0 + "V" + range1 : tickSizeOuter ? "M" + range0 + "," + k2 * tickSizeOuter + "V" + offset2 + "H" + range1 + "V" + k2 * tickSizeOuter : "M" + range0 + "," + offset2 + "H" + range1);
    tick.attr("opacity", 1).attr("transform", function(d) {
      return transform(position2(d) + offset2);
    });
    line.attr(x + "2", k2 * tickSizeInner);
    text2.attr(x, k2 * spacing).text(format2);
    selection2.filter(entering).attr("fill", "none").attr("font-size", 10).attr("font-family", "sans-serif").attr("text-anchor", orient === right ? "start" : orient === left ? "end" : "middle");
    selection2.each(function() {
      this.__axis = position2;
    });
  }
  axis2.scale = function(_) {
    return arguments.length ? (scale = _, axis2) : scale;
  };
  axis2.ticks = function() {
    return tickArguments = Array.from(arguments), axis2;
  };
  axis2.tickArguments = function(_) {
    return arguments.length ? (tickArguments = _ == null ? [] : Array.from(_), axis2) : tickArguments.slice();
  };
  axis2.tickValues = function(_) {
    return arguments.length ? (tickValues = _ == null ? null : Array.from(_), axis2) : tickValues && tickValues.slice();
  };
  axis2.tickFormat = function(_) {
    return arguments.length ? (tickFormat2 = _, axis2) : tickFormat2;
  };
  axis2.tickSize = function(_) {
    return arguments.length ? (tickSizeInner = tickSizeOuter = +_, axis2) : tickSizeInner;
  };
  axis2.tickSizeInner = function(_) {
    return arguments.length ? (tickSizeInner = +_, axis2) : tickSizeInner;
  };
  axis2.tickSizeOuter = function(_) {
    return arguments.length ? (tickSizeOuter = +_, axis2) : tickSizeOuter;
  };
  axis2.tickPadding = function(_) {
    return arguments.length ? (tickPadding = +_, axis2) : tickPadding;
  };
  axis2.offset = function(_) {
    return arguments.length ? (offset2 = +_, axis2) : offset2;
  };
  return axis2;
}
function axisBottom(scale) {
  return axis(bottom, scale);
}
var noop$1 = { value: () => {
} };
function dispatch() {
  for (var i = 0, n = arguments.length, _ = {}, t; i < n; ++i) {
    if (!(t = arguments[i] + "") || t in _ || /[\s.]/.test(t))
      throw new Error("illegal type: " + t);
    _[t] = [];
  }
  return new Dispatch(_);
}
function Dispatch(_) {
  this._ = _;
}
function parseTypenames$1(typenames, types) {
  return typenames.trim().split(/^|\s+/).map(function(t) {
    var name = "", i = t.indexOf(".");
    if (i >= 0)
      name = t.slice(i + 1), t = t.slice(0, i);
    if (t && !types.hasOwnProperty(t))
      throw new Error("unknown type: " + t);
    return { type: t, name };
  });
}
Dispatch.prototype = dispatch.prototype = {
  constructor: Dispatch,
  on: function(typename, callback) {
    var _ = this._, T = parseTypenames$1(typename + "", _), t, i = -1, n = T.length;
    if (arguments.length < 2) {
      while (++i < n)
        if ((t = (typename = T[i]).type) && (t = get$1(_[t], typename.name)))
          return t;
      return;
    }
    if (callback != null && typeof callback !== "function")
      throw new Error("invalid callback: " + callback);
    while (++i < n) {
      if (t = (typename = T[i]).type)
        _[t] = set$1(_[t], typename.name, callback);
      else if (callback == null)
        for (t in _)
          _[t] = set$1(_[t], typename.name, null);
    }
    return this;
  },
  copy: function() {
    var copy2 = {}, _ = this._;
    for (var t in _)
      copy2[t] = _[t].slice();
    return new Dispatch(copy2);
  },
  call: function(type, that) {
    if ((n = arguments.length - 2) > 0)
      for (var args = new Array(n), i = 0, n, t; i < n; ++i)
        args[i] = arguments[i + 2];
    if (!this._.hasOwnProperty(type))
      throw new Error("unknown type: " + type);
    for (t = this._[type], i = 0, n = t.length; i < n; ++i)
      t[i].value.apply(that, args);
  },
  apply: function(type, that, args) {
    if (!this._.hasOwnProperty(type))
      throw new Error("unknown type: " + type);
    for (var t = this._[type], i = 0, n = t.length; i < n; ++i)
      t[i].value.apply(that, args);
  }
};
function get$1(type, name) {
  for (var i = 0, n = type.length, c2; i < n; ++i) {
    if ((c2 = type[i]).name === name) {
      return c2.value;
    }
  }
}
function set$1(type, name, callback) {
  for (var i = 0, n = type.length; i < n; ++i) {
    if (type[i].name === name) {
      type[i] = noop$1, type = type.slice(0, i).concat(type.slice(i + 1));
      break;
    }
  }
  if (callback != null)
    type.push({ name, value: callback });
  return type;
}
var xhtml = "http://www.w3.org/1999/xhtml";
const namespaces = {
  svg: "http://www.w3.org/2000/svg",
  xhtml,
  xlink: "http://www.w3.org/1999/xlink",
  xml: "http://www.w3.org/XML/1998/namespace",
  xmlns: "http://www.w3.org/2000/xmlns/"
};
function namespace(name) {
  var prefix = name += "", i = prefix.indexOf(":");
  if (i >= 0 && (prefix = name.slice(0, i)) !== "xmlns")
    name = name.slice(i + 1);
  return namespaces.hasOwnProperty(prefix) ? { space: namespaces[prefix], local: name } : name;
}
function creatorInherit(name) {
  return function() {
    var document2 = this.ownerDocument, uri = this.namespaceURI;
    return uri === xhtml && document2.documentElement.namespaceURI === xhtml ? document2.createElement(name) : document2.createElementNS(uri, name);
  };
}
function creatorFixed(fullname) {
  return function() {
    return this.ownerDocument.createElementNS(fullname.space, fullname.local);
  };
}
function creator(name) {
  var fullname = namespace(name);
  return (fullname.local ? creatorFixed : creatorInherit)(fullname);
}
function none() {
}
function selector(selector2) {
  return selector2 == null ? none : function() {
    return this.querySelector(selector2);
  };
}
function selection_select(select2) {
  if (typeof select2 !== "function")
    select2 = selector(select2);
  for (var groups2 = this._groups, m = groups2.length, subgroups = new Array(m), j = 0; j < m; ++j) {
    for (var group2 = groups2[j], n = group2.length, subgroup = subgroups[j] = new Array(n), node, subnode, i = 0; i < n; ++i) {
      if ((node = group2[i]) && (subnode = select2.call(node, node.__data__, i, group2))) {
        if ("__data__" in node)
          subnode.__data__ = node.__data__;
        subgroup[i] = subnode;
      }
    }
  }
  return new Selection$1(subgroups, this._parents);
}
function array(x) {
  return x == null ? [] : Array.isArray(x) ? x : Array.from(x);
}
function empty() {
  return [];
}
function selectorAll(selector2) {
  return selector2 == null ? empty : function() {
    return this.querySelectorAll(selector2);
  };
}
function arrayAll(select2) {
  return function() {
    return array(select2.apply(this, arguments));
  };
}
function selection_selectAll(select2) {
  if (typeof select2 === "function")
    select2 = arrayAll(select2);
  else
    select2 = selectorAll(select2);
  for (var groups2 = this._groups, m = groups2.length, subgroups = [], parents = [], j = 0; j < m; ++j) {
    for (var group2 = groups2[j], n = group2.length, node, i = 0; i < n; ++i) {
      if (node = group2[i]) {
        subgroups.push(select2.call(node, node.__data__, i, group2));
        parents.push(node);
      }
    }
  }
  return new Selection$1(subgroups, parents);
}
function matcher(selector2) {
  return function() {
    return this.matches(selector2);
  };
}
function childMatcher(selector2) {
  return function(node) {
    return node.matches(selector2);
  };
}
var find = Array.prototype.find;
function childFind(match) {
  return function() {
    return find.call(this.children, match);
  };
}
function childFirst() {
  return this.firstElementChild;
}
function selection_selectChild(match) {
  return this.select(match == null ? childFirst : childFind(typeof match === "function" ? match : childMatcher(match)));
}
var filter = Array.prototype.filter;
function children() {
  return Array.from(this.children);
}
function childrenFilter(match) {
  return function() {
    return filter.call(this.children, match);
  };
}
function selection_selectChildren(match) {
  return this.selectAll(match == null ? children : childrenFilter(typeof match === "function" ? match : childMatcher(match)));
}
function selection_filter(match) {
  if (typeof match !== "function")
    match = matcher(match);
  for (var groups2 = this._groups, m = groups2.length, subgroups = new Array(m), j = 0; j < m; ++j) {
    for (var group2 = groups2[j], n = group2.length, subgroup = subgroups[j] = [], node, i = 0; i < n; ++i) {
      if ((node = group2[i]) && match.call(node, node.__data__, i, group2)) {
        subgroup.push(node);
      }
    }
  }
  return new Selection$1(subgroups, this._parents);
}
function sparse(update) {
  return new Array(update.length);
}
function selection_enter() {
  return new Selection$1(this._enter || this._groups.map(sparse), this._parents);
}
function EnterNode(parent, datum2) {
  this.ownerDocument = parent.ownerDocument;
  this.namespaceURI = parent.namespaceURI;
  this._next = null;
  this._parent = parent;
  this.__data__ = datum2;
}
EnterNode.prototype = {
  constructor: EnterNode,
  appendChild: function(child) {
    return this._parent.insertBefore(child, this._next);
  },
  insertBefore: function(child, next) {
    return this._parent.insertBefore(child, next);
  },
  querySelector: function(selector2) {
    return this._parent.querySelector(selector2);
  },
  querySelectorAll: function(selector2) {
    return this._parent.querySelectorAll(selector2);
  }
};
function constant$2(x) {
  return function() {
    return x;
  };
}
function bindIndex(parent, group2, enter, update, exit, data) {
  var i = 0, node, groupLength = group2.length, dataLength = data.length;
  for (; i < dataLength; ++i) {
    if (node = group2[i]) {
      node.__data__ = data[i];
      update[i] = node;
    } else {
      enter[i] = new EnterNode(parent, data[i]);
    }
  }
  for (; i < groupLength; ++i) {
    if (node = group2[i]) {
      exit[i] = node;
    }
  }
}
function bindKey(parent, group2, enter, update, exit, data, key) {
  var i, node, nodeByKeyValue = /* @__PURE__ */ new Map(), groupLength = group2.length, dataLength = data.length, keyValues = new Array(groupLength), keyValue;
  for (i = 0; i < groupLength; ++i) {
    if (node = group2[i]) {
      keyValues[i] = keyValue = key.call(node, node.__data__, i, group2) + "";
      if (nodeByKeyValue.has(keyValue)) {
        exit[i] = node;
      } else {
        nodeByKeyValue.set(keyValue, node);
      }
    }
  }
  for (i = 0; i < dataLength; ++i) {
    keyValue = key.call(parent, data[i], i, data) + "";
    if (node = nodeByKeyValue.get(keyValue)) {
      update[i] = node;
      node.__data__ = data[i];
      nodeByKeyValue.delete(keyValue);
    } else {
      enter[i] = new EnterNode(parent, data[i]);
    }
  }
  for (i = 0; i < groupLength; ++i) {
    if ((node = group2[i]) && nodeByKeyValue.get(keyValues[i]) === node) {
      exit[i] = node;
    }
  }
}
function datum(node) {
  return node.__data__;
}
function selection_data(value, key) {
  if (!arguments.length)
    return Array.from(this, datum);
  var bind = key ? bindKey : bindIndex, parents = this._parents, groups2 = this._groups;
  if (typeof value !== "function")
    value = constant$2(value);
  for (var m = groups2.length, update = new Array(m), enter = new Array(m), exit = new Array(m), j = 0; j < m; ++j) {
    var parent = parents[j], group2 = groups2[j], groupLength = group2.length, data = arraylike(value.call(parent, parent && parent.__data__, j, parents)), dataLength = data.length, enterGroup = enter[j] = new Array(dataLength), updateGroup = update[j] = new Array(dataLength), exitGroup = exit[j] = new Array(groupLength);
    bind(parent, group2, enterGroup, updateGroup, exitGroup, data, key);
    for (var i0 = 0, i1 = 0, previous, next; i0 < dataLength; ++i0) {
      if (previous = enterGroup[i0]) {
        if (i0 >= i1)
          i1 = i0 + 1;
        while (!(next = updateGroup[i1]) && ++i1 < dataLength)
          ;
        previous._next = next || null;
      }
    }
  }
  update = new Selection$1(update, parents);
  update._enter = enter;
  update._exit = exit;
  return update;
}
function arraylike(data) {
  return typeof data === "object" && "length" in data ? data : Array.from(data);
}
function selection_exit() {
  return new Selection$1(this._exit || this._groups.map(sparse), this._parents);
}
function selection_join(onenter, onupdate, onexit) {
  var enter = this.enter(), update = this, exit = this.exit();
  if (typeof onenter === "function") {
    enter = onenter(enter);
    if (enter)
      enter = enter.selection();
  } else {
    enter = enter.append(onenter + "");
  }
  if (onupdate != null) {
    update = onupdate(update);
    if (update)
      update = update.selection();
  }
  if (onexit == null)
    exit.remove();
  else
    onexit(exit);
  return enter && update ? enter.merge(update).order() : update;
}
function selection_merge(context) {
  var selection2 = context.selection ? context.selection() : context;
  for (var groups0 = this._groups, groups1 = selection2._groups, m0 = groups0.length, m1 = groups1.length, m = Math.min(m0, m1), merges = new Array(m0), j = 0; j < m; ++j) {
    for (var group0 = groups0[j], group1 = groups1[j], n = group0.length, merge2 = merges[j] = new Array(n), node, i = 0; i < n; ++i) {
      if (node = group0[i] || group1[i]) {
        merge2[i] = node;
      }
    }
  }
  for (; j < m0; ++j) {
    merges[j] = groups0[j];
  }
  return new Selection$1(merges, this._parents);
}
function selection_order() {
  for (var groups2 = this._groups, j = -1, m = groups2.length; ++j < m; ) {
    for (var group2 = groups2[j], i = group2.length - 1, next = group2[i], node; --i >= 0; ) {
      if (node = group2[i]) {
        if (next && node.compareDocumentPosition(next) ^ 4)
          next.parentNode.insertBefore(node, next);
        next = node;
      }
    }
  }
  return this;
}
function selection_sort(compare) {
  if (!compare)
    compare = ascending;
  function compareNode(a2, b) {
    return a2 && b ? compare(a2.__data__, b.__data__) : !a2 - !b;
  }
  for (var groups2 = this._groups, m = groups2.length, sortgroups = new Array(m), j = 0; j < m; ++j) {
    for (var group2 = groups2[j], n = group2.length, sortgroup = sortgroups[j] = new Array(n), node, i = 0; i < n; ++i) {
      if (node = group2[i]) {
        sortgroup[i] = node;
      }
    }
    sortgroup.sort(compareNode);
  }
  return new Selection$1(sortgroups, this._parents).order();
}
function ascending(a2, b) {
  return a2 < b ? -1 : a2 > b ? 1 : a2 >= b ? 0 : NaN;
}
function selection_call() {
  var callback = arguments[0];
  arguments[0] = this;
  callback.apply(null, arguments);
  return this;
}
function selection_nodes() {
  return Array.from(this);
}
function selection_node() {
  for (var groups2 = this._groups, j = 0, m = groups2.length; j < m; ++j) {
    for (var group2 = groups2[j], i = 0, n = group2.length; i < n; ++i) {
      var node = group2[i];
      if (node)
        return node;
    }
  }
  return null;
}
function selection_size() {
  let size = 0;
  for (const node of this)
    ++size;
  return size;
}
function selection_empty() {
  return !this.node();
}
function selection_each(callback) {
  for (var groups2 = this._groups, j = 0, m = groups2.length; j < m; ++j) {
    for (var group2 = groups2[j], i = 0, n = group2.length, node; i < n; ++i) {
      if (node = group2[i])
        callback.call(node, node.__data__, i, group2);
    }
  }
  return this;
}
function attrRemove$1(name) {
  return function() {
    this.removeAttribute(name);
  };
}
function attrRemoveNS$1(fullname) {
  return function() {
    this.removeAttributeNS(fullname.space, fullname.local);
  };
}
function attrConstant$1(name, value) {
  return function() {
    this.setAttribute(name, value);
  };
}
function attrConstantNS$1(fullname, value) {
  return function() {
    this.setAttributeNS(fullname.space, fullname.local, value);
  };
}
function attrFunction$1(name, value) {
  return function() {
    var v = value.apply(this, arguments);
    if (v == null)
      this.removeAttribute(name);
    else
      this.setAttribute(name, v);
  };
}
function attrFunctionNS$1(fullname, value) {
  return function() {
    var v = value.apply(this, arguments);
    if (v == null)
      this.removeAttributeNS(fullname.space, fullname.local);
    else
      this.setAttributeNS(fullname.space, fullname.local, v);
  };
}
function selection_attr(name, value) {
  var fullname = namespace(name);
  if (arguments.length < 2) {
    var node = this.node();
    return fullname.local ? node.getAttributeNS(fullname.space, fullname.local) : node.getAttribute(fullname);
  }
  return this.each((value == null ? fullname.local ? attrRemoveNS$1 : attrRemove$1 : typeof value === "function" ? fullname.local ? attrFunctionNS$1 : attrFunction$1 : fullname.local ? attrConstantNS$1 : attrConstant$1)(fullname, value));
}
function defaultView(node) {
  return node.ownerDocument && node.ownerDocument.defaultView || node.document && node || node.defaultView;
}
function styleRemove$1(name) {
  return function() {
    this.style.removeProperty(name);
  };
}
function styleConstant$1(name, value, priority) {
  return function() {
    this.style.setProperty(name, value, priority);
  };
}
function styleFunction$1(name, value, priority) {
  return function() {
    var v = value.apply(this, arguments);
    if (v == null)
      this.style.removeProperty(name);
    else
      this.style.setProperty(name, v, priority);
  };
}
function selection_style(name, value, priority) {
  return arguments.length > 1 ? this.each((value == null ? styleRemove$1 : typeof value === "function" ? styleFunction$1 : styleConstant$1)(name, value, priority == null ? "" : priority)) : styleValue(this.node(), name);
}
function styleValue(node, name) {
  return node.style.getPropertyValue(name) || defaultView(node).getComputedStyle(node, null).getPropertyValue(name);
}
function propertyRemove(name) {
  return function() {
    delete this[name];
  };
}
function propertyConstant(name, value) {
  return function() {
    this[name] = value;
  };
}
function propertyFunction(name, value) {
  return function() {
    var v = value.apply(this, arguments);
    if (v == null)
      delete this[name];
    else
      this[name] = v;
  };
}
function selection_property(name, value) {
  return arguments.length > 1 ? this.each((value == null ? propertyRemove : typeof value === "function" ? propertyFunction : propertyConstant)(name, value)) : this.node()[name];
}
function classArray(string2) {
  return string2.trim().split(/^|\s+/);
}
function classList(node) {
  return node.classList || new ClassList(node);
}
function ClassList(node) {
  this._node = node;
  this._names = classArray(node.getAttribute("class") || "");
}
ClassList.prototype = {
  add: function(name) {
    var i = this._names.indexOf(name);
    if (i < 0) {
      this._names.push(name);
      this._node.setAttribute("class", this._names.join(" "));
    }
  },
  remove: function(name) {
    var i = this._names.indexOf(name);
    if (i >= 0) {
      this._names.splice(i, 1);
      this._node.setAttribute("class", this._names.join(" "));
    }
  },
  contains: function(name) {
    return this._names.indexOf(name) >= 0;
  }
};
function classedAdd(node, names) {
  var list = classList(node), i = -1, n = names.length;
  while (++i < n)
    list.add(names[i]);
}
function classedRemove(node, names) {
  var list = classList(node), i = -1, n = names.length;
  while (++i < n)
    list.remove(names[i]);
}
function classedTrue(names) {
  return function() {
    classedAdd(this, names);
  };
}
function classedFalse(names) {
  return function() {
    classedRemove(this, names);
  };
}
function classedFunction(names, value) {
  return function() {
    (value.apply(this, arguments) ? classedAdd : classedRemove)(this, names);
  };
}
function selection_classed(name, value) {
  var names = classArray(name + "");
  if (arguments.length < 2) {
    var list = classList(this.node()), i = -1, n = names.length;
    while (++i < n)
      if (!list.contains(names[i]))
        return false;
    return true;
  }
  return this.each((typeof value === "function" ? classedFunction : value ? classedTrue : classedFalse)(names, value));
}
function textRemove() {
  this.textContent = "";
}
function textConstant$1(value) {
  return function() {
    this.textContent = value;
  };
}
function textFunction$1(value) {
  return function() {
    var v = value.apply(this, arguments);
    this.textContent = v == null ? "" : v;
  };
}
function selection_text(value) {
  return arguments.length ? this.each(value == null ? textRemove : (typeof value === "function" ? textFunction$1 : textConstant$1)(value)) : this.node().textContent;
}
function htmlRemove() {
  this.innerHTML = "";
}
function htmlConstant(value) {
  return function() {
    this.innerHTML = value;
  };
}
function htmlFunction(value) {
  return function() {
    var v = value.apply(this, arguments);
    this.innerHTML = v == null ? "" : v;
  };
}
function selection_html(value) {
  return arguments.length ? this.each(value == null ? htmlRemove : (typeof value === "function" ? htmlFunction : htmlConstant)(value)) : this.node().innerHTML;
}
function raise() {
  if (this.nextSibling)
    this.parentNode.appendChild(this);
}
function selection_raise() {
  return this.each(raise);
}
function lower() {
  if (this.previousSibling)
    this.parentNode.insertBefore(this, this.parentNode.firstChild);
}
function selection_lower() {
  return this.each(lower);
}
function selection_append(name) {
  var create2 = typeof name === "function" ? name : creator(name);
  return this.select(function() {
    return this.appendChild(create2.apply(this, arguments));
  });
}
function constantNull() {
  return null;
}
function selection_insert(name, before) {
  var create2 = typeof name === "function" ? name : creator(name), select2 = before == null ? constantNull : typeof before === "function" ? before : selector(before);
  return this.select(function() {
    return this.insertBefore(create2.apply(this, arguments), select2.apply(this, arguments) || null);
  });
}
function remove() {
  var parent = this.parentNode;
  if (parent)
    parent.removeChild(this);
}
function selection_remove() {
  return this.each(remove);
}
function selection_cloneShallow() {
  var clone = this.cloneNode(false), parent = this.parentNode;
  return parent ? parent.insertBefore(clone, this.nextSibling) : clone;
}
function selection_cloneDeep() {
  var clone = this.cloneNode(true), parent = this.parentNode;
  return parent ? parent.insertBefore(clone, this.nextSibling) : clone;
}
function selection_clone(deep) {
  return this.select(deep ? selection_cloneDeep : selection_cloneShallow);
}
function selection_datum(value) {
  return arguments.length ? this.property("__data__", value) : this.node().__data__;
}
function contextListener(listener) {
  return function(event) {
    listener.call(this, event, this.__data__);
  };
}
function parseTypenames(typenames) {
  return typenames.trim().split(/^|\s+/).map(function(t) {
    var name = "", i = t.indexOf(".");
    if (i >= 0)
      name = t.slice(i + 1), t = t.slice(0, i);
    return { type: t, name };
  });
}
function onRemove(typename) {
  return function() {
    var on = this.__on;
    if (!on)
      return;
    for (var j = 0, i = -1, m = on.length, o; j < m; ++j) {
      if (o = on[j], (!typename.type || o.type === typename.type) && o.name === typename.name) {
        this.removeEventListener(o.type, o.listener, o.options);
      } else {
        on[++i] = o;
      }
    }
    if (++i)
      on.length = i;
    else
      delete this.__on;
  };
}
function onAdd(typename, value, options) {
  return function() {
    var on = this.__on, o, listener = contextListener(value);
    if (on)
      for (var j = 0, m = on.length; j < m; ++j) {
        if ((o = on[j]).type === typename.type && o.name === typename.name) {
          this.removeEventListener(o.type, o.listener, o.options);
          this.addEventListener(o.type, o.listener = listener, o.options = options);
          o.value = value;
          return;
        }
      }
    this.addEventListener(typename.type, listener, options);
    o = { type: typename.type, name: typename.name, value, listener, options };
    if (!on)
      this.__on = [o];
    else
      on.push(o);
  };
}
function selection_on(typename, value, options) {
  var typenames = parseTypenames(typename + ""), i, n = typenames.length, t;
  if (arguments.length < 2) {
    var on = this.node().__on;
    if (on)
      for (var j = 0, m = on.length, o; j < m; ++j) {
        for (i = 0, o = on[j]; i < n; ++i) {
          if ((t = typenames[i]).type === o.type && t.name === o.name) {
            return o.value;
          }
        }
      }
    return;
  }
  on = value ? onAdd : onRemove;
  for (i = 0; i < n; ++i)
    this.each(on(typenames[i], value, options));
  return this;
}
function dispatchEvent(node, type, params) {
  var window2 = defaultView(node), event = window2.CustomEvent;
  if (typeof event === "function") {
    event = new event(type, params);
  } else {
    event = window2.document.createEvent("Event");
    if (params)
      event.initEvent(type, params.bubbles, params.cancelable), event.detail = params.detail;
    else
      event.initEvent(type, false, false);
  }
  node.dispatchEvent(event);
}
function dispatchConstant(type, params) {
  return function() {
    return dispatchEvent(this, type, params);
  };
}
function dispatchFunction(type, params) {
  return function() {
    return dispatchEvent(this, type, params.apply(this, arguments));
  };
}
function selection_dispatch(type, params) {
  return this.each((typeof params === "function" ? dispatchFunction : dispatchConstant)(type, params));
}
function* selection_iterator() {
  for (var groups2 = this._groups, j = 0, m = groups2.length; j < m; ++j) {
    for (var group2 = groups2[j], i = 0, n = group2.length, node; i < n; ++i) {
      if (node = group2[i])
        yield node;
    }
  }
}
var root = [null];
function Selection$1(groups2, parents) {
  this._groups = groups2;
  this._parents = parents;
}
function selection() {
  return new Selection$1([[document.documentElement]], root);
}
function selection_selection() {
  return this;
}
Selection$1.prototype = selection.prototype = {
  constructor: Selection$1,
  select: selection_select,
  selectAll: selection_selectAll,
  selectChild: selection_selectChild,
  selectChildren: selection_selectChildren,
  filter: selection_filter,
  data: selection_data,
  enter: selection_enter,
  exit: selection_exit,
  join: selection_join,
  merge: selection_merge,
  selection: selection_selection,
  order: selection_order,
  sort: selection_sort,
  call: selection_call,
  nodes: selection_nodes,
  node: selection_node,
  size: selection_size,
  empty: selection_empty,
  each: selection_each,
  attr: selection_attr,
  style: selection_style,
  property: selection_property,
  classed: selection_classed,
  text: selection_text,
  html: selection_html,
  raise: selection_raise,
  lower: selection_lower,
  append: selection_append,
  insert: selection_insert,
  remove: selection_remove,
  clone: selection_clone,
  datum: selection_datum,
  on: selection_on,
  dispatch: selection_dispatch,
  [Symbol.iterator]: selection_iterator
};
function select(selector2) {
  return typeof selector2 === "string" ? new Selection$1([[document.querySelector(selector2)]], [document.documentElement]) : new Selection$1([[selector2]], root);
}
function define(constructor, factory, prototype) {
  constructor.prototype = factory.prototype = prototype;
  prototype.constructor = constructor;
}
function extend(parent, definition) {
  var prototype = Object.create(parent.prototype);
  for (var key in definition)
    prototype[key] = definition[key];
  return prototype;
}
function Color() {
}
var darker = 0.7;
var brighter = 1 / darker;
var reI = "\\s*([+-]?\\d+)\\s*", reN = "\\s*([+-]?(?:\\d*\\.)?\\d+(?:[eE][+-]?\\d+)?)\\s*", reP = "\\s*([+-]?(?:\\d*\\.)?\\d+(?:[eE][+-]?\\d+)?)%\\s*", reHex = /^#([0-9a-f]{3,8})$/, reRgbInteger = new RegExp(`^rgb\\(${reI},${reI},${reI}\\)$`), reRgbPercent = new RegExp(`^rgb\\(${reP},${reP},${reP}\\)$`), reRgbaInteger = new RegExp(`^rgba\\(${reI},${reI},${reI},${reN}\\)$`), reRgbaPercent = new RegExp(`^rgba\\(${reP},${reP},${reP},${reN}\\)$`), reHslPercent = new RegExp(`^hsl\\(${reN},${reP},${reP}\\)$`), reHslaPercent = new RegExp(`^hsla\\(${reN},${reP},${reP},${reN}\\)$`);
var named = {
  aliceblue: 15792383,
  antiquewhite: 16444375,
  aqua: 65535,
  aquamarine: 8388564,
  azure: 15794175,
  beige: 16119260,
  bisque: 16770244,
  black: 0,
  blanchedalmond: 16772045,
  blue: 255,
  blueviolet: 9055202,
  brown: 10824234,
  burlywood: 14596231,
  cadetblue: 6266528,
  chartreuse: 8388352,
  chocolate: 13789470,
  coral: 16744272,
  cornflowerblue: 6591981,
  cornsilk: 16775388,
  crimson: 14423100,
  cyan: 65535,
  darkblue: 139,
  darkcyan: 35723,
  darkgoldenrod: 12092939,
  darkgray: 11119017,
  darkgreen: 25600,
  darkgrey: 11119017,
  darkkhaki: 12433259,
  darkmagenta: 9109643,
  darkolivegreen: 5597999,
  darkorange: 16747520,
  darkorchid: 10040012,
  darkred: 9109504,
  darksalmon: 15308410,
  darkseagreen: 9419919,
  darkslateblue: 4734347,
  darkslategray: 3100495,
  darkslategrey: 3100495,
  darkturquoise: 52945,
  darkviolet: 9699539,
  deeppink: 16716947,
  deepskyblue: 49151,
  dimgray: 6908265,
  dimgrey: 6908265,
  dodgerblue: 2003199,
  firebrick: 11674146,
  floralwhite: 16775920,
  forestgreen: 2263842,
  fuchsia: 16711935,
  gainsboro: 14474460,
  ghostwhite: 16316671,
  gold: 16766720,
  goldenrod: 14329120,
  gray: 8421504,
  green: 32768,
  greenyellow: 11403055,
  grey: 8421504,
  honeydew: 15794160,
  hotpink: 16738740,
  indianred: 13458524,
  indigo: 4915330,
  ivory: 16777200,
  khaki: 15787660,
  lavender: 15132410,
  lavenderblush: 16773365,
  lawngreen: 8190976,
  lemonchiffon: 16775885,
  lightblue: 11393254,
  lightcoral: 15761536,
  lightcyan: 14745599,
  lightgoldenrodyellow: 16448210,
  lightgray: 13882323,
  lightgreen: 9498256,
  lightgrey: 13882323,
  lightpink: 16758465,
  lightsalmon: 16752762,
  lightseagreen: 2142890,
  lightskyblue: 8900346,
  lightslategray: 7833753,
  lightslategrey: 7833753,
  lightsteelblue: 11584734,
  lightyellow: 16777184,
  lime: 65280,
  limegreen: 3329330,
  linen: 16445670,
  magenta: 16711935,
  maroon: 8388608,
  mediumaquamarine: 6737322,
  mediumblue: 205,
  mediumorchid: 12211667,
  mediumpurple: 9662683,
  mediumseagreen: 3978097,
  mediumslateblue: 8087790,
  mediumspringgreen: 64154,
  mediumturquoise: 4772300,
  mediumvioletred: 13047173,
  midnightblue: 1644912,
  mintcream: 16121850,
  mistyrose: 16770273,
  moccasin: 16770229,
  navajowhite: 16768685,
  navy: 128,
  oldlace: 16643558,
  olive: 8421376,
  olivedrab: 7048739,
  orange: 16753920,
  orangered: 16729344,
  orchid: 14315734,
  palegoldenrod: 15657130,
  palegreen: 10025880,
  paleturquoise: 11529966,
  palevioletred: 14381203,
  papayawhip: 16773077,
  peachpuff: 16767673,
  peru: 13468991,
  pink: 16761035,
  plum: 14524637,
  powderblue: 11591910,
  purple: 8388736,
  rebeccapurple: 6697881,
  red: 16711680,
  rosybrown: 12357519,
  royalblue: 4286945,
  saddlebrown: 9127187,
  salmon: 16416882,
  sandybrown: 16032864,
  seagreen: 3050327,
  seashell: 16774638,
  sienna: 10506797,
  silver: 12632256,
  skyblue: 8900331,
  slateblue: 6970061,
  slategray: 7372944,
  slategrey: 7372944,
  snow: 16775930,
  springgreen: 65407,
  steelblue: 4620980,
  tan: 13808780,
  teal: 32896,
  thistle: 14204888,
  tomato: 16737095,
  turquoise: 4251856,
  violet: 15631086,
  wheat: 16113331,
  white: 16777215,
  whitesmoke: 16119285,
  yellow: 16776960,
  yellowgreen: 10145074
};
define(Color, color$1, {
  copy(channels) {
    return Object.assign(new this.constructor(), this, channels);
  },
  displayable() {
    return this.rgb().displayable();
  },
  hex: color_formatHex,
  // Deprecated! Use color.formatHex.
  formatHex: color_formatHex,
  formatHex8: color_formatHex8,
  formatHsl: color_formatHsl,
  formatRgb: color_formatRgb,
  toString: color_formatRgb
});
function color_formatHex() {
  return this.rgb().formatHex();
}
function color_formatHex8() {
  return this.rgb().formatHex8();
}
function color_formatHsl() {
  return hslConvert(this).formatHsl();
}
function color_formatRgb() {
  return this.rgb().formatRgb();
}
function color$1(format2) {
  var m, l;
  format2 = (format2 + "").trim().toLowerCase();
  return (m = reHex.exec(format2)) ? (l = m[1].length, m = parseInt(m[1], 16), l === 6 ? rgbn(m) : l === 3 ? new Rgb(m >> 8 & 15 | m >> 4 & 240, m >> 4 & 15 | m & 240, (m & 15) << 4 | m & 15, 1) : l === 8 ? rgba(m >> 24 & 255, m >> 16 & 255, m >> 8 & 255, (m & 255) / 255) : l === 4 ? rgba(m >> 12 & 15 | m >> 8 & 240, m >> 8 & 15 | m >> 4 & 240, m >> 4 & 15 | m & 240, ((m & 15) << 4 | m & 15) / 255) : null) : (m = reRgbInteger.exec(format2)) ? new Rgb(m[1], m[2], m[3], 1) : (m = reRgbPercent.exec(format2)) ? new Rgb(m[1] * 255 / 100, m[2] * 255 / 100, m[3] * 255 / 100, 1) : (m = reRgbaInteger.exec(format2)) ? rgba(m[1], m[2], m[3], m[4]) : (m = reRgbaPercent.exec(format2)) ? rgba(m[1] * 255 / 100, m[2] * 255 / 100, m[3] * 255 / 100, m[4]) : (m = reHslPercent.exec(format2)) ? hsla(m[1], m[2] / 100, m[3] / 100, 1) : (m = reHslaPercent.exec(format2)) ? hsla(m[1], m[2] / 100, m[3] / 100, m[4]) : named.hasOwnProperty(format2) ? rgbn(named[format2]) : format2 === "transparent" ? new Rgb(NaN, NaN, NaN, 0) : null;
}
function rgbn(n) {
  return new Rgb(n >> 16 & 255, n >> 8 & 255, n & 255, 1);
}
function rgba(r, g, b, a2) {
  if (a2 <= 0)
    r = g = b = NaN;
  return new Rgb(r, g, b, a2);
}
function rgbConvert(o) {
  if (!(o instanceof Color))
    o = color$1(o);
  if (!o)
    return new Rgb();
  o = o.rgb();
  return new Rgb(o.r, o.g, o.b, o.opacity);
}
function rgb(r, g, b, opacity2) {
  return arguments.length === 1 ? rgbConvert(r) : new Rgb(r, g, b, opacity2 == null ? 1 : opacity2);
}
function Rgb(r, g, b, opacity2) {
  this.r = +r;
  this.g = +g;
  this.b = +b;
  this.opacity = +opacity2;
}
define(Rgb, rgb, extend(Color, {
  brighter(k2) {
    k2 = k2 == null ? brighter : Math.pow(brighter, k2);
    return new Rgb(this.r * k2, this.g * k2, this.b * k2, this.opacity);
  },
  darker(k2) {
    k2 = k2 == null ? darker : Math.pow(darker, k2);
    return new Rgb(this.r * k2, this.g * k2, this.b * k2, this.opacity);
  },
  rgb() {
    return this;
  },
  clamp() {
    return new Rgb(clampi(this.r), clampi(this.g), clampi(this.b), clampa(this.opacity));
  },
  displayable() {
    return -0.5 <= this.r && this.r < 255.5 && (-0.5 <= this.g && this.g < 255.5) && (-0.5 <= this.b && this.b < 255.5) && (0 <= this.opacity && this.opacity <= 1);
  },
  hex: rgb_formatHex,
  // Deprecated! Use color.formatHex.
  formatHex: rgb_formatHex,
  formatHex8: rgb_formatHex8,
  formatRgb: rgb_formatRgb,
  toString: rgb_formatRgb
}));
function rgb_formatHex() {
  return `#${hex(this.r)}${hex(this.g)}${hex(this.b)}`;
}
function rgb_formatHex8() {
  return `#${hex(this.r)}${hex(this.g)}${hex(this.b)}${hex((isNaN(this.opacity) ? 1 : this.opacity) * 255)}`;
}
function rgb_formatRgb() {
  const a2 = clampa(this.opacity);
  return `${a2 === 1 ? "rgb(" : "rgba("}${clampi(this.r)}, ${clampi(this.g)}, ${clampi(this.b)}${a2 === 1 ? ")" : `, ${a2})`}`;
}
function clampa(opacity2) {
  return isNaN(opacity2) ? 1 : Math.max(0, Math.min(1, opacity2));
}
function clampi(value) {
  return Math.max(0, Math.min(255, Math.round(value) || 0));
}
function hex(value) {
  value = clampi(value);
  return (value < 16 ? "0" : "") + value.toString(16);
}
function hsla(h, s2, l, a2) {
  if (a2 <= 0)
    h = s2 = l = NaN;
  else if (l <= 0 || l >= 1)
    h = s2 = NaN;
  else if (s2 <= 0)
    h = NaN;
  return new Hsl(h, s2, l, a2);
}
function hslConvert(o) {
  if (o instanceof Hsl)
    return new Hsl(o.h, o.s, o.l, o.opacity);
  if (!(o instanceof Color))
    o = color$1(o);
  if (!o)
    return new Hsl();
  if (o instanceof Hsl)
    return o;
  o = o.rgb();
  var r = o.r / 255, g = o.g / 255, b = o.b / 255, min2 = Math.min(r, g, b), max2 = Math.max(r, g, b), h = NaN, s2 = max2 - min2, l = (max2 + min2) / 2;
  if (s2) {
    if (r === max2)
      h = (g - b) / s2 + (g < b) * 6;
    else if (g === max2)
      h = (b - r) / s2 + 2;
    else
      h = (r - g) / s2 + 4;
    s2 /= l < 0.5 ? max2 + min2 : 2 - max2 - min2;
    h *= 60;
  } else {
    s2 = l > 0 && l < 1 ? 0 : h;
  }
  return new Hsl(h, s2, l, o.opacity);
}
function hsl$1(h, s2, l, opacity2) {
  return arguments.length === 1 ? hslConvert(h) : new Hsl(h, s2, l, opacity2 == null ? 1 : opacity2);
}
function Hsl(h, s2, l, opacity2) {
  this.h = +h;
  this.s = +s2;
  this.l = +l;
  this.opacity = +opacity2;
}
define(Hsl, hsl$1, extend(Color, {
  brighter(k2) {
    k2 = k2 == null ? brighter : Math.pow(brighter, k2);
    return new Hsl(this.h, this.s, this.l * k2, this.opacity);
  },
  darker(k2) {
    k2 = k2 == null ? darker : Math.pow(darker, k2);
    return new Hsl(this.h, this.s, this.l * k2, this.opacity);
  },
  rgb() {
    var h = this.h % 360 + (this.h < 0) * 360, s2 = isNaN(h) || isNaN(this.s) ? 0 : this.s, l = this.l, m2 = l + (l < 0.5 ? l : 1 - l) * s2, m1 = 2 * l - m2;
    return new Rgb(
      hsl2rgb(h >= 240 ? h - 240 : h + 120, m1, m2),
      hsl2rgb(h, m1, m2),
      hsl2rgb(h < 120 ? h + 240 : h - 120, m1, m2),
      this.opacity
    );
  },
  clamp() {
    return new Hsl(clamph(this.h), clampt(this.s), clampt(this.l), clampa(this.opacity));
  },
  displayable() {
    return (0 <= this.s && this.s <= 1 || isNaN(this.s)) && (0 <= this.l && this.l <= 1) && (0 <= this.opacity && this.opacity <= 1);
  },
  formatHsl() {
    const a2 = clampa(this.opacity);
    return `${a2 === 1 ? "hsl(" : "hsla("}${clamph(this.h)}, ${clampt(this.s) * 100}%, ${clampt(this.l) * 100}%${a2 === 1 ? ")" : `, ${a2})`}`;
  }
}));
function clamph(value) {
  value = (value || 0) % 360;
  return value < 0 ? value + 360 : value;
}
function clampt(value) {
  return Math.max(0, Math.min(1, value || 0));
}
function hsl2rgb(h, m1, m2) {
  return (h < 60 ? m1 + (m2 - m1) * h / 60 : h < 180 ? m2 : h < 240 ? m1 + (m2 - m1) * (240 - h) / 60 : m1) * 255;
}
const radians$2 = Math.PI / 180;
const degrees$2 = 180 / Math.PI;
const K = 18, Xn = 0.96422, Yn = 1, Zn = 0.82521, t0$1 = 4 / 29, t1$1 = 6 / 29, t2 = 3 * t1$1 * t1$1, t3 = t1$1 * t1$1 * t1$1;
function labConvert(o) {
  if (o instanceof Lab)
    return new Lab(o.l, o.a, o.b, o.opacity);
  if (o instanceof Hcl)
    return hcl2lab(o);
  if (!(o instanceof Rgb))
    o = rgbConvert(o);
  var r = rgb2lrgb(o.r), g = rgb2lrgb(o.g), b = rgb2lrgb(o.b), y = xyz2lab((0.2225045 * r + 0.7168786 * g + 0.0606169 * b) / Yn), x, z;
  if (r === g && g === b)
    x = z = y;
  else {
    x = xyz2lab((0.4360747 * r + 0.3850649 * g + 0.1430804 * b) / Xn);
    z = xyz2lab((0.0139322 * r + 0.0971045 * g + 0.7141733 * b) / Zn);
  }
  return new Lab(116 * y - 16, 500 * (x - y), 200 * (y - z), o.opacity);
}
function lab$1(l, a2, b, opacity2) {
  return arguments.length === 1 ? labConvert(l) : new Lab(l, a2, b, opacity2 == null ? 1 : opacity2);
}
function Lab(l, a2, b, opacity2) {
  this.l = +l;
  this.a = +a2;
  this.b = +b;
  this.opacity = +opacity2;
}
define(Lab, lab$1, extend(Color, {
  brighter(k2) {
    return new Lab(this.l + K * (k2 == null ? 1 : k2), this.a, this.b, this.opacity);
  },
  darker(k2) {
    return new Lab(this.l - K * (k2 == null ? 1 : k2), this.a, this.b, this.opacity);
  },
  rgb() {
    var y = (this.l + 16) / 116, x = isNaN(this.a) ? y : y + this.a / 500, z = isNaN(this.b) ? y : y - this.b / 200;
    x = Xn * lab2xyz(x);
    y = Yn * lab2xyz(y);
    z = Zn * lab2xyz(z);
    return new Rgb(
      lrgb2rgb(3.1338561 * x - 1.6168667 * y - 0.4906146 * z),
      lrgb2rgb(-0.9787684 * x + 1.9161415 * y + 0.033454 * z),
      lrgb2rgb(0.0719453 * x - 0.2289914 * y + 1.4052427 * z),
      this.opacity
    );
  }
}));
function xyz2lab(t) {
  return t > t3 ? Math.pow(t, 1 / 3) : t / t2 + t0$1;
}
function lab2xyz(t) {
  return t > t1$1 ? t * t * t : t2 * (t - t0$1);
}
function lrgb2rgb(x) {
  return 255 * (x <= 31308e-7 ? 12.92 * x : 1.055 * Math.pow(x, 1 / 2.4) - 0.055);
}
function rgb2lrgb(x) {
  return (x /= 255) <= 0.04045 ? x / 12.92 : Math.pow((x + 0.055) / 1.055, 2.4);
}
function hclConvert(o) {
  if (o instanceof Hcl)
    return new Hcl(o.h, o.c, o.l, o.opacity);
  if (!(o instanceof Lab))
    o = labConvert(o);
  if (o.a === 0 && o.b === 0)
    return new Hcl(NaN, 0 < o.l && o.l < 100 ? 0 : NaN, o.l, o.opacity);
  var h = Math.atan2(o.b, o.a) * degrees$2;
  return new Hcl(h < 0 ? h + 360 : h, Math.sqrt(o.a * o.a + o.b * o.b), o.l, o.opacity);
}
function hcl$1(h, c2, l, opacity2) {
  return arguments.length === 1 ? hclConvert(h) : new Hcl(h, c2, l, opacity2 == null ? 1 : opacity2);
}
function Hcl(h, c2, l, opacity2) {
  this.h = +h;
  this.c = +c2;
  this.l = +l;
  this.opacity = +opacity2;
}
function hcl2lab(o) {
  if (isNaN(o.h))
    return new Lab(o.l, 0, 0, o.opacity);
  var h = o.h * radians$2;
  return new Lab(o.l, Math.cos(h) * o.c, Math.sin(h) * o.c, o.opacity);
}
define(Hcl, hcl$1, extend(Color, {
  brighter(k2) {
    return new Hcl(this.h, this.c, this.l + K * (k2 == null ? 1 : k2), this.opacity);
  },
  darker(k2) {
    return new Hcl(this.h, this.c, this.l - K * (k2 == null ? 1 : k2), this.opacity);
  },
  rgb() {
    return hcl2lab(this).rgb();
  }
}));
var A = -0.14861, B = 1.78277, C = -0.29227, D = -0.90649, E = 1.97294, ED = E * D, EB = E * B, BC_DA = B * C - D * A;
function cubehelixConvert(o) {
  if (o instanceof Cubehelix)
    return new Cubehelix(o.h, o.s, o.l, o.opacity);
  if (!(o instanceof Rgb))
    o = rgbConvert(o);
  var r = o.r / 255, g = o.g / 255, b = o.b / 255, l = (BC_DA * b + ED * r - EB * g) / (BC_DA + ED - EB), bl = b - l, k2 = (E * (g - l) - C * bl) / D, s2 = Math.sqrt(k2 * k2 + bl * bl) / (E * l * (1 - l)), h = s2 ? Math.atan2(k2, bl) * degrees$2 - 120 : NaN;
  return new Cubehelix(h < 0 ? h + 360 : h, s2, l, o.opacity);
}
function cubehelix$1(h, s2, l, opacity2) {
  return arguments.length === 1 ? cubehelixConvert(h) : new Cubehelix(h, s2, l, opacity2 == null ? 1 : opacity2);
}
function Cubehelix(h, s2, l, opacity2) {
  this.h = +h;
  this.s = +s2;
  this.l = +l;
  this.opacity = +opacity2;
}
define(Cubehelix, cubehelix$1, extend(Color, {
  brighter(k2) {
    k2 = k2 == null ? brighter : Math.pow(brighter, k2);
    return new Cubehelix(this.h, this.s, this.l * k2, this.opacity);
  },
  darker(k2) {
    k2 = k2 == null ? darker : Math.pow(darker, k2);
    return new Cubehelix(this.h, this.s, this.l * k2, this.opacity);
  },
  rgb() {
    var h = isNaN(this.h) ? 0 : (this.h + 120) * radians$2, l = +this.l, a2 = isNaN(this.s) ? 0 : this.s * l * (1 - l), cosh = Math.cos(h), sinh = Math.sin(h);
    return new Rgb(
      255 * (l + a2 * (A * cosh + B * sinh)),
      255 * (l + a2 * (C * cosh + D * sinh)),
      255 * (l + a2 * (E * cosh)),
      this.opacity
    );
  }
}));
function basis(t12, v0, v1, v2, v3) {
  var t22 = t12 * t12, t32 = t22 * t12;
  return ((1 - 3 * t12 + 3 * t22 - t32) * v0 + (4 - 6 * t22 + 3 * t32) * v1 + (1 + 3 * t12 + 3 * t22 - 3 * t32) * v2 + t32 * v3) / 6;
}
function basis$1(values2) {
  var n = values2.length - 1;
  return function(t) {
    var i = t <= 0 ? t = 0 : t >= 1 ? (t = 1, n - 1) : Math.floor(t * n), v1 = values2[i], v2 = values2[i + 1], v0 = i > 0 ? values2[i - 1] : 2 * v1 - v2, v3 = i < n - 1 ? values2[i + 2] : 2 * v2 - v1;
    return basis((t - i / n) * n, v0, v1, v2, v3);
  };
}
const constant$1 = (x) => () => x;
function linear$1(a2, d) {
  return function(t) {
    return a2 + t * d;
  };
}
function exponential(a2, b, y) {
  return a2 = Math.pow(a2, y), b = Math.pow(b, y) - a2, y = 1 / y, function(t) {
    return Math.pow(a2 + t * b, y);
  };
}
function hue(a2, b) {
  var d = b - a2;
  return d ? linear$1(a2, d > 180 || d < -180 ? d - 360 * Math.round(d / 360) : d) : constant$1(isNaN(a2) ? b : a2);
}
function gamma(y) {
  return (y = +y) === 1 ? nogamma : function(a2, b) {
    return b - a2 ? exponential(a2, b, y) : constant$1(isNaN(a2) ? b : a2);
  };
}
function nogamma(a2, b) {
  var d = b - a2;
  return d ? linear$1(a2, d) : constant$1(isNaN(a2) ? b : a2);
}
const interpolateRgb = function rgbGamma(y) {
  var color2 = gamma(y);
  function rgb$1(start2, end) {
    var r = color2((start2 = rgb(start2)).r, (end = rgb(end)).r), g = color2(start2.g, end.g), b = color2(start2.b, end.b), opacity2 = nogamma(start2.opacity, end.opacity);
    return function(t) {
      start2.r = r(t);
      start2.g = g(t);
      start2.b = b(t);
      start2.opacity = opacity2(t);
      return start2 + "";
    };
  }
  rgb$1.gamma = rgbGamma;
  return rgb$1;
}(1);
function rgbSpline(spline) {
  return function(colors2) {
    var n = colors2.length, r = new Array(n), g = new Array(n), b = new Array(n), i, color2;
    for (i = 0; i < n; ++i) {
      color2 = rgb(colors2[i]);
      r[i] = color2.r || 0;
      g[i] = color2.g || 0;
      b[i] = color2.b || 0;
    }
    r = spline(r);
    g = spline(g);
    b = spline(b);
    color2.opacity = 1;
    return function(t) {
      color2.r = r(t);
      color2.g = g(t);
      color2.b = b(t);
      return color2 + "";
    };
  };
}
var rgbBasis = rgbSpline(basis$1);
function numberArray(a2, b) {
  if (!b)
    b = [];
  var n = a2 ? Math.min(b.length, a2.length) : 0, c2 = b.slice(), i;
  return function(t) {
    for (i = 0; i < n; ++i)
      c2[i] = a2[i] * (1 - t) + b[i] * t;
    return c2;
  };
}
function isNumberArray(x) {
  return ArrayBuffer.isView(x) && !(x instanceof DataView);
}
function genericArray(a2, b) {
  var nb = b ? b.length : 0, na = a2 ? Math.min(nb, a2.length) : 0, x = new Array(na), c2 = new Array(nb), i;
  for (i = 0; i < na; ++i)
    x[i] = interpolate$1(a2[i], b[i]);
  for (; i < nb; ++i)
    c2[i] = b[i];
  return function(t) {
    for (i = 0; i < na; ++i)
      c2[i] = x[i](t);
    return c2;
  };
}
function date$1(a2, b) {
  var d = new Date();
  return a2 = +a2, b = +b, function(t) {
    return d.setTime(a2 * (1 - t) + b * t), d;
  };
}
function interpolateNumber(a2, b) {
  return a2 = +a2, b = +b, function(t) {
    return a2 * (1 - t) + b * t;
  };
}
function object(a2, b) {
  var i = {}, c2 = {}, k2;
  if (a2 === null || typeof a2 !== "object")
    a2 = {};
  if (b === null || typeof b !== "object")
    b = {};
  for (k2 in b) {
    if (k2 in a2) {
      i[k2] = interpolate$1(a2[k2], b[k2]);
    } else {
      c2[k2] = b[k2];
    }
  }
  return function(t) {
    for (k2 in i)
      c2[k2] = i[k2](t);
    return c2;
  };
}
var reA = /[-+]?(?:\d+\.?\d*|\.?\d+)(?:[eE][-+]?\d+)?/g, reB = new RegExp(reA.source, "g");
function zero(b) {
  return function() {
    return b;
  };
}
function one$1(b) {
  return function(t) {
    return b(t) + "";
  };
}
function interpolateString(a2, b) {
  var bi = reA.lastIndex = reB.lastIndex = 0, am, bm, bs, i = -1, s2 = [], q = [];
  a2 = a2 + "", b = b + "";
  while ((am = reA.exec(a2)) && (bm = reB.exec(b))) {
    if ((bs = bm.index) > bi) {
      bs = b.slice(bi, bs);
      if (s2[i])
        s2[i] += bs;
      else
        s2[++i] = bs;
    }
    if ((am = am[0]) === (bm = bm[0])) {
      if (s2[i])
        s2[i] += bm;
      else
        s2[++i] = bm;
    } else {
      s2[++i] = null;
      q.push({ i, x: interpolateNumber(am, bm) });
    }
    bi = reB.lastIndex;
  }
  if (bi < b.length) {
    bs = b.slice(bi);
    if (s2[i])
      s2[i] += bs;
    else
      s2[++i] = bs;
  }
  return s2.length < 2 ? q[0] ? one$1(q[0].x) : zero(b) : (b = q.length, function(t) {
    for (var i2 = 0, o; i2 < b; ++i2)
      s2[(o = q[i2]).i] = o.x(t);
    return s2.join("");
  });
}
function interpolate$1(a2, b) {
  var t = typeof b, c2;
  return b == null || t === "boolean" ? constant$1(b) : (t === "number" ? interpolateNumber : t === "string" ? (c2 = color$1(b)) ? (b = c2, interpolateRgb) : interpolateString : b instanceof color$1 ? interpolateRgb : b instanceof Date ? date$1 : isNumberArray(b) ? numberArray : Array.isArray(b) ? genericArray : typeof b.valueOf !== "function" && typeof b.toString !== "function" || isNaN(b) ? object : interpolateNumber)(a2, b);
}
function interpolateRound(a2, b) {
  return a2 = +a2, b = +b, function(t) {
    return Math.round(a2 * (1 - t) + b * t);
  };
}
var degrees$1 = 180 / Math.PI;
var identity$6 = {
  translateX: 0,
  translateY: 0,
  rotate: 0,
  skewX: 0,
  scaleX: 1,
  scaleY: 1
};
function decompose(a2, b, c2, d, e, f) {
  var scaleX, scaleY, skewX;
  if (scaleX = Math.sqrt(a2 * a2 + b * b))
    a2 /= scaleX, b /= scaleX;
  if (skewX = a2 * c2 + b * d)
    c2 -= a2 * skewX, d -= b * skewX;
  if (scaleY = Math.sqrt(c2 * c2 + d * d))
    c2 /= scaleY, d /= scaleY, skewX /= scaleY;
  if (a2 * d < b * c2)
    a2 = -a2, b = -b, skewX = -skewX, scaleX = -scaleX;
  return {
    translateX: e,
    translateY: f,
    rotate: Math.atan2(b, a2) * degrees$1,
    skewX: Math.atan(skewX) * degrees$1,
    scaleX,
    scaleY
  };
}
var svgNode;
function parseCss(value) {
  const m = new (typeof DOMMatrix === "function" ? DOMMatrix : WebKitCSSMatrix)(value + "");
  return m.isIdentity ? identity$6 : decompose(m.a, m.b, m.c, m.d, m.e, m.f);
}
function parseSvg(value) {
  if (value == null)
    return identity$6;
  if (!svgNode)
    svgNode = document.createElementNS("http://www.w3.org/2000/svg", "g");
  svgNode.setAttribute("transform", value);
  if (!(value = svgNode.transform.baseVal.consolidate()))
    return identity$6;
  value = value.matrix;
  return decompose(value.a, value.b, value.c, value.d, value.e, value.f);
}
function interpolateTransform(parse2, pxComma, pxParen, degParen) {
  function pop(s2) {
    return s2.length ? s2.pop() + " " : "";
  }
  function translate(xa, ya, xb, yb, s2, q) {
    if (xa !== xb || ya !== yb) {
      var i = s2.push("translate(", null, pxComma, null, pxParen);
      q.push({ i: i - 4, x: interpolateNumber(xa, xb) }, { i: i - 2, x: interpolateNumber(ya, yb) });
    } else if (xb || yb) {
      s2.push("translate(" + xb + pxComma + yb + pxParen);
    }
  }
  function rotate(a2, b, s2, q) {
    if (a2 !== b) {
      if (a2 - b > 180)
        b += 360;
      else if (b - a2 > 180)
        a2 += 360;
      q.push({ i: s2.push(pop(s2) + "rotate(", null, degParen) - 2, x: interpolateNumber(a2, b) });
    } else if (b) {
      s2.push(pop(s2) + "rotate(" + b + degParen);
    }
  }
  function skewX(a2, b, s2, q) {
    if (a2 !== b) {
      q.push({ i: s2.push(pop(s2) + "skewX(", null, degParen) - 2, x: interpolateNumber(a2, b) });
    } else if (b) {
      s2.push(pop(s2) + "skewX(" + b + degParen);
    }
  }
  function scale(xa, ya, xb, yb, s2, q) {
    if (xa !== xb || ya !== yb) {
      var i = s2.push(pop(s2) + "scale(", null, ",", null, ")");
      q.push({ i: i - 4, x: interpolateNumber(xa, xb) }, { i: i - 2, x: interpolateNumber(ya, yb) });
    } else if (xb !== 1 || yb !== 1) {
      s2.push(pop(s2) + "scale(" + xb + "," + yb + ")");
    }
  }
  return function(a2, b) {
    var s2 = [], q = [];
    a2 = parse2(a2), b = parse2(b);
    translate(a2.translateX, a2.translateY, b.translateX, b.translateY, s2, q);
    rotate(a2.rotate, b.rotate, s2, q);
    skewX(a2.skewX, b.skewX, s2, q);
    scale(a2.scaleX, a2.scaleY, b.scaleX, b.scaleY, s2, q);
    a2 = b = null;
    return function(t) {
      var i = -1, n = q.length, o;
      while (++i < n)
        s2[(o = q[i]).i] = o.x(t);
      return s2.join("");
    };
  };
}
var interpolateTransformCss = interpolateTransform(parseCss, "px, ", "px)", "deg)");
var interpolateTransformSvg = interpolateTransform(parseSvg, ", ", ")", ")");
function hsl(hue2) {
  return function(start2, end) {
    var h = hue2((start2 = hsl$1(start2)).h, (end = hsl$1(end)).h), s2 = nogamma(start2.s, end.s), l = nogamma(start2.l, end.l), opacity2 = nogamma(start2.opacity, end.opacity);
    return function(t) {
      start2.h = h(t);
      start2.s = s2(t);
      start2.l = l(t);
      start2.opacity = opacity2(t);
      return start2 + "";
    };
  };
}
const interpolateHsl = hsl(hue);
function lab(start2, end) {
  var l = nogamma((start2 = lab$1(start2)).l, (end = lab$1(end)).l), a2 = nogamma(start2.a, end.a), b = nogamma(start2.b, end.b), opacity2 = nogamma(start2.opacity, end.opacity);
  return function(t) {
    start2.l = l(t);
    start2.a = a2(t);
    start2.b = b(t);
    start2.opacity = opacity2(t);
    return start2 + "";
  };
}
function hcl(hue2) {
  return function(start2, end) {
    var h = hue2((start2 = hcl$1(start2)).h, (end = hcl$1(end)).h), c2 = nogamma(start2.c, end.c), l = nogamma(start2.l, end.l), opacity2 = nogamma(start2.opacity, end.opacity);
    return function(t) {
      start2.h = h(t);
      start2.c = c2(t);
      start2.l = l(t);
      start2.opacity = opacity2(t);
      return start2 + "";
    };
  };
}
const interpolateHcl = hcl(hue);
function cubehelix(hue2) {
  return function cubehelixGamma(y) {
    y = +y;
    function cubehelix2(start2, end) {
      var h = hue2((start2 = cubehelix$1(start2)).h, (end = cubehelix$1(end)).h), s2 = nogamma(start2.s, end.s), l = nogamma(start2.l, end.l), opacity2 = nogamma(start2.opacity, end.opacity);
      return function(t) {
        start2.h = h(t);
        start2.s = s2(t);
        start2.l = l(Math.pow(t, y));
        start2.opacity = opacity2(t);
        return start2 + "";
      };
    }
    cubehelix2.gamma = cubehelixGamma;
    return cubehelix2;
  }(1);
}
cubehelix(hue);
var cubehelixLong = cubehelix(nogamma);
function piecewise(interpolate2, values2) {
  if (values2 === void 0)
    values2 = interpolate2, interpolate2 = interpolate$1;
  var i = 0, n = values2.length - 1, v = values2[0], I = new Array(n < 0 ? 0 : n);
  while (i < n)
    I[i] = interpolate2(v, v = values2[++i]);
  return function(t) {
    var i2 = Math.max(0, Math.min(n - 1, Math.floor(t *= n)));
    return I[i2](t - i2);
  };
}
function quantize(interpolator, n) {
  var samples = new Array(n);
  for (var i = 0; i < n; ++i)
    samples[i] = interpolator(i / (n - 1));
  return samples;
}
var frame$1 = 0, timeout$1 = 0, interval = 0, pokeDelay = 1e3, taskHead, taskTail, clockLast = 0, clockNow = 0, clockSkew = 0, clock = typeof performance === "object" && performance.now ? performance : Date, setFrame = typeof window === "object" && window.requestAnimationFrame ? window.requestAnimationFrame.bind(window) : function(f) {
  setTimeout(f, 17);
};
function now() {
  return clockNow || (setFrame(clearNow), clockNow = clock.now() + clockSkew);
}
function clearNow() {
  clockNow = 0;
}
function Timer() {
  this._call = this._time = this._next = null;
}
Timer.prototype = timer.prototype = {
  constructor: Timer,
  restart: function(callback, delay, time2) {
    if (typeof callback !== "function")
      throw new TypeError("callback is not a function");
    time2 = (time2 == null ? now() : +time2) + (delay == null ? 0 : +delay);
    if (!this._next && taskTail !== this) {
      if (taskTail)
        taskTail._next = this;
      else
        taskHead = this;
      taskTail = this;
    }
    this._call = callback;
    this._time = time2;
    sleep();
  },
  stop: function() {
    if (this._call) {
      this._call = null;
      this._time = Infinity;
      sleep();
    }
  }
};
function timer(callback, delay, time2) {
  var t = new Timer();
  t.restart(callback, delay, time2);
  return t;
}
function timerFlush() {
  now();
  ++frame$1;
  var t = taskHead, e;
  while (t) {
    if ((e = clockNow - t._time) >= 0)
      t._call.call(void 0, e);
    t = t._next;
  }
  --frame$1;
}
function wake() {
  clockNow = (clockLast = clock.now()) + clockSkew;
  frame$1 = timeout$1 = 0;
  try {
    timerFlush();
  } finally {
    frame$1 = 0;
    nap();
    clockNow = 0;
  }
}
function poke() {
  var now2 = clock.now(), delay = now2 - clockLast;
  if (delay > pokeDelay)
    clockSkew -= delay, clockLast = now2;
}
function nap() {
  var t02, t12 = taskHead, t22, time2 = Infinity;
  while (t12) {
    if (t12._call) {
      if (time2 > t12._time)
        time2 = t12._time;
      t02 = t12, t12 = t12._next;
    } else {
      t22 = t12._next, t12._next = null;
      t12 = t02 ? t02._next = t22 : taskHead = t22;
    }
  }
  taskTail = t02;
  sleep(time2);
}
function sleep(time2) {
  if (frame$1)
    return;
  if (timeout$1)
    timeout$1 = clearTimeout(timeout$1);
  var delay = time2 - clockNow;
  if (delay > 24) {
    if (time2 < Infinity)
      timeout$1 = setTimeout(wake, time2 - clock.now() - clockSkew);
    if (interval)
      interval = clearInterval(interval);
  } else {
    if (!interval)
      clockLast = clock.now(), interval = setInterval(poke, pokeDelay);
    frame$1 = 1, setFrame(wake);
  }
}
function timeout(callback, delay, time2) {
  var t = new Timer();
  delay = delay == null ? 0 : +delay;
  t.restart((elapsed) => {
    t.stop();
    callback(elapsed + delay);
  }, delay, time2);
  return t;
}
var emptyOn = dispatch("start", "end", "cancel", "interrupt");
var emptyTween = [];
var CREATED = 0;
var SCHEDULED = 1;
var STARTING = 2;
var STARTED = 3;
var RUNNING = 4;
var ENDING = 5;
var ENDED = 6;
function schedule(node, name, id2, index2, group2, timing) {
  var schedules = node.__transition;
  if (!schedules)
    node.__transition = {};
  else if (id2 in schedules)
    return;
  create$1(node, id2, {
    name,
    index: index2,
    // For context during callback.
    group: group2,
    // For context during callback.
    on: emptyOn,
    tween: emptyTween,
    time: timing.time,
    delay: timing.delay,
    duration: timing.duration,
    ease: timing.ease,
    timer: null,
    state: CREATED
  });
}
function init(node, id2) {
  var schedule2 = get(node, id2);
  if (schedule2.state > CREATED)
    throw new Error("too late; already scheduled");
  return schedule2;
}
function set(node, id2) {
  var schedule2 = get(node, id2);
  if (schedule2.state > STARTED)
    throw new Error("too late; already running");
  return schedule2;
}
function get(node, id2) {
  var schedule2 = node.__transition;
  if (!schedule2 || !(schedule2 = schedule2[id2]))
    throw new Error("transition not found");
  return schedule2;
}
function create$1(node, id2, self) {
  var schedules = node.__transition, tween;
  schedules[id2] = self;
  self.timer = timer(schedule2, 0, self.time);
  function schedule2(elapsed) {
    self.state = SCHEDULED;
    self.timer.restart(start2, self.delay, self.time);
    if (self.delay <= elapsed)
      start2(elapsed - self.delay);
  }
  function start2(elapsed) {
    var i, j, n, o;
    if (self.state !== SCHEDULED)
      return stop();
    for (i in schedules) {
      o = schedules[i];
      if (o.name !== self.name)
        continue;
      if (o.state === STARTED)
        return timeout(start2);
      if (o.state === RUNNING) {
        o.state = ENDED;
        o.timer.stop();
        o.on.call("interrupt", node, node.__data__, o.index, o.group);
        delete schedules[i];
      } else if (+i < id2) {
        o.state = ENDED;
        o.timer.stop();
        o.on.call("cancel", node, node.__data__, o.index, o.group);
        delete schedules[i];
      }
    }
    timeout(function() {
      if (self.state === STARTED) {
        self.state = RUNNING;
        self.timer.restart(tick, self.delay, self.time);
        tick(elapsed);
      }
    });
    self.state = STARTING;
    self.on.call("start", node, node.__data__, self.index, self.group);
    if (self.state !== STARTING)
      return;
    self.state = STARTED;
    tween = new Array(n = self.tween.length);
    for (i = 0, j = -1; i < n; ++i) {
      if (o = self.tween[i].value.call(node, node.__data__, self.index, self.group)) {
        tween[++j] = o;
      }
    }
    tween.length = j + 1;
  }
  function tick(elapsed) {
    var t = elapsed < self.duration ? self.ease.call(null, elapsed / self.duration) : (self.timer.restart(stop), self.state = ENDING, 1), i = -1, n = tween.length;
    while (++i < n) {
      tween[i].call(node, t);
    }
    if (self.state === ENDING) {
      self.on.call("end", node, node.__data__, self.index, self.group);
      stop();
    }
  }
  function stop() {
    self.state = ENDED;
    self.timer.stop();
    delete schedules[id2];
    for (var i in schedules)
      return;
    delete node.__transition;
  }
}
function interrupt(node, name) {
  var schedules = node.__transition, schedule2, active, empty2 = true, i;
  if (!schedules)
    return;
  name = name == null ? null : name + "";
  for (i in schedules) {
    if ((schedule2 = schedules[i]).name !== name) {
      empty2 = false;
      continue;
    }
    active = schedule2.state > STARTING && schedule2.state < ENDING;
    schedule2.state = ENDED;
    schedule2.timer.stop();
    schedule2.on.call(active ? "interrupt" : "cancel", node, node.__data__, schedule2.index, schedule2.group);
    delete schedules[i];
  }
  if (empty2)
    delete node.__transition;
}
function selection_interrupt(name) {
  return this.each(function() {
    interrupt(this, name);
  });
}
function tweenRemove(id2, name) {
  var tween0, tween1;
  return function() {
    var schedule2 = set(this, id2), tween = schedule2.tween;
    if (tween !== tween0) {
      tween1 = tween0 = tween;
      for (var i = 0, n = tween1.length; i < n; ++i) {
        if (tween1[i].name === name) {
          tween1 = tween1.slice();
          tween1.splice(i, 1);
          break;
        }
      }
    }
    schedule2.tween = tween1;
  };
}
function tweenFunction(id2, name, value) {
  var tween0, tween1;
  if (typeof value !== "function")
    throw new Error();
  return function() {
    var schedule2 = set(this, id2), tween = schedule2.tween;
    if (tween !== tween0) {
      tween1 = (tween0 = tween).slice();
      for (var t = { name, value }, i = 0, n = tween1.length; i < n; ++i) {
        if (tween1[i].name === name) {
          tween1[i] = t;
          break;
        }
      }
      if (i === n)
        tween1.push(t);
    }
    schedule2.tween = tween1;
  };
}
function transition_tween(name, value) {
  var id2 = this._id;
  name += "";
  if (arguments.length < 2) {
    var tween = get(this.node(), id2).tween;
    for (var i = 0, n = tween.length, t; i < n; ++i) {
      if ((t = tween[i]).name === name) {
        return t.value;
      }
    }
    return null;
  }
  return this.each((value == null ? tweenRemove : tweenFunction)(id2, name, value));
}
function tweenValue(transition, name, value) {
  var id2 = transition._id;
  transition.each(function() {
    var schedule2 = set(this, id2);
    (schedule2.value || (schedule2.value = {}))[name] = value.apply(this, arguments);
  });
  return function(node) {
    return get(node, id2).value[name];
  };
}
function interpolate(a2, b) {
  var c2;
  return (typeof b === "number" ? interpolateNumber : b instanceof color$1 ? interpolateRgb : (c2 = color$1(b)) ? (b = c2, interpolateRgb) : interpolateString)(a2, b);
}
function attrRemove(name) {
  return function() {
    this.removeAttribute(name);
  };
}
function attrRemoveNS(fullname) {
  return function() {
    this.removeAttributeNS(fullname.space, fullname.local);
  };
}
function attrConstant(name, interpolate2, value1) {
  var string00, string1 = value1 + "", interpolate0;
  return function() {
    var string0 = this.getAttribute(name);
    return string0 === string1 ? null : string0 === string00 ? interpolate0 : interpolate0 = interpolate2(string00 = string0, value1);
  };
}
function attrConstantNS(fullname, interpolate2, value1) {
  var string00, string1 = value1 + "", interpolate0;
  return function() {
    var string0 = this.getAttributeNS(fullname.space, fullname.local);
    return string0 === string1 ? null : string0 === string00 ? interpolate0 : interpolate0 = interpolate2(string00 = string0, value1);
  };
}
function attrFunction(name, interpolate2, value) {
  var string00, string10, interpolate0;
  return function() {
    var string0, value1 = value(this), string1;
    if (value1 == null)
      return void this.removeAttribute(name);
    string0 = this.getAttribute(name);
    string1 = value1 + "";
    return string0 === string1 ? null : string0 === string00 && string1 === string10 ? interpolate0 : (string10 = string1, interpolate0 = interpolate2(string00 = string0, value1));
  };
}
function attrFunctionNS(fullname, interpolate2, value) {
  var string00, string10, interpolate0;
  return function() {
    var string0, value1 = value(this), string1;
    if (value1 == null)
      return void this.removeAttributeNS(fullname.space, fullname.local);
    string0 = this.getAttributeNS(fullname.space, fullname.local);
    string1 = value1 + "";
    return string0 === string1 ? null : string0 === string00 && string1 === string10 ? interpolate0 : (string10 = string1, interpolate0 = interpolate2(string00 = string0, value1));
  };
}
function transition_attr(name, value) {
  var fullname = namespace(name), i = fullname === "transform" ? interpolateTransformSvg : interpolate;
  return this.attrTween(name, typeof value === "function" ? (fullname.local ? attrFunctionNS : attrFunction)(fullname, i, tweenValue(this, "attr." + name, value)) : value == null ? (fullname.local ? attrRemoveNS : attrRemove)(fullname) : (fullname.local ? attrConstantNS : attrConstant)(fullname, i, value));
}
function attrInterpolate(name, i) {
  return function(t) {
    this.setAttribute(name, i.call(this, t));
  };
}
function attrInterpolateNS(fullname, i) {
  return function(t) {
    this.setAttributeNS(fullname.space, fullname.local, i.call(this, t));
  };
}
function attrTweenNS(fullname, value) {
  var t02, i0;
  function tween() {
    var i = value.apply(this, arguments);
    if (i !== i0)
      t02 = (i0 = i) && attrInterpolateNS(fullname, i);
    return t02;
  }
  tween._value = value;
  return tween;
}
function attrTween(name, value) {
  var t02, i0;
  function tween() {
    var i = value.apply(this, arguments);
    if (i !== i0)
      t02 = (i0 = i) && attrInterpolate(name, i);
    return t02;
  }
  tween._value = value;
  return tween;
}
function transition_attrTween(name, value) {
  var key = "attr." + name;
  if (arguments.length < 2)
    return (key = this.tween(key)) && key._value;
  if (value == null)
    return this.tween(key, null);
  if (typeof value !== "function")
    throw new Error();
  var fullname = namespace(name);
  return this.tween(key, (fullname.local ? attrTweenNS : attrTween)(fullname, value));
}
function delayFunction(id2, value) {
  return function() {
    init(this, id2).delay = +value.apply(this, arguments);
  };
}
function delayConstant(id2, value) {
  return value = +value, function() {
    init(this, id2).delay = value;
  };
}
function transition_delay(value) {
  var id2 = this._id;
  return arguments.length ? this.each((typeof value === "function" ? delayFunction : delayConstant)(id2, value)) : get(this.node(), id2).delay;
}
function durationFunction(id2, value) {
  return function() {
    set(this, id2).duration = +value.apply(this, arguments);
  };
}
function durationConstant(id2, value) {
  return value = +value, function() {
    set(this, id2).duration = value;
  };
}
function transition_duration(value) {
  var id2 = this._id;
  return arguments.length ? this.each((typeof value === "function" ? durationFunction : durationConstant)(id2, value)) : get(this.node(), id2).duration;
}
function easeConstant(id2, value) {
  if (typeof value !== "function")
    throw new Error();
  return function() {
    set(this, id2).ease = value;
  };
}
function transition_ease(value) {
  var id2 = this._id;
  return arguments.length ? this.each(easeConstant(id2, value)) : get(this.node(), id2).ease;
}
function easeVarying(id2, value) {
  return function() {
    var v = value.apply(this, arguments);
    if (typeof v !== "function")
      throw new Error();
    set(this, id2).ease = v;
  };
}
function transition_easeVarying(value) {
  if (typeof value !== "function")
    throw new Error();
  return this.each(easeVarying(this._id, value));
}
function transition_filter(match) {
  if (typeof match !== "function")
    match = matcher(match);
  for (var groups2 = this._groups, m = groups2.length, subgroups = new Array(m), j = 0; j < m; ++j) {
    for (var group2 = groups2[j], n = group2.length, subgroup = subgroups[j] = [], node, i = 0; i < n; ++i) {
      if ((node = group2[i]) && match.call(node, node.__data__, i, group2)) {
        subgroup.push(node);
      }
    }
  }
  return new Transition(subgroups, this._parents, this._name, this._id);
}
function transition_merge(transition) {
  if (transition._id !== this._id)
    throw new Error();
  for (var groups0 = this._groups, groups1 = transition._groups, m0 = groups0.length, m1 = groups1.length, m = Math.min(m0, m1), merges = new Array(m0), j = 0; j < m; ++j) {
    for (var group0 = groups0[j], group1 = groups1[j], n = group0.length, merge2 = merges[j] = new Array(n), node, i = 0; i < n; ++i) {
      if (node = group0[i] || group1[i]) {
        merge2[i] = node;
      }
    }
  }
  for (; j < m0; ++j) {
    merges[j] = groups0[j];
  }
  return new Transition(merges, this._parents, this._name, this._id);
}
function start(name) {
  return (name + "").trim().split(/^|\s+/).every(function(t) {
    var i = t.indexOf(".");
    if (i >= 0)
      t = t.slice(0, i);
    return !t || t === "start";
  });
}
function onFunction(id2, name, listener) {
  var on0, on1, sit = start(name) ? init : set;
  return function() {
    var schedule2 = sit(this, id2), on = schedule2.on;
    if (on !== on0)
      (on1 = (on0 = on).copy()).on(name, listener);
    schedule2.on = on1;
  };
}
function transition_on(name, listener) {
  var id2 = this._id;
  return arguments.length < 2 ? get(this.node(), id2).on.on(name) : this.each(onFunction(id2, name, listener));
}
function removeFunction(id2) {
  return function() {
    var parent = this.parentNode;
    for (var i in this.__transition)
      if (+i !== id2)
        return;
    if (parent)
      parent.removeChild(this);
  };
}
function transition_remove() {
  return this.on("end.remove", removeFunction(this._id));
}
function transition_select(select2) {
  var name = this._name, id2 = this._id;
  if (typeof select2 !== "function")
    select2 = selector(select2);
  for (var groups2 = this._groups, m = groups2.length, subgroups = new Array(m), j = 0; j < m; ++j) {
    for (var group2 = groups2[j], n = group2.length, subgroup = subgroups[j] = new Array(n), node, subnode, i = 0; i < n; ++i) {
      if ((node = group2[i]) && (subnode = select2.call(node, node.__data__, i, group2))) {
        if ("__data__" in node)
          subnode.__data__ = node.__data__;
        subgroup[i] = subnode;
        schedule(subgroup[i], name, id2, i, subgroup, get(node, id2));
      }
    }
  }
  return new Transition(subgroups, this._parents, name, id2);
}
function transition_selectAll(select2) {
  var name = this._name, id2 = this._id;
  if (typeof select2 !== "function")
    select2 = selectorAll(select2);
  for (var groups2 = this._groups, m = groups2.length, subgroups = [], parents = [], j = 0; j < m; ++j) {
    for (var group2 = groups2[j], n = group2.length, node, i = 0; i < n; ++i) {
      if (node = group2[i]) {
        for (var children2 = select2.call(node, node.__data__, i, group2), child, inherit2 = get(node, id2), k2 = 0, l = children2.length; k2 < l; ++k2) {
          if (child = children2[k2]) {
            schedule(child, name, id2, k2, children2, inherit2);
          }
        }
        subgroups.push(children2);
        parents.push(node);
      }
    }
  }
  return new Transition(subgroups, parents, name, id2);
}
var Selection = selection.prototype.constructor;
function transition_selection() {
  return new Selection(this._groups, this._parents);
}
function styleNull(name, interpolate2) {
  var string00, string10, interpolate0;
  return function() {
    var string0 = styleValue(this, name), string1 = (this.style.removeProperty(name), styleValue(this, name));
    return string0 === string1 ? null : string0 === string00 && string1 === string10 ? interpolate0 : interpolate0 = interpolate2(string00 = string0, string10 = string1);
  };
}
function styleRemove(name) {
  return function() {
    this.style.removeProperty(name);
  };
}
function styleConstant(name, interpolate2, value1) {
  var string00, string1 = value1 + "", interpolate0;
  return function() {
    var string0 = styleValue(this, name);
    return string0 === string1 ? null : string0 === string00 ? interpolate0 : interpolate0 = interpolate2(string00 = string0, value1);
  };
}
function styleFunction(name, interpolate2, value) {
  var string00, string10, interpolate0;
  return function() {
    var string0 = styleValue(this, name), value1 = value(this), string1 = value1 + "";
    if (value1 == null)
      string1 = value1 = (this.style.removeProperty(name), styleValue(this, name));
    return string0 === string1 ? null : string0 === string00 && string1 === string10 ? interpolate0 : (string10 = string1, interpolate0 = interpolate2(string00 = string0, value1));
  };
}
function styleMaybeRemove(id2, name) {
  var on0, on1, listener0, key = "style." + name, event = "end." + key, remove2;
  return function() {
    var schedule2 = set(this, id2), on = schedule2.on, listener = schedule2.value[key] == null ? remove2 || (remove2 = styleRemove(name)) : void 0;
    if (on !== on0 || listener0 !== listener)
      (on1 = (on0 = on).copy()).on(event, listener0 = listener);
    schedule2.on = on1;
  };
}
function transition_style(name, value, priority) {
  var i = (name += "") === "transform" ? interpolateTransformCss : interpolate;
  return value == null ? this.styleTween(name, styleNull(name, i)).on("end.style." + name, styleRemove(name)) : typeof value === "function" ? this.styleTween(name, styleFunction(name, i, tweenValue(this, "style." + name, value))).each(styleMaybeRemove(this._id, name)) : this.styleTween(name, styleConstant(name, i, value), priority).on("end.style." + name, null);
}
function styleInterpolate(name, i, priority) {
  return function(t) {
    this.style.setProperty(name, i.call(this, t), priority);
  };
}
function styleTween(name, value, priority) {
  var t, i0;
  function tween() {
    var i = value.apply(this, arguments);
    if (i !== i0)
      t = (i0 = i) && styleInterpolate(name, i, priority);
    return t;
  }
  tween._value = value;
  return tween;
}
function transition_styleTween(name, value, priority) {
  var key = "style." + (name += "");
  if (arguments.length < 2)
    return (key = this.tween(key)) && key._value;
  if (value == null)
    return this.tween(key, null);
  if (typeof value !== "function")
    throw new Error();
  return this.tween(key, styleTween(name, value, priority == null ? "" : priority));
}
function textConstant(value) {
  return function() {
    this.textContent = value;
  };
}
function textFunction(value) {
  return function() {
    var value1 = value(this);
    this.textContent = value1 == null ? "" : value1;
  };
}
function transition_text(value) {
  return this.tween("text", typeof value === "function" ? textFunction(tweenValue(this, "text", value)) : textConstant(value == null ? "" : value + ""));
}
function textInterpolate(i) {
  return function(t) {
    this.textContent = i.call(this, t);
  };
}
function textTween(value) {
  var t02, i0;
  function tween() {
    var i = value.apply(this, arguments);
    if (i !== i0)
      t02 = (i0 = i) && textInterpolate(i);
    return t02;
  }
  tween._value = value;
  return tween;
}
function transition_textTween(value) {
  var key = "text";
  if (arguments.length < 1)
    return (key = this.tween(key)) && key._value;
  if (value == null)
    return this.tween(key, null);
  if (typeof value !== "function")
    throw new Error();
  return this.tween(key, textTween(value));
}
function transition_transition() {
  var name = this._name, id0 = this._id, id1 = newId();
  for (var groups2 = this._groups, m = groups2.length, j = 0; j < m; ++j) {
    for (var group2 = groups2[j], n = group2.length, node, i = 0; i < n; ++i) {
      if (node = group2[i]) {
        var inherit2 = get(node, id0);
        schedule(node, name, id1, i, group2, {
          time: inherit2.time + inherit2.delay + inherit2.duration,
          delay: 0,
          duration: inherit2.duration,
          ease: inherit2.ease
        });
      }
    }
  }
  return new Transition(groups2, this._parents, name, id1);
}
function transition_end() {
  var on0, on1, that = this, id2 = that._id, size = that.size();
  return new Promise(function(resolve, reject) {
    var cancel = { value: reject }, end = { value: function() {
      if (--size === 0)
        resolve();
    } };
    that.each(function() {
      var schedule2 = set(this, id2), on = schedule2.on;
      if (on !== on0) {
        on1 = (on0 = on).copy();
        on1._.cancel.push(cancel);
        on1._.interrupt.push(cancel);
        on1._.end.push(end);
      }
      schedule2.on = on1;
    });
    if (size === 0)
      resolve();
  });
}
var id = 0;
function Transition(groups2, parents, name, id2) {
  this._groups = groups2;
  this._parents = parents;
  this._name = name;
  this._id = id2;
}
function newId() {
  return ++id;
}
var selection_prototype = selection.prototype;
Transition.prototype = {
  constructor: Transition,
  select: transition_select,
  selectAll: transition_selectAll,
  selectChild: selection_prototype.selectChild,
  selectChildren: selection_prototype.selectChildren,
  filter: transition_filter,
  merge: transition_merge,
  selection: transition_selection,
  transition: transition_transition,
  call: selection_prototype.call,
  nodes: selection_prototype.nodes,
  node: selection_prototype.node,
  size: selection_prototype.size,
  empty: selection_prototype.empty,
  each: selection_prototype.each,
  on: transition_on,
  attr: transition_attr,
  attrTween: transition_attrTween,
  style: transition_style,
  styleTween: transition_styleTween,
  text: transition_text,
  textTween: transition_textTween,
  remove: transition_remove,
  tween: transition_tween,
  delay: transition_delay,
  duration: transition_duration,
  ease: transition_ease,
  easeVarying: transition_easeVarying,
  end: transition_end,
  [Symbol.iterator]: selection_prototype[Symbol.iterator]
};
function cubicInOut(t) {
  return ((t *= 2) <= 1 ? t * t * t : (t -= 2) * t * t + 2) / 2;
}
var defaultTiming = {
  time: null,
  // Set on use.
  delay: 0,
  duration: 250,
  ease: cubicInOut
};
function inherit$1(node, id2) {
  var timing;
  while (!(timing = node.__transition) || !(timing = timing[id2])) {
    if (!(node = node.parentNode)) {
      throw new Error(`transition ${id2} not found`);
    }
  }
  return timing;
}
function selection_transition(name) {
  var id2, timing;
  if (name instanceof Transition) {
    id2 = name._id, name = name._name;
  } else {
    id2 = newId(), (timing = defaultTiming).time = now(), name = name == null ? null : name + "";
  }
  for (var groups2 = this._groups, m = groups2.length, j = 0; j < m; ++j) {
    for (var group2 = groups2[j], n = group2.length, node, i = 0; i < n; ++i) {
      if (node = group2[i]) {
        schedule(node, name, id2, i, group2, timing || inherit$1(node, id2));
      }
    }
  }
  return new Transition(groups2, this._parents, name, id2);
}
selection.prototype.interrupt = selection_interrupt;
selection.prototype.transition = selection_transition;
const pi$3 = Math.PI, tau$3 = 2 * pi$3, epsilon$1 = 1e-6, tauEpsilon = tau$3 - epsilon$1;
function append$1(strings) {
  this._ += strings[0];
  for (let i = 1, n = strings.length; i < n; ++i) {
    this._ += arguments[i] + strings[i];
  }
}
function appendRound$1(digits) {
  let d = Math.floor(digits);
  if (!(d >= 0))
    throw new Error(`invalid digits: ${digits}`);
  if (d > 15)
    return append$1;
  const k2 = 10 ** d;
  return function(strings) {
    this._ += strings[0];
    for (let i = 1, n = strings.length; i < n; ++i) {
      this._ += Math.round(arguments[i] * k2) / k2 + strings[i];
    }
  };
}
class Path {
  constructor(digits) {
    this._x0 = this._y0 = // start of current subpath
    this._x1 = this._y1 = null;
    this._ = "";
    this._append = digits == null ? append$1 : appendRound$1(digits);
  }
  moveTo(x, y) {
    this._append`M${this._x0 = this._x1 = +x},${this._y0 = this._y1 = +y}`;
  }
  closePath() {
    if (this._x1 !== null) {
      this._x1 = this._x0, this._y1 = this._y0;
      this._append`Z`;
    }
  }
  lineTo(x, y) {
    this._append`L${this._x1 = +x},${this._y1 = +y}`;
  }
  quadraticCurveTo(x12, y12, x, y) {
    this._append`Q${+x12},${+y12},${this._x1 = +x},${this._y1 = +y}`;
  }
  bezierCurveTo(x12, y12, x2, y2, x, y) {
    this._append`C${+x12},${+y12},${+x2},${+y2},${this._x1 = +x},${this._y1 = +y}`;
  }
  arcTo(x12, y12, x2, y2, r) {
    x12 = +x12, y12 = +y12, x2 = +x2, y2 = +y2, r = +r;
    if (r < 0)
      throw new Error(`negative radius: ${r}`);
    let x02 = this._x1, y02 = this._y1, x21 = x2 - x12, y21 = y2 - y12, x01 = x02 - x12, y01 = y02 - y12, l01_2 = x01 * x01 + y01 * y01;
    if (this._x1 === null) {
      this._append`M${this._x1 = x12},${this._y1 = y12}`;
    } else if (!(l01_2 > epsilon$1))
      ;
    else if (!(Math.abs(y01 * x21 - y21 * x01) > epsilon$1) || !r) {
      this._append`L${this._x1 = x12},${this._y1 = y12}`;
    } else {
      let x20 = x2 - x02, y20 = y2 - y02, l21_2 = x21 * x21 + y21 * y21, l20_2 = x20 * x20 + y20 * y20, l21 = Math.sqrt(l21_2), l01 = Math.sqrt(l01_2), l = r * Math.tan((pi$3 - Math.acos((l21_2 + l01_2 - l20_2) / (2 * l21 * l01))) / 2), t01 = l / l01, t21 = l / l21;
      if (Math.abs(t01 - 1) > epsilon$1) {
        this._append`L${x12 + t01 * x01},${y12 + t01 * y01}`;
      }
      this._append`A${r},${r},0,0,${+(y01 * x20 > x01 * y20)},${this._x1 = x12 + t21 * x21},${this._y1 = y12 + t21 * y21}`;
    }
  }
  arc(x, y, r, a0, a1, ccw) {
    x = +x, y = +y, r = +r, ccw = !!ccw;
    if (r < 0)
      throw new Error(`negative radius: ${r}`);
    let dx = r * Math.cos(a0), dy = r * Math.sin(a0), x02 = x + dx, y02 = y + dy, cw = 1 ^ ccw, da = ccw ? a0 - a1 : a1 - a0;
    if (this._x1 === null) {
      this._append`M${x02},${y02}`;
    } else if (Math.abs(this._x1 - x02) > epsilon$1 || Math.abs(this._y1 - y02) > epsilon$1) {
      this._append`L${x02},${y02}`;
    }
    if (!r)
      return;
    if (da < 0)
      da = da % tau$3 + tau$3;
    if (da > tauEpsilon) {
      this._append`A${r},${r},0,1,${cw},${x - dx},${y - dy}A${r},${r},0,1,${cw},${this._x1 = x02},${this._y1 = y02}`;
    } else if (da > epsilon$1) {
      this._append`A${r},${r},0,${+(da >= pi$3)},${cw},${this._x1 = x + r * Math.cos(a1)},${this._y1 = y + r * Math.sin(a1)}`;
    }
  }
  rect(x, y, w, h) {
    this._append`M${this._x0 = this._x1 = +x},${this._y0 = this._y1 = +y}h${w = +w}v${+h}h${-w}Z`;
  }
  toString() {
    return this._;
  }
}
function pathRound(digits = 3) {
  return new Path(+digits);
}
function formatDecimal(x) {
  return Math.abs(x = Math.round(x)) >= 1e21 ? x.toLocaleString("en").replace(/,/g, "") : x.toString(10);
}
function formatDecimalParts(x, p) {
  if ((i = (x = p ? x.toExponential(p - 1) : x.toExponential()).indexOf("e")) < 0)
    return null;
  var i, coefficient = x.slice(0, i);
  return [
    coefficient.length > 1 ? coefficient[0] + coefficient.slice(2) : coefficient,
    +x.slice(i + 1)
  ];
}
function exponent(x) {
  return x = formatDecimalParts(Math.abs(x)), x ? x[1] : NaN;
}
function formatGroup(grouping, thousands) {
  return function(value, width) {
    var i = value.length, t = [], j = 0, g = grouping[0], length2 = 0;
    while (i > 0 && g > 0) {
      if (length2 + g + 1 > width)
        g = Math.max(1, width - length2);
      t.push(value.substring(i -= g, i + g));
      if ((length2 += g + 1) > width)
        break;
      g = grouping[j = (j + 1) % grouping.length];
    }
    return t.reverse().join(thousands);
  };
}
function formatNumerals(numerals) {
  return function(value) {
    return value.replace(/[0-9]/g, function(i) {
      return numerals[+i];
    });
  };
}
var re$1 = /^(?:(.)?([<>=^]))?([+\-( ])?([$#])?(0)?(\d+)?(,)?(\.\d+)?(~)?([a-z%])?$/i;
function formatSpecifier(specifier) {
  if (!(match = re$1.exec(specifier)))
    throw new Error("invalid format: " + specifier);
  var match;
  return new FormatSpecifier({
    fill: match[1],
    align: match[2],
    sign: match[3],
    symbol: match[4],
    zero: match[5],
    width: match[6],
    comma: match[7],
    precision: match[8] && match[8].slice(1),
    trim: match[9],
    type: match[10]
  });
}
formatSpecifier.prototype = FormatSpecifier.prototype;
function FormatSpecifier(specifier) {
  this.fill = specifier.fill === void 0 ? " " : specifier.fill + "";
  this.align = specifier.align === void 0 ? ">" : specifier.align + "";
  this.sign = specifier.sign === void 0 ? "-" : specifier.sign + "";
  this.symbol = specifier.symbol === void 0 ? "" : specifier.symbol + "";
  this.zero = !!specifier.zero;
  this.width = specifier.width === void 0 ? void 0 : +specifier.width;
  this.comma = !!specifier.comma;
  this.precision = specifier.precision === void 0 ? void 0 : +specifier.precision;
  this.trim = !!specifier.trim;
  this.type = specifier.type === void 0 ? "" : specifier.type + "";
}
FormatSpecifier.prototype.toString = function() {
  return this.fill + this.align + this.sign + this.symbol + (this.zero ? "0" : "") + (this.width === void 0 ? "" : Math.max(1, this.width | 0)) + (this.comma ? "," : "") + (this.precision === void 0 ? "" : "." + Math.max(0, this.precision | 0)) + (this.trim ? "~" : "") + this.type;
};
function formatTrim(s2) {
  out:
    for (var n = s2.length, i = 1, i0 = -1, i1; i < n; ++i) {
      switch (s2[i]) {
        case ".":
          i0 = i1 = i;
          break;
        case "0":
          if (i0 === 0)
            i0 = i;
          i1 = i;
          break;
        default:
          if (!+s2[i])
            break out;
          if (i0 > 0)
            i0 = 0;
          break;
      }
    }
  return i0 > 0 ? s2.slice(0, i0) + s2.slice(i1 + 1) : s2;
}
var prefixExponent;
function formatPrefixAuto(x, p) {
  var d = formatDecimalParts(x, p);
  if (!d)
    return x + "";
  var coefficient = d[0], exponent2 = d[1], i = exponent2 - (prefixExponent = Math.max(-8, Math.min(8, Math.floor(exponent2 / 3))) * 3) + 1, n = coefficient.length;
  return i === n ? coefficient : i > n ? coefficient + new Array(i - n + 1).join("0") : i > 0 ? coefficient.slice(0, i) + "." + coefficient.slice(i) : "0." + new Array(1 - i).join("0") + formatDecimalParts(x, Math.max(0, p + i - 1))[0];
}
function formatRounded(x, p) {
  var d = formatDecimalParts(x, p);
  if (!d)
    return x + "";
  var coefficient = d[0], exponent2 = d[1];
  return exponent2 < 0 ? "0." + new Array(-exponent2).join("0") + coefficient : coefficient.length > exponent2 + 1 ? coefficient.slice(0, exponent2 + 1) + "." + coefficient.slice(exponent2 + 1) : coefficient + new Array(exponent2 - coefficient.length + 2).join("0");
}
const formatTypes = {
  "%": (x, p) => (x * 100).toFixed(p),
  "b": (x) => Math.round(x).toString(2),
  "c": (x) => x + "",
  "d": formatDecimal,
  "e": (x, p) => x.toExponential(p),
  "f": (x, p) => x.toFixed(p),
  "g": (x, p) => x.toPrecision(p),
  "o": (x) => Math.round(x).toString(8),
  "p": (x, p) => formatRounded(x * 100, p),
  "r": formatRounded,
  "s": formatPrefixAuto,
  "X": (x) => Math.round(x).toString(16).toUpperCase(),
  "x": (x) => Math.round(x).toString(16)
};
function identity$5(x) {
  return x;
}
var map$1 = Array.prototype.map, prefixes = ["y", "z", "a", "f", "p", "n", "µ", "m", "", "k", "M", "G", "T", "P", "E", "Z", "Y"];
function formatLocale$1(locale2) {
  var group2 = locale2.grouping === void 0 || locale2.thousands === void 0 ? identity$5 : formatGroup(map$1.call(locale2.grouping, Number), locale2.thousands + ""), currencyPrefix = locale2.currency === void 0 ? "" : locale2.currency[0] + "", currencySuffix = locale2.currency === void 0 ? "" : locale2.currency[1] + "", decimal = locale2.decimal === void 0 ? "." : locale2.decimal + "", numerals = locale2.numerals === void 0 ? identity$5 : formatNumerals(map$1.call(locale2.numerals, String)), percent = locale2.percent === void 0 ? "%" : locale2.percent + "", minus = locale2.minus === void 0 ? "−" : locale2.minus + "", nan = locale2.nan === void 0 ? "NaN" : locale2.nan + "";
  function newFormat(specifier) {
    specifier = formatSpecifier(specifier);
    var fill = specifier.fill, align = specifier.align, sign2 = specifier.sign, symbol2 = specifier.symbol, zero2 = specifier.zero, width = specifier.width, comma = specifier.comma, precision = specifier.precision, trim = specifier.trim, type = specifier.type;
    if (type === "n")
      comma = true, type = "g";
    else if (!formatTypes[type])
      precision === void 0 && (precision = 12), trim = true, type = "g";
    if (zero2 || fill === "0" && align === "=")
      zero2 = true, fill = "0", align = "=";
    var prefix = symbol2 === "$" ? currencyPrefix : symbol2 === "#" && /[boxX]/.test(type) ? "0" + type.toLowerCase() : "", suffix = symbol2 === "$" ? currencySuffix : /[%p]/.test(type) ? percent : "";
    var formatType = formatTypes[type], maybeSuffix = /[defgprs%]/.test(type);
    precision = precision === void 0 ? 6 : /[gprs]/.test(type) ? Math.max(1, Math.min(21, precision)) : Math.max(0, Math.min(20, precision));
    function format2(value) {
      var valuePrefix = prefix, valueSuffix = suffix, i, n, c2;
      if (type === "c") {
        valueSuffix = formatType(value) + valueSuffix;
        value = "";
      } else {
        value = +value;
        var valueNegative = value < 0 || 1 / value < 0;
        value = isNaN(value) ? nan : formatType(Math.abs(value), precision);
        if (trim)
          value = formatTrim(value);
        if (valueNegative && +value === 0 && sign2 !== "+")
          valueNegative = false;
        valuePrefix = (valueNegative ? sign2 === "(" ? sign2 : minus : sign2 === "-" || sign2 === "(" ? "" : sign2) + valuePrefix;
        valueSuffix = (type === "s" ? prefixes[8 + prefixExponent / 3] : "") + valueSuffix + (valueNegative && sign2 === "(" ? ")" : "");
        if (maybeSuffix) {
          i = -1, n = value.length;
          while (++i < n) {
            if (c2 = value.charCodeAt(i), 48 > c2 || c2 > 57) {
              valueSuffix = (c2 === 46 ? decimal + value.slice(i + 1) : value.slice(i)) + valueSuffix;
              value = value.slice(0, i);
              break;
            }
          }
        }
      }
      if (comma && !zero2)
        value = group2(value, Infinity);
      var length2 = valuePrefix.length + value.length + valueSuffix.length, padding = length2 < width ? new Array(width - length2 + 1).join(fill) : "";
      if (comma && zero2)
        value = group2(padding + value, padding.length ? width - valueSuffix.length : Infinity), padding = "";
      switch (align) {
        case "<":
          value = valuePrefix + value + valueSuffix + padding;
          break;
        case "=":
          value = valuePrefix + padding + value + valueSuffix;
          break;
        case "^":
          value = padding.slice(0, length2 = padding.length >> 1) + valuePrefix + value + valueSuffix + padding.slice(length2);
          break;
        default:
          value = padding + valuePrefix + value + valueSuffix;
          break;
      }
      return numerals(value);
    }
    format2.toString = function() {
      return specifier + "";
    };
    return format2;
  }
  function formatPrefix2(specifier, value) {
    var f = newFormat((specifier = formatSpecifier(specifier), specifier.type = "f", specifier)), e = Math.max(-8, Math.min(8, Math.floor(exponent(value) / 3))) * 3, k2 = Math.pow(10, -e), prefix = prefixes[8 + e / 3];
    return function(value2) {
      return f(k2 * value2) + prefix;
    };
  }
  return {
    format: newFormat,
    formatPrefix: formatPrefix2
  };
}
var locale$1;
var format$1;
var formatPrefix;
defaultLocale$1({
  thousands: ",",
  grouping: [3],
  currency: ["$", ""]
});
function defaultLocale$1(definition) {
  locale$1 = formatLocale$1(definition);
  format$1 = locale$1.format;
  formatPrefix = locale$1.formatPrefix;
  return locale$1;
}
function precisionFixed(step) {
  return Math.max(0, -exponent(Math.abs(step)));
}
function precisionPrefix(step, value) {
  return Math.max(0, Math.max(-8, Math.min(8, Math.floor(exponent(value) / 3))) * 3 - exponent(Math.abs(step)));
}
function precisionRound(step, max2) {
  step = Math.abs(step), max2 = Math.abs(max2) - step;
  return Math.max(0, exponent(max2) - exponent(step)) + 1;
}
var epsilon = 1e-6;
var epsilon2 = 1e-12;
var pi$2 = Math.PI;
var halfPi = pi$2 / 2;
var quarterPi = pi$2 / 4;
var tau$2 = pi$2 * 2;
var degrees = 180 / pi$2;
var radians$1 = pi$2 / 180;
var abs = Math.abs;
var atan = Math.atan;
var atan2 = Math.atan2;
var cos$1 = Math.cos;
var exp = Math.exp;
var log$1 = Math.log;
var pow$1 = Math.pow;
var sin$1 = Math.sin;
var sign = Math.sign || function(x) {
  return x > 0 ? 1 : x < 0 ? -1 : 0;
};
var sqrt$1 = Math.sqrt;
var tan = Math.tan;
function acos(x) {
  return x > 1 ? 0 : x < -1 ? pi$2 : Math.acos(x);
}
function asin(x) {
  return x > 1 ? halfPi : x < -1 ? -halfPi : Math.asin(x);
}
function noop() {
}
function streamGeometry(geometry, stream) {
  if (geometry && streamGeometryType.hasOwnProperty(geometry.type)) {
    streamGeometryType[geometry.type](geometry, stream);
  }
}
var streamObjectType = {
  Feature: function(object2, stream) {
    streamGeometry(object2.geometry, stream);
  },
  FeatureCollection: function(object2, stream) {
    var features = object2.features, i = -1, n = features.length;
    while (++i < n)
      streamGeometry(features[i].geometry, stream);
  }
};
var streamGeometryType = {
  Sphere: function(object2, stream) {
    stream.sphere();
  },
  Point: function(object2, stream) {
    object2 = object2.coordinates;
    stream.point(object2[0], object2[1], object2[2]);
  },
  MultiPoint: function(object2, stream) {
    var coordinates = object2.coordinates, i = -1, n = coordinates.length;
    while (++i < n)
      object2 = coordinates[i], stream.point(object2[0], object2[1], object2[2]);
  },
  LineString: function(object2, stream) {
    streamLine(object2.coordinates, stream, 0);
  },
  MultiLineString: function(object2, stream) {
    var coordinates = object2.coordinates, i = -1, n = coordinates.length;
    while (++i < n)
      streamLine(coordinates[i], stream, 0);
  },
  Polygon: function(object2, stream) {
    streamPolygon(object2.coordinates, stream);
  },
  MultiPolygon: function(object2, stream) {
    var coordinates = object2.coordinates, i = -1, n = coordinates.length;
    while (++i < n)
      streamPolygon(coordinates[i], stream);
  },
  GeometryCollection: function(object2, stream) {
    var geometries = object2.geometries, i = -1, n = geometries.length;
    while (++i < n)
      streamGeometry(geometries[i], stream);
  }
};
function streamLine(coordinates, stream, closed) {
  var i = -1, n = coordinates.length - closed, coordinate;
  stream.lineStart();
  while (++i < n)
    coordinate = coordinates[i], stream.point(coordinate[0], coordinate[1], coordinate[2]);
  stream.lineEnd();
}
function streamPolygon(coordinates, stream) {
  var i = -1, n = coordinates.length;
  stream.polygonStart();
  while (++i < n)
    streamLine(coordinates[i], stream, 1);
  stream.polygonEnd();
}
function geoStream(object2, stream) {
  if (object2 && streamObjectType.hasOwnProperty(object2.type)) {
    streamObjectType[object2.type](object2, stream);
  } else {
    streamGeometry(object2, stream);
  }
}
function spherical(cartesian2) {
  return [atan2(cartesian2[1], cartesian2[0]), asin(cartesian2[2])];
}
function cartesian(spherical2) {
  var lambda = spherical2[0], phi = spherical2[1], cosPhi = cos$1(phi);
  return [cosPhi * cos$1(lambda), cosPhi * sin$1(lambda), sin$1(phi)];
}
function cartesianDot(a2, b) {
  return a2[0] * b[0] + a2[1] * b[1] + a2[2] * b[2];
}
function cartesianCross(a2, b) {
  return [a2[1] * b[2] - a2[2] * b[1], a2[2] * b[0] - a2[0] * b[2], a2[0] * b[1] - a2[1] * b[0]];
}
function cartesianAddInPlace(a2, b) {
  a2[0] += b[0], a2[1] += b[1], a2[2] += b[2];
}
function cartesianScale(vector, k2) {
  return [vector[0] * k2, vector[1] * k2, vector[2] * k2];
}
function cartesianNormalizeInPlace(d) {
  var l = sqrt$1(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  d[0] /= l, d[1] /= l, d[2] /= l;
}
function compose(a2, b) {
  function compose2(x, y) {
    return x = a2(x, y), b(x[0], x[1]);
  }
  if (a2.invert && b.invert)
    compose2.invert = function(x, y) {
      return x = b.invert(x, y), x && a2.invert(x[0], x[1]);
    };
  return compose2;
}
function rotationIdentity(lambda, phi) {
  if (abs(lambda) > pi$2)
    lambda -= Math.round(lambda / tau$2) * tau$2;
  return [lambda, phi];
}
rotationIdentity.invert = rotationIdentity;
function rotateRadians(deltaLambda, deltaPhi, deltaGamma) {
  return (deltaLambda %= tau$2) ? deltaPhi || deltaGamma ? compose(rotationLambda(deltaLambda), rotationPhiGamma(deltaPhi, deltaGamma)) : rotationLambda(deltaLambda) : deltaPhi || deltaGamma ? rotationPhiGamma(deltaPhi, deltaGamma) : rotationIdentity;
}
function forwardRotationLambda(deltaLambda) {
  return function(lambda, phi) {
    lambda += deltaLambda;
    if (abs(lambda) > pi$2)
      lambda -= Math.round(lambda / tau$2) * tau$2;
    return [lambda, phi];
  };
}
function rotationLambda(deltaLambda) {
  var rotation2 = forwardRotationLambda(deltaLambda);
  rotation2.invert = forwardRotationLambda(-deltaLambda);
  return rotation2;
}
function rotationPhiGamma(deltaPhi, deltaGamma) {
  var cosDeltaPhi = cos$1(deltaPhi), sinDeltaPhi = sin$1(deltaPhi), cosDeltaGamma = cos$1(deltaGamma), sinDeltaGamma = sin$1(deltaGamma);
  function rotation2(lambda, phi) {
    var cosPhi = cos$1(phi), x = cos$1(lambda) * cosPhi, y = sin$1(lambda) * cosPhi, z = sin$1(phi), k2 = z * cosDeltaPhi + x * sinDeltaPhi;
    return [
      atan2(y * cosDeltaGamma - k2 * sinDeltaGamma, x * cosDeltaPhi - z * sinDeltaPhi),
      asin(k2 * cosDeltaGamma + y * sinDeltaGamma)
    ];
  }
  rotation2.invert = function(lambda, phi) {
    var cosPhi = cos$1(phi), x = cos$1(lambda) * cosPhi, y = sin$1(lambda) * cosPhi, z = sin$1(phi), k2 = z * cosDeltaGamma - y * sinDeltaGamma;
    return [
      atan2(y * cosDeltaGamma + z * sinDeltaGamma, x * cosDeltaPhi + k2 * sinDeltaPhi),
      asin(k2 * cosDeltaPhi - x * sinDeltaPhi)
    ];
  };
  return rotation2;
}
function rotation(rotate) {
  rotate = rotateRadians(rotate[0] * radians$1, rotate[1] * radians$1, rotate.length > 2 ? rotate[2] * radians$1 : 0);
  function forward(coordinates) {
    coordinates = rotate(coordinates[0] * radians$1, coordinates[1] * radians$1);
    return coordinates[0] *= degrees, coordinates[1] *= degrees, coordinates;
  }
  forward.invert = function(coordinates) {
    coordinates = rotate.invert(coordinates[0] * radians$1, coordinates[1] * radians$1);
    return coordinates[0] *= degrees, coordinates[1] *= degrees, coordinates;
  };
  return forward;
}
function circleStream(stream, radius2, delta, direction, t02, t12) {
  if (!delta)
    return;
  var cosRadius = cos$1(radius2), sinRadius = sin$1(radius2), step = direction * delta;
  if (t02 == null) {
    t02 = radius2 + direction * tau$2;
    t12 = radius2 - step / 2;
  } else {
    t02 = circleRadius(cosRadius, t02);
    t12 = circleRadius(cosRadius, t12);
    if (direction > 0 ? t02 < t12 : t02 > t12)
      t02 += direction * tau$2;
  }
  for (var point2, t = t02; direction > 0 ? t > t12 : t < t12; t -= step) {
    point2 = spherical([cosRadius, -sinRadius * cos$1(t), -sinRadius * sin$1(t)]);
    stream.point(point2[0], point2[1]);
  }
}
function circleRadius(cosRadius, point2) {
  point2 = cartesian(point2), point2[0] -= cosRadius;
  cartesianNormalizeInPlace(point2);
  var radius2 = acos(-point2[1]);
  return ((-point2[2] < 0 ? -radius2 : radius2) + tau$2 - epsilon) % tau$2;
}
function clipBuffer() {
  var lines = [], line;
  return {
    point: function(x, y, m) {
      line.push([x, y, m]);
    },
    lineStart: function() {
      lines.push(line = []);
    },
    lineEnd: noop,
    rejoin: function() {
      if (lines.length > 1)
        lines.push(lines.pop().concat(lines.shift()));
    },
    result: function() {
      var result = lines;
      lines = [];
      line = null;
      return result;
    }
  };
}
function pointEqual(a2, b) {
  return abs(a2[0] - b[0]) < epsilon && abs(a2[1] - b[1]) < epsilon;
}
function Intersection(point2, points, other, entry) {
  this.x = point2;
  this.z = points;
  this.o = other;
  this.e = entry;
  this.v = false;
  this.n = this.p = null;
}
function clipRejoin(segments, compareIntersection2, startInside, interpolate2, stream) {
  var subject = [], clip2 = [], i, n;
  segments.forEach(function(segment) {
    if ((n2 = segment.length - 1) <= 0)
      return;
    var n2, p0 = segment[0], p1 = segment[n2], x;
    if (pointEqual(p0, p1)) {
      if (!p0[2] && !p1[2]) {
        stream.lineStart();
        for (i = 0; i < n2; ++i)
          stream.point((p0 = segment[i])[0], p0[1]);
        stream.lineEnd();
        return;
      }
      p1[0] += 2 * epsilon;
    }
    subject.push(x = new Intersection(p0, segment, null, true));
    clip2.push(x.o = new Intersection(p0, null, x, false));
    subject.push(x = new Intersection(p1, segment, null, false));
    clip2.push(x.o = new Intersection(p1, null, x, true));
  });
  if (!subject.length)
    return;
  clip2.sort(compareIntersection2);
  link(subject);
  link(clip2);
  for (i = 0, n = clip2.length; i < n; ++i) {
    clip2[i].e = startInside = !startInside;
  }
  var start2 = subject[0], points, point2;
  while (1) {
    var current = start2, isSubject = true;
    while (current.v)
      if ((current = current.n) === start2)
        return;
    points = current.z;
    stream.lineStart();
    do {
      current.v = current.o.v = true;
      if (current.e) {
        if (isSubject) {
          for (i = 0, n = points.length; i < n; ++i)
            stream.point((point2 = points[i])[0], point2[1]);
        } else {
          interpolate2(current.x, current.n.x, 1, stream);
        }
        current = current.n;
      } else {
        if (isSubject) {
          points = current.p.z;
          for (i = points.length - 1; i >= 0; --i)
            stream.point((point2 = points[i])[0], point2[1]);
        } else {
          interpolate2(current.x, current.p.x, -1, stream);
        }
        current = current.p;
      }
      current = current.o;
      points = current.z;
      isSubject = !isSubject;
    } while (!current.v);
    stream.lineEnd();
  }
}
function link(array2) {
  if (!(n = array2.length))
    return;
  var n, i = 0, a2 = array2[0], b;
  while (++i < n) {
    a2.n = b = array2[i];
    b.p = a2;
    a2 = b;
  }
  a2.n = b = array2[0];
  b.p = a2;
}
function longitude(point2) {
  return abs(point2[0]) <= pi$2 ? point2[0] : sign(point2[0]) * ((abs(point2[0]) + pi$2) % tau$2 - pi$2);
}
function polygonContains(polygon, point2) {
  var lambda = longitude(point2), phi = point2[1], sinPhi = sin$1(phi), normal = [sin$1(lambda), -cos$1(lambda), 0], angle = 0, winding = 0;
  var sum2 = new Adder();
  if (sinPhi === 1)
    phi = halfPi + epsilon;
  else if (sinPhi === -1)
    phi = -halfPi - epsilon;
  for (var i = 0, n = polygon.length; i < n; ++i) {
    if (!(m = (ring = polygon[i]).length))
      continue;
    var ring, m, point0 = ring[m - 1], lambda0 = longitude(point0), phi0 = point0[1] / 2 + quarterPi, sinPhi0 = sin$1(phi0), cosPhi0 = cos$1(phi0);
    for (var j = 0; j < m; ++j, lambda0 = lambda1, sinPhi0 = sinPhi1, cosPhi0 = cosPhi1, point0 = point1) {
      var point1 = ring[j], lambda1 = longitude(point1), phi1 = point1[1] / 2 + quarterPi, sinPhi1 = sin$1(phi1), cosPhi1 = cos$1(phi1), delta = lambda1 - lambda0, sign2 = delta >= 0 ? 1 : -1, absDelta = sign2 * delta, antimeridian = absDelta > pi$2, k2 = sinPhi0 * sinPhi1;
      sum2.add(atan2(k2 * sign2 * sin$1(absDelta), cosPhi0 * cosPhi1 + k2 * cos$1(absDelta)));
      angle += antimeridian ? delta + sign2 * tau$2 : delta;
      if (antimeridian ^ lambda0 >= lambda ^ lambda1 >= lambda) {
        var arc = cartesianCross(cartesian(point0), cartesian(point1));
        cartesianNormalizeInPlace(arc);
        var intersection = cartesianCross(normal, arc);
        cartesianNormalizeInPlace(intersection);
        var phiArc = (antimeridian ^ delta >= 0 ? -1 : 1) * asin(intersection[2]);
        if (phi > phiArc || phi === phiArc && (arc[0] || arc[1])) {
          winding += antimeridian ^ delta >= 0 ? 1 : -1;
        }
      }
    }
  }
  return (angle < -epsilon || angle < epsilon && sum2 < -epsilon2) ^ winding & 1;
}
function clip(pointVisible, clipLine2, interpolate2, start2) {
  return function(sink) {
    var line = clipLine2(sink), ringBuffer = clipBuffer(), ringSink = clipLine2(ringBuffer), polygonStarted = false, polygon, segments, ring;
    var clip2 = {
      point: point2,
      lineStart,
      lineEnd,
      polygonStart: function() {
        clip2.point = pointRing;
        clip2.lineStart = ringStart;
        clip2.lineEnd = ringEnd;
        segments = [];
        polygon = [];
      },
      polygonEnd: function() {
        clip2.point = point2;
        clip2.lineStart = lineStart;
        clip2.lineEnd = lineEnd;
        segments = merge(segments);
        var startInside = polygonContains(polygon, start2);
        if (segments.length) {
          if (!polygonStarted)
            sink.polygonStart(), polygonStarted = true;
          clipRejoin(segments, compareIntersection, startInside, interpolate2, sink);
        } else if (startInside) {
          if (!polygonStarted)
            sink.polygonStart(), polygonStarted = true;
          sink.lineStart();
          interpolate2(null, null, 1, sink);
          sink.lineEnd();
        }
        if (polygonStarted)
          sink.polygonEnd(), polygonStarted = false;
        segments = polygon = null;
      },
      sphere: function() {
        sink.polygonStart();
        sink.lineStart();
        interpolate2(null, null, 1, sink);
        sink.lineEnd();
        sink.polygonEnd();
      }
    };
    function point2(lambda, phi) {
      if (pointVisible(lambda, phi))
        sink.point(lambda, phi);
    }
    function pointLine(lambda, phi) {
      line.point(lambda, phi);
    }
    function lineStart() {
      clip2.point = pointLine;
      line.lineStart();
    }
    function lineEnd() {
      clip2.point = point2;
      line.lineEnd();
    }
    function pointRing(lambda, phi) {
      ring.push([lambda, phi]);
      ringSink.point(lambda, phi);
    }
    function ringStart() {
      ringSink.lineStart();
      ring = [];
    }
    function ringEnd() {
      pointRing(ring[0][0], ring[0][1]);
      ringSink.lineEnd();
      var clean = ringSink.clean(), ringSegments = ringBuffer.result(), i, n = ringSegments.length, m, segment, point3;
      ring.pop();
      polygon.push(ring);
      ring = null;
      if (!n)
        return;
      if (clean & 1) {
        segment = ringSegments[0];
        if ((m = segment.length - 1) > 0) {
          if (!polygonStarted)
            sink.polygonStart(), polygonStarted = true;
          sink.lineStart();
          for (i = 0; i < m; ++i)
            sink.point((point3 = segment[i])[0], point3[1]);
          sink.lineEnd();
        }
        return;
      }
      if (n > 1 && clean & 2)
        ringSegments.push(ringSegments.pop().concat(ringSegments.shift()));
      segments.push(ringSegments.filter(validSegment));
    }
    return clip2;
  };
}
function validSegment(segment) {
  return segment.length > 1;
}
function compareIntersection(a2, b) {
  return ((a2 = a2.x)[0] < 0 ? a2[1] - halfPi - epsilon : halfPi - a2[1]) - ((b = b.x)[0] < 0 ? b[1] - halfPi - epsilon : halfPi - b[1]);
}
const clipAntimeridian = clip(
  function() {
    return true;
  },
  clipAntimeridianLine,
  clipAntimeridianInterpolate,
  [-pi$2, -halfPi]
);
function clipAntimeridianLine(stream) {
  var lambda0 = NaN, phi0 = NaN, sign0 = NaN, clean;
  return {
    lineStart: function() {
      stream.lineStart();
      clean = 1;
    },
    point: function(lambda1, phi1) {
      var sign1 = lambda1 > 0 ? pi$2 : -pi$2, delta = abs(lambda1 - lambda0);
      if (abs(delta - pi$2) < epsilon) {
        stream.point(lambda0, phi0 = (phi0 + phi1) / 2 > 0 ? halfPi : -halfPi);
        stream.point(sign0, phi0);
        stream.lineEnd();
        stream.lineStart();
        stream.point(sign1, phi0);
        stream.point(lambda1, phi0);
        clean = 0;
      } else if (sign0 !== sign1 && delta >= pi$2) {
        if (abs(lambda0 - sign0) < epsilon)
          lambda0 -= sign0 * epsilon;
        if (abs(lambda1 - sign1) < epsilon)
          lambda1 -= sign1 * epsilon;
        phi0 = clipAntimeridianIntersect(lambda0, phi0, lambda1, phi1);
        stream.point(sign0, phi0);
        stream.lineEnd();
        stream.lineStart();
        stream.point(sign1, phi0);
        clean = 0;
      }
      stream.point(lambda0 = lambda1, phi0 = phi1);
      sign0 = sign1;
    },
    lineEnd: function() {
      stream.lineEnd();
      lambda0 = phi0 = NaN;
    },
    clean: function() {
      return 2 - clean;
    }
  };
}
function clipAntimeridianIntersect(lambda0, phi0, lambda1, phi1) {
  var cosPhi0, cosPhi1, sinLambda0Lambda1 = sin$1(lambda0 - lambda1);
  return abs(sinLambda0Lambda1) > epsilon ? atan((sin$1(phi0) * (cosPhi1 = cos$1(phi1)) * sin$1(lambda1) - sin$1(phi1) * (cosPhi0 = cos$1(phi0)) * sin$1(lambda0)) / (cosPhi0 * cosPhi1 * sinLambda0Lambda1)) : (phi0 + phi1) / 2;
}
function clipAntimeridianInterpolate(from, to, direction, stream) {
  var phi;
  if (from == null) {
    phi = direction * halfPi;
    stream.point(-pi$2, phi);
    stream.point(0, phi);
    stream.point(pi$2, phi);
    stream.point(pi$2, 0);
    stream.point(pi$2, -phi);
    stream.point(0, -phi);
    stream.point(-pi$2, -phi);
    stream.point(-pi$2, 0);
    stream.point(-pi$2, phi);
  } else if (abs(from[0] - to[0]) > epsilon) {
    var lambda = from[0] < to[0] ? pi$2 : -pi$2;
    phi = direction * lambda / 2;
    stream.point(-lambda, phi);
    stream.point(0, phi);
    stream.point(lambda, phi);
  } else {
    stream.point(to[0], to[1]);
  }
}
function clipCircle(radius2) {
  var cr = cos$1(radius2), delta = 6 * radians$1, smallRadius = cr > 0, notHemisphere = abs(cr) > epsilon;
  function interpolate2(from, to, direction, stream) {
    circleStream(stream, radius2, delta, direction, from, to);
  }
  function visible(lambda, phi) {
    return cos$1(lambda) * cos$1(phi) > cr;
  }
  function clipLine2(stream) {
    var point0, c0, v0, v00, clean;
    return {
      lineStart: function() {
        v00 = v0 = false;
        clean = 1;
      },
      point: function(lambda, phi) {
        var point1 = [lambda, phi], point2, v = visible(lambda, phi), c2 = smallRadius ? v ? 0 : code(lambda, phi) : v ? code(lambda + (lambda < 0 ? pi$2 : -pi$2), phi) : 0;
        if (!point0 && (v00 = v0 = v))
          stream.lineStart();
        if (v !== v0) {
          point2 = intersect(point0, point1);
          if (!point2 || pointEqual(point0, point2) || pointEqual(point1, point2))
            point1[2] = 1;
        }
        if (v !== v0) {
          clean = 0;
          if (v) {
            stream.lineStart();
            point2 = intersect(point1, point0);
            stream.point(point2[0], point2[1]);
          } else {
            point2 = intersect(point0, point1);
            stream.point(point2[0], point2[1], 2);
            stream.lineEnd();
          }
          point0 = point2;
        } else if (notHemisphere && point0 && smallRadius ^ v) {
          var t;
          if (!(c2 & c0) && (t = intersect(point1, point0, true))) {
            clean = 0;
            if (smallRadius) {
              stream.lineStart();
              stream.point(t[0][0], t[0][1]);
              stream.point(t[1][0], t[1][1]);
              stream.lineEnd();
            } else {
              stream.point(t[1][0], t[1][1]);
              stream.lineEnd();
              stream.lineStart();
              stream.point(t[0][0], t[0][1], 3);
            }
          }
        }
        if (v && (!point0 || !pointEqual(point0, point1))) {
          stream.point(point1[0], point1[1]);
        }
        point0 = point1, v0 = v, c0 = c2;
      },
      lineEnd: function() {
        if (v0)
          stream.lineEnd();
        point0 = null;
      },
      // Rejoin first and last segments if there were intersections and the first
      // and last points were visible.
      clean: function() {
        return clean | (v00 && v0) << 1;
      }
    };
  }
  function intersect(a2, b, two) {
    var pa = cartesian(a2), pb = cartesian(b);
    var n1 = [1, 0, 0], n2 = cartesianCross(pa, pb), n2n2 = cartesianDot(n2, n2), n1n2 = n2[0], determinant = n2n2 - n1n2 * n1n2;
    if (!determinant)
      return !two && a2;
    var c1 = cr * n2n2 / determinant, c2 = -cr * n1n2 / determinant, n1xn2 = cartesianCross(n1, n2), A5 = cartesianScale(n1, c1), B2 = cartesianScale(n2, c2);
    cartesianAddInPlace(A5, B2);
    var u = n1xn2, w = cartesianDot(A5, u), uu = cartesianDot(u, u), t22 = w * w - uu * (cartesianDot(A5, A5) - 1);
    if (t22 < 0)
      return;
    var t = sqrt$1(t22), q = cartesianScale(u, (-w - t) / uu);
    cartesianAddInPlace(q, A5);
    q = spherical(q);
    if (!two)
      return q;
    var lambda0 = a2[0], lambda1 = b[0], phi0 = a2[1], phi1 = b[1], z;
    if (lambda1 < lambda0)
      z = lambda0, lambda0 = lambda1, lambda1 = z;
    var delta2 = lambda1 - lambda0, polar = abs(delta2 - pi$2) < epsilon, meridian = polar || delta2 < epsilon;
    if (!polar && phi1 < phi0)
      z = phi0, phi0 = phi1, phi1 = z;
    if (meridian ? polar ? phi0 + phi1 > 0 ^ q[1] < (abs(q[0] - lambda0) < epsilon ? phi0 : phi1) : phi0 <= q[1] && q[1] <= phi1 : delta2 > pi$2 ^ (lambda0 <= q[0] && q[0] <= lambda1)) {
      var q1 = cartesianScale(u, (-w + t) / uu);
      cartesianAddInPlace(q1, A5);
      return [q, spherical(q1)];
    }
  }
  function code(lambda, phi) {
    var r = smallRadius ? radius2 : pi$2 - radius2, code2 = 0;
    if (lambda < -r)
      code2 |= 1;
    else if (lambda > r)
      code2 |= 2;
    if (phi < -r)
      code2 |= 4;
    else if (phi > r)
      code2 |= 8;
    return code2;
  }
  return clip(visible, clipLine2, interpolate2, smallRadius ? [0, -radius2] : [-pi$2, radius2 - pi$2]);
}
function clipLine(a2, b, x02, y02, x12, y12) {
  var ax = a2[0], ay = a2[1], bx = b[0], by = b[1], t02 = 0, t12 = 1, dx = bx - ax, dy = by - ay, r;
  r = x02 - ax;
  if (!dx && r > 0)
    return;
  r /= dx;
  if (dx < 0) {
    if (r < t02)
      return;
    if (r < t12)
      t12 = r;
  } else if (dx > 0) {
    if (r > t12)
      return;
    if (r > t02)
      t02 = r;
  }
  r = x12 - ax;
  if (!dx && r < 0)
    return;
  r /= dx;
  if (dx < 0) {
    if (r > t12)
      return;
    if (r > t02)
      t02 = r;
  } else if (dx > 0) {
    if (r < t02)
      return;
    if (r < t12)
      t12 = r;
  }
  r = y02 - ay;
  if (!dy && r > 0)
    return;
  r /= dy;
  if (dy < 0) {
    if (r < t02)
      return;
    if (r < t12)
      t12 = r;
  } else if (dy > 0) {
    if (r > t12)
      return;
    if (r > t02)
      t02 = r;
  }
  r = y12 - ay;
  if (!dy && r < 0)
    return;
  r /= dy;
  if (dy < 0) {
    if (r > t12)
      return;
    if (r > t02)
      t02 = r;
  } else if (dy > 0) {
    if (r < t02)
      return;
    if (r < t12)
      t12 = r;
  }
  if (t02 > 0)
    a2[0] = ax + t02 * dx, a2[1] = ay + t02 * dy;
  if (t12 < 1)
    b[0] = ax + t12 * dx, b[1] = ay + t12 * dy;
  return true;
}
var clipMax = 1e9, clipMin = -clipMax;
function clipRectangle(x02, y02, x12, y12) {
  function visible(x, y) {
    return x02 <= x && x <= x12 && y02 <= y && y <= y12;
  }
  function interpolate2(from, to, direction, stream) {
    var a2 = 0, a1 = 0;
    if (from == null || (a2 = corner(from, direction)) !== (a1 = corner(to, direction)) || comparePoint(from, to) < 0 ^ direction > 0) {
      do
        stream.point(a2 === 0 || a2 === 3 ? x02 : x12, a2 > 1 ? y12 : y02);
      while ((a2 = (a2 + direction + 4) % 4) !== a1);
    } else {
      stream.point(to[0], to[1]);
    }
  }
  function corner(p, direction) {
    return abs(p[0] - x02) < epsilon ? direction > 0 ? 0 : 3 : abs(p[0] - x12) < epsilon ? direction > 0 ? 2 : 1 : abs(p[1] - y02) < epsilon ? direction > 0 ? 1 : 0 : direction > 0 ? 3 : 2;
  }
  function compareIntersection2(a2, b) {
    return comparePoint(a2.x, b.x);
  }
  function comparePoint(a2, b) {
    var ca = corner(a2, 1), cb = corner(b, 1);
    return ca !== cb ? ca - cb : ca === 0 ? b[1] - a2[1] : ca === 1 ? a2[0] - b[0] : ca === 2 ? a2[1] - b[1] : b[0] - a2[0];
  }
  return function(stream) {
    var activeStream = stream, bufferStream = clipBuffer(), segments, polygon, ring, x__, y__, v__, x_, y_, v_, first2, clean;
    var clipStream = {
      point: point2,
      lineStart,
      lineEnd,
      polygonStart,
      polygonEnd
    };
    function point2(x, y) {
      if (visible(x, y))
        activeStream.point(x, y);
    }
    function polygonInside() {
      var winding = 0;
      for (var i = 0, n = polygon.length; i < n; ++i) {
        for (var ring2 = polygon[i], j = 1, m = ring2.length, point3 = ring2[0], a0, a1, b0 = point3[0], b1 = point3[1]; j < m; ++j) {
          a0 = b0, a1 = b1, point3 = ring2[j], b0 = point3[0], b1 = point3[1];
          if (a1 <= y12) {
            if (b1 > y12 && (b0 - a0) * (y12 - a1) > (b1 - a1) * (x02 - a0))
              ++winding;
          } else {
            if (b1 <= y12 && (b0 - a0) * (y12 - a1) < (b1 - a1) * (x02 - a0))
              --winding;
          }
        }
      }
      return winding;
    }
    function polygonStart() {
      activeStream = bufferStream, segments = [], polygon = [], clean = true;
    }
    function polygonEnd() {
      var startInside = polygonInside(), cleanInside = clean && startInside, visible2 = (segments = merge(segments)).length;
      if (cleanInside || visible2) {
        stream.polygonStart();
        if (cleanInside) {
          stream.lineStart();
          interpolate2(null, null, 1, stream);
          stream.lineEnd();
        }
        if (visible2) {
          clipRejoin(segments, compareIntersection2, startInside, interpolate2, stream);
        }
        stream.polygonEnd();
      }
      activeStream = stream, segments = polygon = ring = null;
    }
    function lineStart() {
      clipStream.point = linePoint;
      if (polygon)
        polygon.push(ring = []);
      first2 = true;
      v_ = false;
      x_ = y_ = NaN;
    }
    function lineEnd() {
      if (segments) {
        linePoint(x__, y__);
        if (v__ && v_)
          bufferStream.rejoin();
        segments.push(bufferStream.result());
      }
      clipStream.point = point2;
      if (v_)
        activeStream.lineEnd();
    }
    function linePoint(x, y) {
      var v = visible(x, y);
      if (polygon)
        ring.push([x, y]);
      if (first2) {
        x__ = x, y__ = y, v__ = v;
        first2 = false;
        if (v) {
          activeStream.lineStart();
          activeStream.point(x, y);
        }
      } else {
        if (v && v_)
          activeStream.point(x, y);
        else {
          var a2 = [x_ = Math.max(clipMin, Math.min(clipMax, x_)), y_ = Math.max(clipMin, Math.min(clipMax, y_))], b = [x = Math.max(clipMin, Math.min(clipMax, x)), y = Math.max(clipMin, Math.min(clipMax, y))];
          if (clipLine(a2, b, x02, y02, x12, y12)) {
            if (!v_) {
              activeStream.lineStart();
              activeStream.point(a2[0], a2[1]);
            }
            activeStream.point(b[0], b[1]);
            if (!v)
              activeStream.lineEnd();
            clean = false;
          } else if (v) {
            activeStream.lineStart();
            activeStream.point(x, y);
            clean = false;
          }
        }
      }
      x_ = x, y_ = y, v_ = v;
    }
    return clipStream;
  };
}
const identity$4 = (x) => x;
var areaSum = new Adder(), areaRingSum = new Adder(), x00$2, y00$2, x0$3, y0$3;
var areaStream = {
  point: noop,
  lineStart: noop,
  lineEnd: noop,
  polygonStart: function() {
    areaStream.lineStart = areaRingStart;
    areaStream.lineEnd = areaRingEnd;
  },
  polygonEnd: function() {
    areaStream.lineStart = areaStream.lineEnd = areaStream.point = noop;
    areaSum.add(abs(areaRingSum));
    areaRingSum = new Adder();
  },
  result: function() {
    var area = areaSum / 2;
    areaSum = new Adder();
    return area;
  }
};
function areaRingStart() {
  areaStream.point = areaPointFirst;
}
function areaPointFirst(x, y) {
  areaStream.point = areaPoint;
  x00$2 = x0$3 = x, y00$2 = y0$3 = y;
}
function areaPoint(x, y) {
  areaRingSum.add(y0$3 * x - x0$3 * y);
  x0$3 = x, y0$3 = y;
}
function areaRingEnd() {
  areaPoint(x00$2, y00$2);
}
const pathArea = areaStream;
var x0$2 = Infinity, y0$2 = x0$2, x1 = -x0$2, y1 = x1;
var boundsStream = {
  point: boundsPoint,
  lineStart: noop,
  lineEnd: noop,
  polygonStart: noop,
  polygonEnd: noop,
  result: function() {
    var bounds = [[x0$2, y0$2], [x1, y1]];
    x1 = y1 = -(y0$2 = x0$2 = Infinity);
    return bounds;
  }
};
function boundsPoint(x, y) {
  if (x < x0$2)
    x0$2 = x;
  if (x > x1)
    x1 = x;
  if (y < y0$2)
    y0$2 = y;
  if (y > y1)
    y1 = y;
}
const boundsStream$1 = boundsStream;
var X0 = 0, Y0 = 0, Z0 = 0, X1 = 0, Y1 = 0, Z1 = 0, X2 = 0, Y2 = 0, Z2 = 0, x00$1, y00$1, x0$1, y0$1;
var centroidStream = {
  point: centroidPoint,
  lineStart: centroidLineStart,
  lineEnd: centroidLineEnd,
  polygonStart: function() {
    centroidStream.lineStart = centroidRingStart;
    centroidStream.lineEnd = centroidRingEnd;
  },
  polygonEnd: function() {
    centroidStream.point = centroidPoint;
    centroidStream.lineStart = centroidLineStart;
    centroidStream.lineEnd = centroidLineEnd;
  },
  result: function() {
    var centroid = Z2 ? [X2 / Z2, Y2 / Z2] : Z1 ? [X1 / Z1, Y1 / Z1] : Z0 ? [X0 / Z0, Y0 / Z0] : [NaN, NaN];
    X0 = Y0 = Z0 = X1 = Y1 = Z1 = X2 = Y2 = Z2 = 0;
    return centroid;
  }
};
function centroidPoint(x, y) {
  X0 += x;
  Y0 += y;
  ++Z0;
}
function centroidLineStart() {
  centroidStream.point = centroidPointFirstLine;
}
function centroidPointFirstLine(x, y) {
  centroidStream.point = centroidPointLine;
  centroidPoint(x0$1 = x, y0$1 = y);
}
function centroidPointLine(x, y) {
  var dx = x - x0$1, dy = y - y0$1, z = sqrt$1(dx * dx + dy * dy);
  X1 += z * (x0$1 + x) / 2;
  Y1 += z * (y0$1 + y) / 2;
  Z1 += z;
  centroidPoint(x0$1 = x, y0$1 = y);
}
function centroidLineEnd() {
  centroidStream.point = centroidPoint;
}
function centroidRingStart() {
  centroidStream.point = centroidPointFirstRing;
}
function centroidRingEnd() {
  centroidPointRing(x00$1, y00$1);
}
function centroidPointFirstRing(x, y) {
  centroidStream.point = centroidPointRing;
  centroidPoint(x00$1 = x0$1 = x, y00$1 = y0$1 = y);
}
function centroidPointRing(x, y) {
  var dx = x - x0$1, dy = y - y0$1, z = sqrt$1(dx * dx + dy * dy);
  X1 += z * (x0$1 + x) / 2;
  Y1 += z * (y0$1 + y) / 2;
  Z1 += z;
  z = y0$1 * x - x0$1 * y;
  X2 += z * (x0$1 + x);
  Y2 += z * (y0$1 + y);
  Z2 += z * 3;
  centroidPoint(x0$1 = x, y0$1 = y);
}
const pathCentroid = centroidStream;
function PathContext(context) {
  this._context = context;
}
PathContext.prototype = {
  _radius: 4.5,
  pointRadius: function(_) {
    return this._radius = _, this;
  },
  polygonStart: function() {
    this._line = 0;
  },
  polygonEnd: function() {
    this._line = NaN;
  },
  lineStart: function() {
    this._point = 0;
  },
  lineEnd: function() {
    if (this._line === 0)
      this._context.closePath();
    this._point = NaN;
  },
  point: function(x, y) {
    switch (this._point) {
      case 0: {
        this._context.moveTo(x, y);
        this._point = 1;
        break;
      }
      case 1: {
        this._context.lineTo(x, y);
        break;
      }
      default: {
        this._context.moveTo(x + this._radius, y);
        this._context.arc(x, y, this._radius, 0, tau$2);
        break;
      }
    }
  },
  result: noop
};
var lengthSum = new Adder(), lengthRing, x00, y00, x0, y0;
var lengthStream = {
  point: noop,
  lineStart: function() {
    lengthStream.point = lengthPointFirst;
  },
  lineEnd: function() {
    if (lengthRing)
      lengthPoint(x00, y00);
    lengthStream.point = noop;
  },
  polygonStart: function() {
    lengthRing = true;
  },
  polygonEnd: function() {
    lengthRing = null;
  },
  result: function() {
    var length2 = +lengthSum;
    lengthSum = new Adder();
    return length2;
  }
};
function lengthPointFirst(x, y) {
  lengthStream.point = lengthPoint;
  x00 = x0 = x, y00 = y0 = y;
}
function lengthPoint(x, y) {
  x0 -= x, y0 -= y;
  lengthSum.add(sqrt$1(x0 * x0 + y0 * y0));
  x0 = x, y0 = y;
}
const pathMeasure = lengthStream;
let cacheDigits, cacheAppend, cacheRadius, cacheCircle;
class PathString {
  constructor(digits) {
    this._append = digits == null ? append : appendRound(digits);
    this._radius = 4.5;
    this._ = "";
  }
  pointRadius(_) {
    this._radius = +_;
    return this;
  }
  polygonStart() {
    this._line = 0;
  }
  polygonEnd() {
    this._line = NaN;
  }
  lineStart() {
    this._point = 0;
  }
  lineEnd() {
    if (this._line === 0)
      this._ += "Z";
    this._point = NaN;
  }
  point(x, y) {
    switch (this._point) {
      case 0: {
        this._append`M${x},${y}`;
        this._point = 1;
        break;
      }
      case 1: {
        this._append`L${x},${y}`;
        break;
      }
      default: {
        this._append`M${x},${y}`;
        if (this._radius !== cacheRadius || this._append !== cacheAppend) {
          const r = this._radius;
          const s2 = this._;
          this._ = "";
          this._append`m0,${r}a${r},${r} 0 1,1 0,${-2 * r}a${r},${r} 0 1,1 0,${2 * r}z`;
          cacheRadius = r;
          cacheAppend = this._append;
          cacheCircle = this._;
          this._ = s2;
        }
        this._ += cacheCircle;
        break;
      }
    }
  }
  result() {
    const result = this._;
    this._ = "";
    return result.length ? result : null;
  }
}
function append(strings) {
  let i = 1;
  this._ += strings[0];
  for (const j = strings.length; i < j; ++i) {
    this._ += arguments[i] + strings[i];
  }
}
function appendRound(digits) {
  const d = Math.floor(digits);
  if (!(d >= 0))
    throw new RangeError(`invalid digits: ${digits}`);
  if (d > 15)
    return append;
  if (d !== cacheDigits) {
    const k2 = 10 ** d;
    cacheDigits = d;
    cacheAppend = function append2(strings) {
      let i = 1;
      this._ += strings[0];
      for (const j = strings.length; i < j; ++i) {
        this._ += Math.round(arguments[i] * k2) / k2 + strings[i];
      }
    };
  }
  return cacheAppend;
}
function geoPath(projection2, context) {
  let digits = 3, pointRadius = 4.5, projectionStream, contextStream;
  function path(object2) {
    if (object2) {
      if (typeof pointRadius === "function")
        contextStream.pointRadius(+pointRadius.apply(this, arguments));
      geoStream(object2, projectionStream(contextStream));
    }
    return contextStream.result();
  }
  path.area = function(object2) {
    geoStream(object2, projectionStream(pathArea));
    return pathArea.result();
  };
  path.measure = function(object2) {
    geoStream(object2, projectionStream(pathMeasure));
    return pathMeasure.result();
  };
  path.bounds = function(object2) {
    geoStream(object2, projectionStream(boundsStream$1));
    return boundsStream$1.result();
  };
  path.centroid = function(object2) {
    geoStream(object2, projectionStream(pathCentroid));
    return pathCentroid.result();
  };
  path.projection = function(_) {
    if (!arguments.length)
      return projection2;
    projectionStream = _ == null ? (projection2 = null, identity$4) : (projection2 = _).stream;
    return path;
  };
  path.context = function(_) {
    if (!arguments.length)
      return context;
    contextStream = _ == null ? (context = null, new PathString(digits)) : new PathContext(context = _);
    if (typeof pointRadius !== "function")
      contextStream.pointRadius(pointRadius);
    return path;
  };
  path.pointRadius = function(_) {
    if (!arguments.length)
      return pointRadius;
    pointRadius = typeof _ === "function" ? _ : (contextStream.pointRadius(+_), +_);
    return path;
  };
  path.digits = function(_) {
    if (!arguments.length)
      return digits;
    if (_ == null)
      digits = null;
    else {
      const d = Math.floor(_);
      if (!(d >= 0))
        throw new RangeError(`invalid digits: ${_}`);
      digits = d;
    }
    if (context === null)
      contextStream = new PathString(digits);
    return path;
  };
  return path.projection(projection2).digits(digits).context(context);
}
function geoTransform(methods) {
  return {
    stream: transformer$2(methods)
  };
}
function transformer$2(methods) {
  return function(stream) {
    var s2 = new TransformStream();
    for (var key in methods)
      s2[key] = methods[key];
    s2.stream = stream;
    return s2;
  };
}
function TransformStream() {
}
TransformStream.prototype = {
  constructor: TransformStream,
  point: function(x, y) {
    this.stream.point(x, y);
  },
  sphere: function() {
    this.stream.sphere();
  },
  lineStart: function() {
    this.stream.lineStart();
  },
  lineEnd: function() {
    this.stream.lineEnd();
  },
  polygonStart: function() {
    this.stream.polygonStart();
  },
  polygonEnd: function() {
    this.stream.polygonEnd();
  }
};
function fit(projection2, fitBounds, object2) {
  var clip2 = projection2.clipExtent && projection2.clipExtent();
  projection2.scale(150).translate([0, 0]);
  if (clip2 != null)
    projection2.clipExtent(null);
  geoStream(object2, projection2.stream(boundsStream$1));
  fitBounds(boundsStream$1.result());
  if (clip2 != null)
    projection2.clipExtent(clip2);
  return projection2;
}
function fitExtent(projection2, extent2, object2) {
  return fit(projection2, function(b) {
    var w = extent2[1][0] - extent2[0][0], h = extent2[1][1] - extent2[0][1], k2 = Math.min(w / (b[1][0] - b[0][0]), h / (b[1][1] - b[0][1])), x = +extent2[0][0] + (w - k2 * (b[1][0] + b[0][0])) / 2, y = +extent2[0][1] + (h - k2 * (b[1][1] + b[0][1])) / 2;
    projection2.scale(150 * k2).translate([x, y]);
  }, object2);
}
function fitSize(projection2, size, object2) {
  return fitExtent(projection2, [[0, 0], size], object2);
}
function fitWidth(projection2, width, object2) {
  return fit(projection2, function(b) {
    var w = +width, k2 = w / (b[1][0] - b[0][0]), x = (w - k2 * (b[1][0] + b[0][0])) / 2, y = -k2 * b[0][1];
    projection2.scale(150 * k2).translate([x, y]);
  }, object2);
}
function fitHeight(projection2, height, object2) {
  return fit(projection2, function(b) {
    var h = +height, k2 = h / (b[1][1] - b[0][1]), x = -k2 * b[0][0], y = (h - k2 * (b[1][1] + b[0][1])) / 2;
    projection2.scale(150 * k2).translate([x, y]);
  }, object2);
}
var maxDepth = 16, cosMinDistance = cos$1(30 * radians$1);
function resample(project2, delta2) {
  return +delta2 ? resample$1(project2, delta2) : resampleNone(project2);
}
function resampleNone(project2) {
  return transformer$2({
    point: function(x, y) {
      x = project2(x, y);
      this.stream.point(x[0], x[1]);
    }
  });
}
function resample$1(project2, delta2) {
  function resampleLineTo(x02, y02, lambda0, a0, b0, c0, x12, y12, lambda1, a1, b1, c1, depth, stream) {
    var dx = x12 - x02, dy = y12 - y02, d2 = dx * dx + dy * dy;
    if (d2 > 4 * delta2 && depth--) {
      var a2 = a0 + a1, b = b0 + b1, c2 = c0 + c1, m = sqrt$1(a2 * a2 + b * b + c2 * c2), phi2 = asin(c2 /= m), lambda2 = abs(abs(c2) - 1) < epsilon || abs(lambda0 - lambda1) < epsilon ? (lambda0 + lambda1) / 2 : atan2(b, a2), p = project2(lambda2, phi2), x2 = p[0], y2 = p[1], dx2 = x2 - x02, dy2 = y2 - y02, dz = dy * dx2 - dx * dy2;
      if (dz * dz / d2 > delta2 || abs((dx * dx2 + dy * dy2) / d2 - 0.5) > 0.3 || a0 * a1 + b0 * b1 + c0 * c1 < cosMinDistance) {
        resampleLineTo(x02, y02, lambda0, a0, b0, c0, x2, y2, lambda2, a2 /= m, b /= m, c2, depth, stream);
        stream.point(x2, y2);
        resampleLineTo(x2, y2, lambda2, a2, b, c2, x12, y12, lambda1, a1, b1, c1, depth, stream);
      }
    }
  }
  return function(stream) {
    var lambda00, x002, y002, a00, b00, c00, lambda0, x02, y02, a0, b0, c0;
    var resampleStream = {
      point: point2,
      lineStart,
      lineEnd,
      polygonStart: function() {
        stream.polygonStart();
        resampleStream.lineStart = ringStart;
      },
      polygonEnd: function() {
        stream.polygonEnd();
        resampleStream.lineStart = lineStart;
      }
    };
    function point2(x, y) {
      x = project2(x, y);
      stream.point(x[0], x[1]);
    }
    function lineStart() {
      x02 = NaN;
      resampleStream.point = linePoint;
      stream.lineStart();
    }
    function linePoint(lambda, phi) {
      var c2 = cartesian([lambda, phi]), p = project2(lambda, phi);
      resampleLineTo(x02, y02, lambda0, a0, b0, c0, x02 = p[0], y02 = p[1], lambda0 = lambda, a0 = c2[0], b0 = c2[1], c0 = c2[2], maxDepth, stream);
      stream.point(x02, y02);
    }
    function lineEnd() {
      resampleStream.point = point2;
      stream.lineEnd();
    }
    function ringStart() {
      lineStart();
      resampleStream.point = ringPoint;
      resampleStream.lineEnd = ringEnd;
    }
    function ringPoint(lambda, phi) {
      linePoint(lambda00 = lambda, phi), x002 = x02, y002 = y02, a00 = a0, b00 = b0, c00 = c0;
      resampleStream.point = linePoint;
    }
    function ringEnd() {
      resampleLineTo(x02, y02, lambda0, a0, b0, c0, x002, y002, lambda00, a00, b00, c00, maxDepth, stream);
      resampleStream.lineEnd = lineEnd;
      lineEnd();
    }
    return resampleStream;
  };
}
var transformRadians = transformer$2({
  point: function(x, y) {
    this.stream.point(x * radians$1, y * radians$1);
  }
});
function transformRotate(rotate) {
  return transformer$2({
    point: function(x, y) {
      var r = rotate(x, y);
      return this.stream.point(r[0], r[1]);
    }
  });
}
function scaleTranslate(k2, dx, dy, sx, sy) {
  function transform(x, y) {
    x *= sx;
    y *= sy;
    return [dx + k2 * x, dy - k2 * y];
  }
  transform.invert = function(x, y) {
    return [(x - dx) / k2 * sx, (dy - y) / k2 * sy];
  };
  return transform;
}
function scaleTranslateRotate(k2, dx, dy, sx, sy, alpha) {
  if (!alpha)
    return scaleTranslate(k2, dx, dy, sx, sy);
  var cosAlpha = cos$1(alpha), sinAlpha = sin$1(alpha), a2 = cosAlpha * k2, b = sinAlpha * k2, ai = cosAlpha / k2, bi = sinAlpha / k2, ci = (sinAlpha * dy - cosAlpha * dx) / k2, fi = (sinAlpha * dx + cosAlpha * dy) / k2;
  function transform(x, y) {
    x *= sx;
    y *= sy;
    return [a2 * x - b * y + dx, dy - b * x - a2 * y];
  }
  transform.invert = function(x, y) {
    return [sx * (ai * x - bi * y + ci), sy * (fi - bi * x - ai * y)];
  };
  return transform;
}
function projection(project2) {
  return projectionMutator(function() {
    return project2;
  })();
}
function projectionMutator(projectAt) {
  var project2, k2 = 150, x = 480, y = 250, lambda = 0, phi = 0, deltaLambda = 0, deltaPhi = 0, deltaGamma = 0, rotate, alpha = 0, sx = 1, sy = 1, theta = null, preclip = clipAntimeridian, x02 = null, y02, x12, y12, postclip = identity$4, delta2 = 0.5, projectResample, projectTransform, projectRotateTransform, cache, cacheStream;
  function projection2(point2) {
    return projectRotateTransform(point2[0] * radians$1, point2[1] * radians$1);
  }
  function invert(point2) {
    point2 = projectRotateTransform.invert(point2[0], point2[1]);
    return point2 && [point2[0] * degrees, point2[1] * degrees];
  }
  projection2.stream = function(stream) {
    return cache && cacheStream === stream ? cache : cache = transformRadians(transformRotate(rotate)(preclip(projectResample(postclip(cacheStream = stream)))));
  };
  projection2.preclip = function(_) {
    return arguments.length ? (preclip = _, theta = void 0, reset2()) : preclip;
  };
  projection2.postclip = function(_) {
    return arguments.length ? (postclip = _, x02 = y02 = x12 = y12 = null, reset2()) : postclip;
  };
  projection2.clipAngle = function(_) {
    return arguments.length ? (preclip = +_ ? clipCircle(theta = _ * radians$1) : (theta = null, clipAntimeridian), reset2()) : theta * degrees;
  };
  projection2.clipExtent = function(_) {
    return arguments.length ? (postclip = _ == null ? (x02 = y02 = x12 = y12 = null, identity$4) : clipRectangle(x02 = +_[0][0], y02 = +_[0][1], x12 = +_[1][0], y12 = +_[1][1]), reset2()) : x02 == null ? null : [[x02, y02], [x12, y12]];
  };
  projection2.scale = function(_) {
    return arguments.length ? (k2 = +_, recenter()) : k2;
  };
  projection2.translate = function(_) {
    return arguments.length ? (x = +_[0], y = +_[1], recenter()) : [x, y];
  };
  projection2.center = function(_) {
    return arguments.length ? (lambda = _[0] % 360 * radians$1, phi = _[1] % 360 * radians$1, recenter()) : [lambda * degrees, phi * degrees];
  };
  projection2.rotate = function(_) {
    return arguments.length ? (deltaLambda = _[0] % 360 * radians$1, deltaPhi = _[1] % 360 * radians$1, deltaGamma = _.length > 2 ? _[2] % 360 * radians$1 : 0, recenter()) : [deltaLambda * degrees, deltaPhi * degrees, deltaGamma * degrees];
  };
  projection2.angle = function(_) {
    return arguments.length ? (alpha = _ % 360 * radians$1, recenter()) : alpha * degrees;
  };
  projection2.reflectX = function(_) {
    return arguments.length ? (sx = _ ? -1 : 1, recenter()) : sx < 0;
  };
  projection2.reflectY = function(_) {
    return arguments.length ? (sy = _ ? -1 : 1, recenter()) : sy < 0;
  };
  projection2.precision = function(_) {
    return arguments.length ? (projectResample = resample(projectTransform, delta2 = _ * _), reset2()) : sqrt$1(delta2);
  };
  projection2.fitExtent = function(extent2, object2) {
    return fitExtent(projection2, extent2, object2);
  };
  projection2.fitSize = function(size, object2) {
    return fitSize(projection2, size, object2);
  };
  projection2.fitWidth = function(width, object2) {
    return fitWidth(projection2, width, object2);
  };
  projection2.fitHeight = function(height, object2) {
    return fitHeight(projection2, height, object2);
  };
  function recenter() {
    var center2 = scaleTranslateRotate(k2, 0, 0, sx, sy, alpha).apply(null, project2(lambda, phi)), transform = scaleTranslateRotate(k2, x - center2[0], y - center2[1], sx, sy, alpha);
    rotate = rotateRadians(deltaLambda, deltaPhi, deltaGamma);
    projectTransform = compose(project2, transform);
    projectRotateTransform = compose(rotate, projectTransform);
    projectResample = resample(projectTransform, delta2);
    return reset2();
  }
  function reset2() {
    cache = cacheStream = null;
    return projection2;
  }
  return function() {
    project2 = projectAt.apply(this, arguments);
    projection2.invert = project2.invert && invert;
    return recenter();
  };
}
function conicProjection$1(projectAt) {
  var phi0 = 0, phi1 = pi$2 / 3, m = projectionMutator(projectAt), p = m(phi0, phi1);
  p.parallels = function(_) {
    return arguments.length ? m(phi0 = _[0] * radians$1, phi1 = _[1] * radians$1) : [phi0 * degrees, phi1 * degrees];
  };
  return p;
}
function cylindricalEqualAreaRaw(phi0) {
  var cosPhi0 = cos$1(phi0);
  function forward(lambda, phi) {
    return [lambda * cosPhi0, sin$1(phi) / cosPhi0];
  }
  forward.invert = function(x, y) {
    return [x / cosPhi0, asin(y * cosPhi0)];
  };
  return forward;
}
function conicEqualAreaRaw(y02, y12) {
  var sy0 = sin$1(y02), n = (sy0 + sin$1(y12)) / 2;
  if (abs(n) < epsilon)
    return cylindricalEqualAreaRaw(y02);
  var c2 = 1 + sy0 * (2 * n - sy0), r0 = sqrt$1(c2) / n;
  function project2(x, y) {
    var r = sqrt$1(c2 - 2 * n * sin$1(y)) / n;
    return [r * sin$1(x *= n), r0 - r * cos$1(x)];
  }
  project2.invert = function(x, y) {
    var r0y = r0 - y, l = atan2(x, abs(r0y)) * sign(r0y);
    if (r0y * n < 0)
      l -= pi$2 * sign(x) * sign(r0y);
    return [l / n, asin((c2 - (x * x + r0y * r0y) * n * n) / (2 * n))];
  };
  return project2;
}
function geoConicEqualArea() {
  return conicProjection$1(conicEqualAreaRaw).scale(155.424).center([0, 33.6442]);
}
function geoAlbers() {
  return geoConicEqualArea().parallels([29.5, 45.5]).scale(1070).translate([480, 250]).rotate([96, 0]).center([-0.6, 38.7]);
}
function multiplex(streams) {
  var n = streams.length;
  return {
    point: function(x, y) {
      var i = -1;
      while (++i < n)
        streams[i].point(x, y);
    },
    sphere: function() {
      var i = -1;
      while (++i < n)
        streams[i].sphere();
    },
    lineStart: function() {
      var i = -1;
      while (++i < n)
        streams[i].lineStart();
    },
    lineEnd: function() {
      var i = -1;
      while (++i < n)
        streams[i].lineEnd();
    },
    polygonStart: function() {
      var i = -1;
      while (++i < n)
        streams[i].polygonStart();
    },
    polygonEnd: function() {
      var i = -1;
      while (++i < n)
        streams[i].polygonEnd();
    }
  };
}
function geoAlbersUsa() {
  var cache, cacheStream, lower48 = geoAlbers(), lower48Point, alaska = geoConicEqualArea().rotate([154, 0]).center([-2, 58.5]).parallels([55, 65]), alaskaPoint, hawaii = geoConicEqualArea().rotate([157, 0]).center([-3, 19.9]).parallels([8, 18]), hawaiiPoint, point2, pointStream = { point: function(x, y) {
    point2 = [x, y];
  } };
  function albersUsa(coordinates) {
    var x = coordinates[0], y = coordinates[1];
    return point2 = null, (lower48Point.point(x, y), point2) || (alaskaPoint.point(x, y), point2) || (hawaiiPoint.point(x, y), point2);
  }
  albersUsa.invert = function(coordinates) {
    var k2 = lower48.scale(), t = lower48.translate(), x = (coordinates[0] - t[0]) / k2, y = (coordinates[1] - t[1]) / k2;
    return (y >= 0.12 && y < 0.234 && x >= -0.425 && x < -0.214 ? alaska : y >= 0.166 && y < 0.234 && x >= -0.214 && x < -0.115 ? hawaii : lower48).invert(coordinates);
  };
  albersUsa.stream = function(stream) {
    return cache && cacheStream === stream ? cache : cache = multiplex([lower48.stream(cacheStream = stream), alaska.stream(stream), hawaii.stream(stream)]);
  };
  albersUsa.precision = function(_) {
    if (!arguments.length)
      return lower48.precision();
    lower48.precision(_), alaska.precision(_), hawaii.precision(_);
    return reset2();
  };
  albersUsa.scale = function(_) {
    if (!arguments.length)
      return lower48.scale();
    lower48.scale(_), alaska.scale(_ * 0.35), hawaii.scale(_);
    return albersUsa.translate(lower48.translate());
  };
  albersUsa.translate = function(_) {
    if (!arguments.length)
      return lower48.translate();
    var k2 = lower48.scale(), x = +_[0], y = +_[1];
    lower48Point = lower48.translate(_).clipExtent([[x - 0.455 * k2, y - 0.238 * k2], [x + 0.455 * k2, y + 0.238 * k2]]).stream(pointStream);
    alaskaPoint = alaska.translate([x - 0.307 * k2, y + 0.201 * k2]).clipExtent([[x - 0.425 * k2 + epsilon, y + 0.12 * k2 + epsilon], [x - 0.214 * k2 - epsilon, y + 0.234 * k2 - epsilon]]).stream(pointStream);
    hawaiiPoint = hawaii.translate([x - 0.205 * k2, y + 0.212 * k2]).clipExtent([[x - 0.214 * k2 + epsilon, y + 0.166 * k2 + epsilon], [x - 0.115 * k2 - epsilon, y + 0.234 * k2 - epsilon]]).stream(pointStream);
    return reset2();
  };
  albersUsa.fitExtent = function(extent2, object2) {
    return fitExtent(albersUsa, extent2, object2);
  };
  albersUsa.fitSize = function(size, object2) {
    return fitSize(albersUsa, size, object2);
  };
  albersUsa.fitWidth = function(width, object2) {
    return fitWidth(albersUsa, width, object2);
  };
  albersUsa.fitHeight = function(height, object2) {
    return fitHeight(albersUsa, height, object2);
  };
  function reset2() {
    cache = cacheStream = null;
    return albersUsa;
  }
  return albersUsa.scale(1070);
}
function azimuthalRaw(scale) {
  return function(x, y) {
    var cx = cos$1(x), cy = cos$1(y), k2 = scale(cx * cy);
    if (k2 === Infinity)
      return [2, 0];
    return [
      k2 * cy * sin$1(x),
      k2 * sin$1(y)
    ];
  };
}
function azimuthalInvert(angle) {
  return function(x, y) {
    var z = sqrt$1(x * x + y * y), c2 = angle(z), sc = sin$1(c2), cc = cos$1(c2);
    return [
      atan2(x * sc, z * cc),
      asin(z && y * sc / z)
    ];
  };
}
var azimuthalEqualAreaRaw = azimuthalRaw(function(cxcy) {
  return sqrt$1(2 / (1 + cxcy));
});
azimuthalEqualAreaRaw.invert = azimuthalInvert(function(z) {
  return 2 * asin(z / 2);
});
function geoAzimuthalEqualArea() {
  return projection(azimuthalEqualAreaRaw).scale(124.75).clipAngle(180 - 1e-3);
}
var azimuthalEquidistantRaw = azimuthalRaw(function(c2) {
  return (c2 = acos(c2)) && c2 / sin$1(c2);
});
azimuthalEquidistantRaw.invert = azimuthalInvert(function(z) {
  return z;
});
function geoAzimuthalEquidistant() {
  return projection(azimuthalEquidistantRaw).scale(79.4188).clipAngle(180 - 1e-3);
}
function mercatorRaw(lambda, phi) {
  return [lambda, log$1(tan((halfPi + phi) / 2))];
}
mercatorRaw.invert = function(x, y) {
  return [x, 2 * atan(exp(y)) - halfPi];
};
function geoMercator() {
  return mercatorProjection(mercatorRaw).scale(961 / tau$2);
}
function mercatorProjection(project2) {
  var m = projection(project2), center2 = m.center, scale = m.scale, translate = m.translate, clipExtent = m.clipExtent, x02 = null, y02, x12, y12;
  m.scale = function(_) {
    return arguments.length ? (scale(_), reclip()) : scale();
  };
  m.translate = function(_) {
    return arguments.length ? (translate(_), reclip()) : translate();
  };
  m.center = function(_) {
    return arguments.length ? (center2(_), reclip()) : center2();
  };
  m.clipExtent = function(_) {
    return arguments.length ? (_ == null ? x02 = y02 = x12 = y12 = null : (x02 = +_[0][0], y02 = +_[0][1], x12 = +_[1][0], y12 = +_[1][1]), reclip()) : x02 == null ? null : [[x02, y02], [x12, y12]];
  };
  function reclip() {
    var k2 = pi$2 * scale(), t = m(rotation(m.rotate()).invert([0, 0]));
    return clipExtent(x02 == null ? [[t[0] - k2, t[1] - k2], [t[0] + k2, t[1] + k2]] : project2 === mercatorRaw ? [[Math.max(t[0] - k2, x02), y02], [Math.min(t[0] + k2, x12), y12]] : [[x02, Math.max(t[1] - k2, y02)], [x12, Math.min(t[1] + k2, y12)]]);
  }
  return reclip();
}
function tany(y) {
  return tan((halfPi + y) / 2);
}
function conicConformalRaw(y02, y12) {
  var cy0 = cos$1(y02), n = y02 === y12 ? sin$1(y02) : log$1(cy0 / cos$1(y12)) / log$1(tany(y12) / tany(y02)), f = cy0 * pow$1(tany(y02), n) / n;
  if (!n)
    return mercatorRaw;
  function project2(x, y) {
    if (f > 0) {
      if (y < -halfPi + epsilon)
        y = -halfPi + epsilon;
    } else {
      if (y > halfPi - epsilon)
        y = halfPi - epsilon;
    }
    var r = f / pow$1(tany(y), n);
    return [r * sin$1(n * x), f - r * cos$1(n * x)];
  }
  project2.invert = function(x, y) {
    var fy = f - y, r = sign(n) * sqrt$1(x * x + fy * fy), l = atan2(x, abs(fy)) * sign(fy);
    if (fy * n < 0)
      l -= pi$2 * sign(x) * sign(fy);
    return [l / n, 2 * atan(pow$1(f / r, 1 / n)) - halfPi];
  };
  return project2;
}
function geoConicConformal() {
  return conicProjection$1(conicConformalRaw).scale(109.5).parallels([30, 30]);
}
function equirectangularRaw(lambda, phi) {
  return [lambda, phi];
}
equirectangularRaw.invert = equirectangularRaw;
function geoEquirectangular() {
  return projection(equirectangularRaw).scale(152.63);
}
function conicEquidistantRaw(y02, y12) {
  var cy0 = cos$1(y02), n = y02 === y12 ? sin$1(y02) : (cy0 - cos$1(y12)) / (y12 - y02), g = cy0 / n + y02;
  if (abs(n) < epsilon)
    return equirectangularRaw;
  function project2(x, y) {
    var gy = g - y, nx = n * x;
    return [gy * sin$1(nx), g - gy * cos$1(nx)];
  }
  project2.invert = function(x, y) {
    var gy = g - y, l = atan2(x, abs(gy)) * sign(gy);
    if (gy * n < 0)
      l -= pi$2 * sign(x) * sign(gy);
    return [l / n, g - sign(n) * sqrt$1(x * x + gy * gy)];
  };
  return project2;
}
function geoConicEquidistant() {
  return conicProjection$1(conicEquidistantRaw).scale(131.154).center([0, 13.9389]);
}
var A1 = 1.340264, A2 = -0.081106, A3 = 893e-6, A4 = 3796e-6, M = sqrt$1(3) / 2, iterations = 12;
function equalEarthRaw(lambda, phi) {
  var l = asin(M * sin$1(phi)), l2 = l * l, l6 = l2 * l2 * l2;
  return [
    lambda * cos$1(l) / (M * (A1 + 3 * A2 * l2 + l6 * (7 * A3 + 9 * A4 * l2))),
    l * (A1 + A2 * l2 + l6 * (A3 + A4 * l2))
  ];
}
equalEarthRaw.invert = function(x, y) {
  var l = y, l2 = l * l, l6 = l2 * l2 * l2;
  for (var i = 0, delta, fy, fpy; i < iterations; ++i) {
    fy = l * (A1 + A2 * l2 + l6 * (A3 + A4 * l2)) - y;
    fpy = A1 + 3 * A2 * l2 + l6 * (7 * A3 + 9 * A4 * l2);
    l -= delta = fy / fpy, l2 = l * l, l6 = l2 * l2 * l2;
    if (abs(delta) < epsilon2)
      break;
  }
  return [
    M * x * (A1 + 3 * A2 * l2 + l6 * (7 * A3 + 9 * A4 * l2)) / cos$1(l),
    asin(sin$1(l) / M)
  ];
};
function geoEqualEarth() {
  return projection(equalEarthRaw).scale(177.158);
}
function gnomonicRaw(x, y) {
  var cy = cos$1(y), k2 = cos$1(x) * cy;
  return [cy * sin$1(x) / k2, sin$1(y) / k2];
}
gnomonicRaw.invert = azimuthalInvert(atan);
function geoGnomonic() {
  return projection(gnomonicRaw).scale(144.049).clipAngle(60);
}
function orthographicRaw(x, y) {
  return [cos$1(y) * sin$1(x), sin$1(y)];
}
orthographicRaw.invert = azimuthalInvert(asin);
function geoOrthographic() {
  return projection(orthographicRaw).scale(249.5).clipAngle(90 + epsilon);
}
function stereographicRaw(x, y) {
  var cy = cos$1(y), k2 = 1 + cos$1(x) * cy;
  return [cy * sin$1(x) / k2, sin$1(y) / k2];
}
stereographicRaw.invert = azimuthalInvert(function(z) {
  return 2 * atan(z);
});
function geoStereographic() {
  return projection(stereographicRaw).scale(250).clipAngle(142);
}
function transverseMercatorRaw(lambda, phi) {
  return [log$1(tan((halfPi + phi) / 2)), -lambda];
}
transverseMercatorRaw.invert = function(x, y) {
  return [-y, 2 * atan(exp(x)) - halfPi];
};
function geoTransverseMercator() {
  var m = mercatorProjection(transverseMercatorRaw), center2 = m.center, rotate = m.rotate;
  m.center = function(_) {
    return arguments.length ? center2([-_[1], _[0]]) : (_ = center2(), [_[1], -_[0]]);
  };
  m.rotate = function(_) {
    return arguments.length ? rotate([_[0], _[1], _.length > 2 ? _[2] + 90 : 90]) : (_ = rotate(), [_[0], _[1], _[2] - 90]);
  };
  return rotate([0, 0, 90]).scale(159.155);
}
function initRange(domain, range2) {
  switch (arguments.length) {
    case 0:
      break;
    case 1:
      this.range(domain);
      break;
    default:
      this.range(range2).domain(domain);
      break;
  }
  return this;
}
function initInterpolator(domain, interpolator) {
  switch (arguments.length) {
    case 0:
      break;
    case 1: {
      if (typeof domain === "function")
        this.interpolator(domain);
      else
        this.range(domain);
      break;
    }
    default: {
      this.domain(domain);
      if (typeof interpolator === "function")
        this.interpolator(interpolator);
      else
        this.range(interpolator);
      break;
    }
  }
  return this;
}
const implicit = Symbol("implicit");
function ordinal() {
  var index2 = new InternMap(), domain = [], range2 = [], unknown = implicit;
  function scale(d) {
    let i = index2.get(d);
    if (i === void 0) {
      if (unknown !== implicit)
        return unknown;
      index2.set(d, i = domain.push(d) - 1);
    }
    return range2[i % range2.length];
  }
  scale.domain = function(_) {
    if (!arguments.length)
      return domain.slice();
    domain = [], index2 = new InternMap();
    for (const value of _) {
      if (index2.has(value))
        continue;
      index2.set(value, domain.push(value) - 1);
    }
    return scale;
  };
  scale.range = function(_) {
    return arguments.length ? (range2 = Array.from(_), scale) : range2.slice();
  };
  scale.unknown = function(_) {
    return arguments.length ? (unknown = _, scale) : unknown;
  };
  scale.copy = function() {
    return ordinal(domain, range2).unknown(unknown);
  };
  initRange.apply(scale, arguments);
  return scale;
}
function band() {
  var scale = ordinal().unknown(void 0), domain = scale.domain, ordinalRange2 = scale.range, r0 = 0, r1 = 1, step, bandwidth, round = false, paddingInner = 0, paddingOuter = 0, align = 0.5;
  delete scale.unknown;
  function rescale() {
    var n = domain().length, reverse2 = r1 < r0, start2 = reverse2 ? r1 : r0, stop = reverse2 ? r0 : r1;
    step = (stop - start2) / Math.max(1, n - paddingInner + paddingOuter * 2);
    if (round)
      step = Math.floor(step);
    start2 += (stop - start2 - step * (n - paddingInner)) * align;
    bandwidth = step * (1 - paddingInner);
    if (round)
      start2 = Math.round(start2), bandwidth = Math.round(bandwidth);
    var values2 = range$1(n).map(function(i) {
      return start2 + step * i;
    });
    return ordinalRange2(reverse2 ? values2.reverse() : values2);
  }
  scale.domain = function(_) {
    return arguments.length ? (domain(_), rescale()) : domain();
  };
  scale.range = function(_) {
    return arguments.length ? ([r0, r1] = _, r0 = +r0, r1 = +r1, rescale()) : [r0, r1];
  };
  scale.rangeRound = function(_) {
    return [r0, r1] = _, r0 = +r0, r1 = +r1, round = true, rescale();
  };
  scale.bandwidth = function() {
    return bandwidth;
  };
  scale.step = function() {
    return step;
  };
  scale.round = function(_) {
    return arguments.length ? (round = !!_, rescale()) : round;
  };
  scale.padding = function(_) {
    return arguments.length ? (paddingInner = Math.min(1, paddingOuter = +_), rescale()) : paddingInner;
  };
  scale.paddingInner = function(_) {
    return arguments.length ? (paddingInner = Math.min(1, _), rescale()) : paddingInner;
  };
  scale.paddingOuter = function(_) {
    return arguments.length ? (paddingOuter = +_, rescale()) : paddingOuter;
  };
  scale.align = function(_) {
    return arguments.length ? (align = Math.max(0, Math.min(1, _)), rescale()) : align;
  };
  scale.copy = function() {
    return band(domain(), [r0, r1]).round(round).paddingInner(paddingInner).paddingOuter(paddingOuter).align(align);
  };
  return initRange.apply(rescale(), arguments);
}
function pointish(scale) {
  var copy2 = scale.copy;
  scale.padding = scale.paddingOuter;
  delete scale.paddingInner;
  delete scale.paddingOuter;
  scale.copy = function() {
    return pointish(copy2());
  };
  return scale;
}
function point() {
  return pointish(band.apply(null, arguments).paddingInner(1));
}
function constants(x) {
  return function() {
    return x;
  };
}
function number$2(x) {
  return +x;
}
var unit$1 = [0, 1];
function identity$3(x) {
  return x;
}
function normalize(a2, b) {
  return (b -= a2 = +a2) ? function(x) {
    return (x - a2) / b;
  } : constants(isNaN(b) ? NaN : 0.5);
}
function clamper(a2, b) {
  var t;
  if (a2 > b)
    t = a2, a2 = b, b = t;
  return function(x) {
    return Math.max(a2, Math.min(b, x));
  };
}
function bimap(domain, range2, interpolate2) {
  var d0 = domain[0], d1 = domain[1], r0 = range2[0], r1 = range2[1];
  if (d1 < d0)
    d0 = normalize(d1, d0), r0 = interpolate2(r1, r0);
  else
    d0 = normalize(d0, d1), r0 = interpolate2(r0, r1);
  return function(x) {
    return r0(d0(x));
  };
}
function polymap(domain, range2, interpolate2) {
  var j = Math.min(domain.length, range2.length) - 1, d = new Array(j), r = new Array(j), i = -1;
  if (domain[j] < domain[0]) {
    domain = domain.slice().reverse();
    range2 = range2.slice().reverse();
  }
  while (++i < j) {
    d[i] = normalize(domain[i], domain[i + 1]);
    r[i] = interpolate2(range2[i], range2[i + 1]);
  }
  return function(x) {
    var i2 = bisect(domain, x, 1, j) - 1;
    return r[i2](d[i2](x));
  };
}
function copy$1(source, target) {
  return target.domain(source.domain()).range(source.range()).interpolate(source.interpolate()).clamp(source.clamp()).unknown(source.unknown());
}
function transformer$1() {
  var domain = unit$1, range2 = unit$1, interpolate2 = interpolate$1, transform, untransform, unknown, clamp = identity$3, piecewise2, output, input;
  function rescale() {
    var n = Math.min(domain.length, range2.length);
    if (clamp !== identity$3)
      clamp = clamper(domain[0], domain[n - 1]);
    piecewise2 = n > 2 ? polymap : bimap;
    output = input = null;
    return scale;
  }
  function scale(x) {
    return x == null || isNaN(x = +x) ? unknown : (output || (output = piecewise2(domain.map(transform), range2, interpolate2)))(transform(clamp(x)));
  }
  scale.invert = function(y) {
    return clamp(untransform((input || (input = piecewise2(range2, domain.map(transform), interpolateNumber)))(y)));
  };
  scale.domain = function(_) {
    return arguments.length ? (domain = Array.from(_, number$2), rescale()) : domain.slice();
  };
  scale.range = function(_) {
    return arguments.length ? (range2 = Array.from(_), rescale()) : range2.slice();
  };
  scale.rangeRound = function(_) {
    return range2 = Array.from(_), interpolate2 = interpolateRound, rescale();
  };
  scale.clamp = function(_) {
    return arguments.length ? (clamp = _ ? true : identity$3, rescale()) : clamp !== identity$3;
  };
  scale.interpolate = function(_) {
    return arguments.length ? (interpolate2 = _, rescale()) : interpolate2;
  };
  scale.unknown = function(_) {
    return arguments.length ? (unknown = _, scale) : unknown;
  };
  return function(t, u) {
    transform = t, untransform = u;
    return rescale();
  };
}
function continuous() {
  return transformer$1()(identity$3, identity$3);
}
function tickFormat(start2, stop, count, specifier) {
  var step = tickStep(start2, stop, count), precision;
  specifier = formatSpecifier(specifier == null ? ",f" : specifier);
  switch (specifier.type) {
    case "s": {
      var value = Math.max(Math.abs(start2), Math.abs(stop));
      if (specifier.precision == null && !isNaN(precision = precisionPrefix(step, value)))
        specifier.precision = precision;
      return formatPrefix(specifier, value);
    }
    case "":
    case "e":
    case "g":
    case "p":
    case "r": {
      if (specifier.precision == null && !isNaN(precision = precisionRound(step, Math.max(Math.abs(start2), Math.abs(stop)))))
        specifier.precision = precision - (specifier.type === "e");
      break;
    }
    case "f":
    case "%": {
      if (specifier.precision == null && !isNaN(precision = precisionFixed(step)))
        specifier.precision = precision - (specifier.type === "%") * 2;
      break;
    }
  }
  return format$1(specifier);
}
function linearish(scale) {
  var domain = scale.domain;
  scale.ticks = function(count) {
    var d = domain();
    return ticks(d[0], d[d.length - 1], count == null ? 10 : count);
  };
  scale.tickFormat = function(count, specifier) {
    var d = domain();
    return tickFormat(d[0], d[d.length - 1], count == null ? 10 : count, specifier);
  };
  scale.nice = function(count) {
    if (count == null)
      count = 10;
    var d = domain();
    var i0 = 0;
    var i1 = d.length - 1;
    var start2 = d[i0];
    var stop = d[i1];
    var prestep;
    var step;
    var maxIter = 10;
    if (stop < start2) {
      step = start2, start2 = stop, stop = step;
      step = i0, i0 = i1, i1 = step;
    }
    while (maxIter-- > 0) {
      step = tickIncrement(start2, stop, count);
      if (step === prestep) {
        d[i0] = start2;
        d[i1] = stop;
        return domain(d);
      } else if (step > 0) {
        start2 = Math.floor(start2 / step) * step;
        stop = Math.ceil(stop / step) * step;
      } else if (step < 0) {
        start2 = Math.ceil(start2 * step) / step;
        stop = Math.floor(stop * step) / step;
      } else {
        break;
      }
      prestep = step;
    }
    return scale;
  };
  return scale;
}
function linear() {
  var scale = continuous();
  scale.copy = function() {
    return copy$1(scale, linear());
  };
  initRange.apply(scale, arguments);
  return linearish(scale);
}
function identity$2(domain) {
  var unknown;
  function scale(x) {
    return x == null || isNaN(x = +x) ? unknown : x;
  }
  scale.invert = scale;
  scale.domain = scale.range = function(_) {
    return arguments.length ? (domain = Array.from(_, number$2), scale) : domain.slice();
  };
  scale.unknown = function(_) {
    return arguments.length ? (unknown = _, scale) : unknown;
  };
  scale.copy = function() {
    return identity$2(domain).unknown(unknown);
  };
  domain = arguments.length ? Array.from(domain, number$2) : [0, 1];
  return linearish(scale);
}
function nice(domain, interval2) {
  domain = domain.slice();
  var i0 = 0, i1 = domain.length - 1, x02 = domain[i0], x12 = domain[i1], t;
  if (x12 < x02) {
    t = i0, i0 = i1, i1 = t;
    t = x02, x02 = x12, x12 = t;
  }
  domain[i0] = interval2.floor(x02);
  domain[i1] = interval2.ceil(x12);
  return domain;
}
function transformLog$1(x) {
  return Math.log(x);
}
function transformExp(x) {
  return Math.exp(x);
}
function transformLogn(x) {
  return -Math.log(-x);
}
function transformExpn(x) {
  return -Math.exp(-x);
}
function pow10(x) {
  return isFinite(x) ? +("1e" + x) : x < 0 ? 0 : x;
}
function powp(base) {
  return base === 10 ? pow10 : base === Math.E ? Math.exp : (x) => Math.pow(base, x);
}
function logp(base) {
  return base === Math.E ? Math.log : base === 10 && Math.log10 || base === 2 && Math.log2 || (base = Math.log(base), (x) => Math.log(x) / base);
}
function reflect(f) {
  return (x, k2) => -f(-x, k2);
}
function loggish(transform) {
  const scale = transform(transformLog$1, transformExp);
  const domain = scale.domain;
  let base = 10;
  let logs;
  let pows;
  function rescale() {
    logs = logp(base), pows = powp(base);
    if (domain()[0] < 0) {
      logs = reflect(logs), pows = reflect(pows);
      transform(transformLogn, transformExpn);
    } else {
      transform(transformLog$1, transformExp);
    }
    return scale;
  }
  scale.base = function(_) {
    return arguments.length ? (base = +_, rescale()) : base;
  };
  scale.domain = function(_) {
    return arguments.length ? (domain(_), rescale()) : domain();
  };
  scale.ticks = (count) => {
    const d = domain();
    let u = d[0];
    let v = d[d.length - 1];
    const r = v < u;
    if (r)
      [u, v] = [v, u];
    let i = logs(u);
    let j = logs(v);
    let k2;
    let t;
    const n = count == null ? 10 : +count;
    let z = [];
    if (!(base % 1) && j - i < n) {
      i = Math.floor(i), j = Math.ceil(j);
      if (u > 0)
        for (; i <= j; ++i) {
          for (k2 = 1; k2 < base; ++k2) {
            t = i < 0 ? k2 / pows(-i) : k2 * pows(i);
            if (t < u)
              continue;
            if (t > v)
              break;
            z.push(t);
          }
        }
      else
        for (; i <= j; ++i) {
          for (k2 = base - 1; k2 >= 1; --k2) {
            t = i > 0 ? k2 / pows(-i) : k2 * pows(i);
            if (t < u)
              continue;
            if (t > v)
              break;
            z.push(t);
          }
        }
      if (z.length * 2 < n)
        z = ticks(u, v, n);
    } else {
      z = ticks(i, j, Math.min(j - i, n)).map(pows);
    }
    return r ? z.reverse() : z;
  };
  scale.tickFormat = (count, specifier) => {
    if (count == null)
      count = 10;
    if (specifier == null)
      specifier = base === 10 ? "s" : ",";
    if (typeof specifier !== "function") {
      if (!(base % 1) && (specifier = formatSpecifier(specifier)).precision == null)
        specifier.trim = true;
      specifier = format$1(specifier);
    }
    if (count === Infinity)
      return specifier;
    const k2 = Math.max(1, base * count / scale.ticks().length);
    return (d) => {
      let i = d / pows(Math.round(logs(d)));
      if (i * base < base - 0.5)
        i *= base;
      return i <= k2 ? specifier(d) : "";
    };
  };
  scale.nice = () => {
    return domain(nice(domain(), {
      floor: (x) => pows(Math.floor(logs(x))),
      ceil: (x) => pows(Math.ceil(logs(x)))
    }));
  };
  return scale;
}
function log() {
  const scale = loggish(transformer$1()).domain([1, 10]);
  scale.copy = () => copy$1(scale, log()).base(scale.base());
  initRange.apply(scale, arguments);
  return scale;
}
function transformSymlog$1(c2) {
  return function(x) {
    return Math.sign(x) * Math.log1p(Math.abs(x / c2));
  };
}
function transformSymexp(c2) {
  return function(x) {
    return Math.sign(x) * Math.expm1(Math.abs(x)) * c2;
  };
}
function symlogish(transform) {
  var c2 = 1, scale = transform(transformSymlog$1(c2), transformSymexp(c2));
  scale.constant = function(_) {
    return arguments.length ? transform(transformSymlog$1(c2 = +_), transformSymexp(c2)) : c2;
  };
  return linearish(scale);
}
function symlog() {
  var scale = symlogish(transformer$1());
  scale.copy = function() {
    return copy$1(scale, symlog()).constant(scale.constant());
  };
  return initRange.apply(scale, arguments);
}
function transformPow$1(exponent2) {
  return function(x) {
    return x < 0 ? -Math.pow(-x, exponent2) : Math.pow(x, exponent2);
  };
}
function transformSqrt$1(x) {
  return x < 0 ? -Math.sqrt(-x) : Math.sqrt(x);
}
function transformSquare(x) {
  return x < 0 ? -x * x : x * x;
}
function powish(transform) {
  var scale = transform(identity$3, identity$3), exponent2 = 1;
  function rescale() {
    return exponent2 === 1 ? transform(identity$3, identity$3) : exponent2 === 0.5 ? transform(transformSqrt$1, transformSquare) : transform(transformPow$1(exponent2), transformPow$1(1 / exponent2));
  }
  scale.exponent = function(_) {
    return arguments.length ? (exponent2 = +_, rescale()) : exponent2;
  };
  return linearish(scale);
}
function pow() {
  var scale = powish(transformer$1());
  scale.copy = function() {
    return copy$1(scale, pow()).exponent(scale.exponent());
  };
  initRange.apply(scale, arguments);
  return scale;
}
function quantile() {
  var domain = [], range2 = [], thresholds = [], unknown;
  function rescale() {
    var i = 0, n = Math.max(1, range2.length);
    thresholds = new Array(n - 1);
    while (++i < n)
      thresholds[i - 1] = quantileSorted(domain, i / n);
    return scale;
  }
  function scale(x) {
    return x == null || isNaN(x = +x) ? unknown : range2[bisect(thresholds, x)];
  }
  scale.invertExtent = function(y) {
    var i = range2.indexOf(y);
    return i < 0 ? [NaN, NaN] : [
      i > 0 ? thresholds[i - 1] : domain[0],
      i < thresholds.length ? thresholds[i] : domain[domain.length - 1]
    ];
  };
  scale.domain = function(_) {
    if (!arguments.length)
      return domain.slice();
    domain = [];
    for (let d of _)
      if (d != null && !isNaN(d = +d))
        domain.push(d);
    domain.sort(ascending$1);
    return rescale();
  };
  scale.range = function(_) {
    return arguments.length ? (range2 = Array.from(_), rescale()) : range2.slice();
  };
  scale.unknown = function(_) {
    return arguments.length ? (unknown = _, scale) : unknown;
  };
  scale.quantiles = function() {
    return thresholds.slice();
  };
  scale.copy = function() {
    return quantile().domain(domain).range(range2).unknown(unknown);
  };
  return initRange.apply(scale, arguments);
}
function threshold() {
  var domain = [0.5], range2 = [0, 1], unknown, n = 1;
  function scale(x) {
    return x != null && x <= x ? range2[bisect(domain, x, 0, n)] : unknown;
  }
  scale.domain = function(_) {
    return arguments.length ? (domain = Array.from(_), n = Math.min(domain.length, range2.length - 1), scale) : domain.slice();
  };
  scale.range = function(_) {
    return arguments.length ? (range2 = Array.from(_), n = Math.min(domain.length, range2.length - 1), scale) : range2.slice();
  };
  scale.invertExtent = function(y) {
    var i = range2.indexOf(y);
    return [domain[i - 1], domain[i]];
  };
  scale.unknown = function(_) {
    return arguments.length ? (unknown = _, scale) : unknown;
  };
  scale.copy = function() {
    return threshold().domain(domain).range(range2).unknown(unknown);
  };
  return initRange.apply(scale, arguments);
}
const t0 = new Date(), t1 = new Date();
function timeInterval(floori, offseti, count, field2) {
  function interval2(date2) {
    return floori(date2 = arguments.length === 0 ? new Date() : new Date(+date2)), date2;
  }
  interval2.floor = (date2) => {
    return floori(date2 = new Date(+date2)), date2;
  };
  interval2.ceil = (date2) => {
    return floori(date2 = new Date(date2 - 1)), offseti(date2, 1), floori(date2), date2;
  };
  interval2.round = (date2) => {
    const d0 = interval2(date2), d1 = interval2.ceil(date2);
    return date2 - d0 < d1 - date2 ? d0 : d1;
  };
  interval2.offset = (date2, step) => {
    return offseti(date2 = new Date(+date2), step == null ? 1 : Math.floor(step)), date2;
  };
  interval2.range = (start2, stop, step) => {
    const range2 = [];
    start2 = interval2.ceil(start2);
    step = step == null ? 1 : Math.floor(step);
    if (!(start2 < stop) || !(step > 0))
      return range2;
    let previous;
    do
      range2.push(previous = new Date(+start2)), offseti(start2, step), floori(start2);
    while (previous < start2 && start2 < stop);
    return range2;
  };
  interval2.filter = (test) => {
    return timeInterval((date2) => {
      if (date2 >= date2)
        while (floori(date2), !test(date2))
          date2.setTime(date2 - 1);
    }, (date2, step) => {
      if (date2 >= date2) {
        if (step < 0)
          while (++step <= 0) {
            while (offseti(date2, -1), !test(date2)) {
            }
          }
        else
          while (--step >= 0) {
            while (offseti(date2, 1), !test(date2)) {
            }
          }
      }
    });
  };
  if (count) {
    interval2.count = (start2, end) => {
      t0.setTime(+start2), t1.setTime(+end);
      floori(t0), floori(t1);
      return Math.floor(count(t0, t1));
    };
    interval2.every = (step) => {
      step = Math.floor(step);
      return !isFinite(step) || !(step > 0) ? null : !(step > 1) ? interval2 : interval2.filter(field2 ? (d) => field2(d) % step === 0 : (d) => interval2.count(0, d) % step === 0);
    };
  }
  return interval2;
}
const millisecond = timeInterval(() => {
}, (date2, step) => {
  date2.setTime(+date2 + step);
}, (start2, end) => {
  return end - start2;
});
millisecond.every = (k2) => {
  k2 = Math.floor(k2);
  if (!isFinite(k2) || !(k2 > 0))
    return null;
  if (!(k2 > 1))
    return millisecond;
  return timeInterval((date2) => {
    date2.setTime(Math.floor(date2 / k2) * k2);
  }, (date2, step) => {
    date2.setTime(+date2 + step * k2);
  }, (start2, end) => {
    return (end - start2) / k2;
  });
};
millisecond.range;
const durationSecond = 1e3;
const durationMinute = durationSecond * 60;
const durationHour = durationMinute * 60;
const durationDay = durationHour * 24;
const durationWeek = durationDay * 7;
const durationMonth = durationDay * 30;
const durationYear = durationDay * 365;
const second$1 = timeInterval((date2) => {
  date2.setTime(date2 - date2.getMilliseconds());
}, (date2, step) => {
  date2.setTime(+date2 + step * durationSecond);
}, (start2, end) => {
  return (end - start2) / durationSecond;
}, (date2) => {
  return date2.getUTCSeconds();
});
second$1.range;
const timeMinute = timeInterval((date2) => {
  date2.setTime(date2 - date2.getMilliseconds() - date2.getSeconds() * durationSecond);
}, (date2, step) => {
  date2.setTime(+date2 + step * durationMinute);
}, (start2, end) => {
  return (end - start2) / durationMinute;
}, (date2) => {
  return date2.getMinutes();
});
timeMinute.range;
const utcMinute = timeInterval((date2) => {
  date2.setUTCSeconds(0, 0);
}, (date2, step) => {
  date2.setTime(+date2 + step * durationMinute);
}, (start2, end) => {
  return (end - start2) / durationMinute;
}, (date2) => {
  return date2.getUTCMinutes();
});
utcMinute.range;
const timeHour = timeInterval((date2) => {
  date2.setTime(date2 - date2.getMilliseconds() - date2.getSeconds() * durationSecond - date2.getMinutes() * durationMinute);
}, (date2, step) => {
  date2.setTime(+date2 + step * durationHour);
}, (start2, end) => {
  return (end - start2) / durationHour;
}, (date2) => {
  return date2.getHours();
});
timeHour.range;
const utcHour = timeInterval((date2) => {
  date2.setUTCMinutes(0, 0, 0);
}, (date2, step) => {
  date2.setTime(+date2 + step * durationHour);
}, (start2, end) => {
  return (end - start2) / durationHour;
}, (date2) => {
  return date2.getUTCHours();
});
utcHour.range;
const timeDay = timeInterval(
  (date2) => date2.setHours(0, 0, 0, 0),
  (date2, step) => date2.setDate(date2.getDate() + step),
  (start2, end) => (end - start2 - (end.getTimezoneOffset() - start2.getTimezoneOffset()) * durationMinute) / durationDay,
  (date2) => date2.getDate() - 1
);
timeDay.range;
const utcDay = timeInterval((date2) => {
  date2.setUTCHours(0, 0, 0, 0);
}, (date2, step) => {
  date2.setUTCDate(date2.getUTCDate() + step);
}, (start2, end) => {
  return (end - start2) / durationDay;
}, (date2) => {
  return date2.getUTCDate() - 1;
});
utcDay.range;
const unixDay = timeInterval((date2) => {
  date2.setUTCHours(0, 0, 0, 0);
}, (date2, step) => {
  date2.setUTCDate(date2.getUTCDate() + step);
}, (start2, end) => {
  return (end - start2) / durationDay;
}, (date2) => {
  return Math.floor(date2 / durationDay);
});
unixDay.range;
function timeWeekday(i) {
  return timeInterval((date2) => {
    date2.setDate(date2.getDate() - (date2.getDay() + 7 - i) % 7);
    date2.setHours(0, 0, 0, 0);
  }, (date2, step) => {
    date2.setDate(date2.getDate() + step * 7);
  }, (start2, end) => {
    return (end - start2 - (end.getTimezoneOffset() - start2.getTimezoneOffset()) * durationMinute) / durationWeek;
  });
}
const timeSunday = timeWeekday(0);
const timeMonday = timeWeekday(1);
const timeTuesday = timeWeekday(2);
const timeWednesday = timeWeekday(3);
const timeThursday = timeWeekday(4);
const timeFriday = timeWeekday(5);
const timeSaturday = timeWeekday(6);
timeSunday.range;
timeMonday.range;
timeTuesday.range;
timeWednesday.range;
timeThursday.range;
timeFriday.range;
timeSaturday.range;
function utcWeekday(i) {
  return timeInterval((date2) => {
    date2.setUTCDate(date2.getUTCDate() - (date2.getUTCDay() + 7 - i) % 7);
    date2.setUTCHours(0, 0, 0, 0);
  }, (date2, step) => {
    date2.setUTCDate(date2.getUTCDate() + step * 7);
  }, (start2, end) => {
    return (end - start2) / durationWeek;
  });
}
const utcSunday = utcWeekday(0);
const utcMonday = utcWeekday(1);
const utcTuesday = utcWeekday(2);
const utcWednesday = utcWeekday(3);
const utcThursday = utcWeekday(4);
const utcFriday = utcWeekday(5);
const utcSaturday = utcWeekday(6);
utcSunday.range;
utcMonday.range;
utcTuesday.range;
utcWednesday.range;
utcThursday.range;
utcFriday.range;
utcSaturday.range;
const timeMonth = timeInterval((date2) => {
  date2.setDate(1);
  date2.setHours(0, 0, 0, 0);
}, (date2, step) => {
  date2.setMonth(date2.getMonth() + step);
}, (start2, end) => {
  return end.getMonth() - start2.getMonth() + (end.getFullYear() - start2.getFullYear()) * 12;
}, (date2) => {
  return date2.getMonth();
});
timeMonth.range;
const utcMonth = timeInterval((date2) => {
  date2.setUTCDate(1);
  date2.setUTCHours(0, 0, 0, 0);
}, (date2, step) => {
  date2.setUTCMonth(date2.getUTCMonth() + step);
}, (start2, end) => {
  return end.getUTCMonth() - start2.getUTCMonth() + (end.getUTCFullYear() - start2.getUTCFullYear()) * 12;
}, (date2) => {
  return date2.getUTCMonth();
});
utcMonth.range;
const timeYear = timeInterval((date2) => {
  date2.setMonth(0, 1);
  date2.setHours(0, 0, 0, 0);
}, (date2, step) => {
  date2.setFullYear(date2.getFullYear() + step);
}, (start2, end) => {
  return end.getFullYear() - start2.getFullYear();
}, (date2) => {
  return date2.getFullYear();
});
timeYear.every = (k2) => {
  return !isFinite(k2 = Math.floor(k2)) || !(k2 > 0) ? null : timeInterval((date2) => {
    date2.setFullYear(Math.floor(date2.getFullYear() / k2) * k2);
    date2.setMonth(0, 1);
    date2.setHours(0, 0, 0, 0);
  }, (date2, step) => {
    date2.setFullYear(date2.getFullYear() + step * k2);
  });
};
timeYear.range;
const utcYear = timeInterval((date2) => {
  date2.setUTCMonth(0, 1);
  date2.setUTCHours(0, 0, 0, 0);
}, (date2, step) => {
  date2.setUTCFullYear(date2.getUTCFullYear() + step);
}, (start2, end) => {
  return end.getUTCFullYear() - start2.getUTCFullYear();
}, (date2) => {
  return date2.getUTCFullYear();
});
utcYear.every = (k2) => {
  return !isFinite(k2 = Math.floor(k2)) || !(k2 > 0) ? null : timeInterval((date2) => {
    date2.setUTCFullYear(Math.floor(date2.getUTCFullYear() / k2) * k2);
    date2.setUTCMonth(0, 1);
    date2.setUTCHours(0, 0, 0, 0);
  }, (date2, step) => {
    date2.setUTCFullYear(date2.getUTCFullYear() + step * k2);
  });
};
utcYear.range;
function ticker(year, month, week, day, hour, minute) {
  const tickIntervals = [
    [second$1, 1, durationSecond],
    [second$1, 5, 5 * durationSecond],
    [second$1, 15, 15 * durationSecond],
    [second$1, 30, 30 * durationSecond],
    [minute, 1, durationMinute],
    [minute, 5, 5 * durationMinute],
    [minute, 15, 15 * durationMinute],
    [minute, 30, 30 * durationMinute],
    [hour, 1, durationHour],
    [hour, 3, 3 * durationHour],
    [hour, 6, 6 * durationHour],
    [hour, 12, 12 * durationHour],
    [day, 1, durationDay],
    [day, 2, 2 * durationDay],
    [week, 1, durationWeek],
    [month, 1, durationMonth],
    [month, 3, 3 * durationMonth],
    [year, 1, durationYear]
  ];
  function ticks2(start2, stop, count) {
    const reverse2 = stop < start2;
    if (reverse2)
      [start2, stop] = [stop, start2];
    const interval2 = count && typeof count.range === "function" ? count : tickInterval(start2, stop, count);
    const ticks3 = interval2 ? interval2.range(start2, +stop + 1) : [];
    return reverse2 ? ticks3.reverse() : ticks3;
  }
  function tickInterval(start2, stop, count) {
    const target = Math.abs(stop - start2) / count;
    const i = bisector(([, , step2]) => step2).right(tickIntervals, target);
    if (i === tickIntervals.length)
      return year.every(tickStep(start2 / durationYear, stop / durationYear, count));
    if (i === 0)
      return millisecond.every(Math.max(tickStep(start2, stop, count), 1));
    const [t, step] = tickIntervals[target / tickIntervals[i - 1][2] < tickIntervals[i][2] / target ? i - 1 : i];
    return t.every(step);
  }
  return [ticks2, tickInterval];
}
const [utcTicks, utcTickInterval] = ticker(utcYear, utcMonth, utcSunday, unixDay, utcHour, utcMinute);
const [timeTicks, timeTickInterval] = ticker(timeYear, timeMonth, timeSunday, timeDay, timeHour, timeMinute);
function localDate(d) {
  if (0 <= d.y && d.y < 100) {
    var date2 = new Date(-1, d.m, d.d, d.H, d.M, d.S, d.L);
    date2.setFullYear(d.y);
    return date2;
  }
  return new Date(d.y, d.m, d.d, d.H, d.M, d.S, d.L);
}
function utcDate(d) {
  if (0 <= d.y && d.y < 100) {
    var date2 = new Date(Date.UTC(-1, d.m, d.d, d.H, d.M, d.S, d.L));
    date2.setUTCFullYear(d.y);
    return date2;
  }
  return new Date(Date.UTC(d.y, d.m, d.d, d.H, d.M, d.S, d.L));
}
function newDate(y, m, d) {
  return { y, m, d, H: 0, M: 0, S: 0, L: 0 };
}
function formatLocale(locale2) {
  var locale_dateTime = locale2.dateTime, locale_date = locale2.date, locale_time = locale2.time, locale_periods = locale2.periods, locale_weekdays = locale2.days, locale_shortWeekdays = locale2.shortDays, locale_months = locale2.months, locale_shortMonths = locale2.shortMonths;
  var periodRe = formatRe(locale_periods), periodLookup = formatLookup(locale_periods), weekdayRe = formatRe(locale_weekdays), weekdayLookup = formatLookup(locale_weekdays), shortWeekdayRe = formatRe(locale_shortWeekdays), shortWeekdayLookup = formatLookup(locale_shortWeekdays), monthRe = formatRe(locale_months), monthLookup = formatLookup(locale_months), shortMonthRe = formatRe(locale_shortMonths), shortMonthLookup = formatLookup(locale_shortMonths);
  var formats = {
    "a": formatShortWeekday,
    "A": formatWeekday,
    "b": formatShortMonth,
    "B": formatMonth,
    "c": null,
    "d": formatDayOfMonth,
    "e": formatDayOfMonth,
    "f": formatMicroseconds,
    "g": formatYearISO,
    "G": formatFullYearISO,
    "H": formatHour24,
    "I": formatHour12,
    "j": formatDayOfYear,
    "L": formatMilliseconds,
    "m": formatMonthNumber,
    "M": formatMinutes,
    "p": formatPeriod,
    "q": formatQuarter,
    "Q": formatUnixTimestamp,
    "s": formatUnixTimestampSeconds,
    "S": formatSeconds,
    "u": formatWeekdayNumberMonday,
    "U": formatWeekNumberSunday,
    "V": formatWeekNumberISO,
    "w": formatWeekdayNumberSunday,
    "W": formatWeekNumberMonday,
    "x": null,
    "X": null,
    "y": formatYear$1,
    "Y": formatFullYear,
    "Z": formatZone,
    "%": formatLiteralPercent
  };
  var utcFormats = {
    "a": formatUTCShortWeekday,
    "A": formatUTCWeekday,
    "b": formatUTCShortMonth,
    "B": formatUTCMonth,
    "c": null,
    "d": formatUTCDayOfMonth,
    "e": formatUTCDayOfMonth,
    "f": formatUTCMicroseconds,
    "g": formatUTCYearISO,
    "G": formatUTCFullYearISO,
    "H": formatUTCHour24,
    "I": formatUTCHour12,
    "j": formatUTCDayOfYear,
    "L": formatUTCMilliseconds,
    "m": formatUTCMonthNumber,
    "M": formatUTCMinutes,
    "p": formatUTCPeriod,
    "q": formatUTCQuarter,
    "Q": formatUnixTimestamp,
    "s": formatUnixTimestampSeconds,
    "S": formatUTCSeconds,
    "u": formatUTCWeekdayNumberMonday,
    "U": formatUTCWeekNumberSunday,
    "V": formatUTCWeekNumberISO,
    "w": formatUTCWeekdayNumberSunday,
    "W": formatUTCWeekNumberMonday,
    "x": null,
    "X": null,
    "y": formatUTCYear,
    "Y": formatUTCFullYear,
    "Z": formatUTCZone,
    "%": formatLiteralPercent
  };
  var parses = {
    "a": parseShortWeekday,
    "A": parseWeekday,
    "b": parseShortMonth,
    "B": parseMonth,
    "c": parseLocaleDateTime,
    "d": parseDayOfMonth,
    "e": parseDayOfMonth,
    "f": parseMicroseconds,
    "g": parseYear,
    "G": parseFullYear,
    "H": parseHour24,
    "I": parseHour24,
    "j": parseDayOfYear,
    "L": parseMilliseconds,
    "m": parseMonthNumber,
    "M": parseMinutes,
    "p": parsePeriod,
    "q": parseQuarter,
    "Q": parseUnixTimestamp,
    "s": parseUnixTimestampSeconds,
    "S": parseSeconds,
    "u": parseWeekdayNumberMonday,
    "U": parseWeekNumberSunday,
    "V": parseWeekNumberISO,
    "w": parseWeekdayNumberSunday,
    "W": parseWeekNumberMonday,
    "x": parseLocaleDate,
    "X": parseLocaleTime,
    "y": parseYear,
    "Y": parseFullYear,
    "Z": parseZone,
    "%": parseLiteralPercent
  };
  formats.x = newFormat(locale_date, formats);
  formats.X = newFormat(locale_time, formats);
  formats.c = newFormat(locale_dateTime, formats);
  utcFormats.x = newFormat(locale_date, utcFormats);
  utcFormats.X = newFormat(locale_time, utcFormats);
  utcFormats.c = newFormat(locale_dateTime, utcFormats);
  function newFormat(specifier, formats2) {
    return function(date2) {
      var string2 = [], i = -1, j = 0, n = specifier.length, c2, pad2, format2;
      if (!(date2 instanceof Date))
        date2 = new Date(+date2);
      while (++i < n) {
        if (specifier.charCodeAt(i) === 37) {
          string2.push(specifier.slice(j, i));
          if ((pad2 = pads[c2 = specifier.charAt(++i)]) != null)
            c2 = specifier.charAt(++i);
          else
            pad2 = c2 === "e" ? " " : "0";
          if (format2 = formats2[c2])
            c2 = format2(date2, pad2);
          string2.push(c2);
          j = i + 1;
        }
      }
      string2.push(specifier.slice(j, i));
      return string2.join("");
    };
  }
  function newParse(specifier, Z) {
    return function(string2) {
      var d = newDate(1900, void 0, 1), i = parseSpecifier(d, specifier, string2 += "", 0), week, day;
      if (i != string2.length)
        return null;
      if ("Q" in d)
        return new Date(d.Q);
      if ("s" in d)
        return new Date(d.s * 1e3 + ("L" in d ? d.L : 0));
      if (Z && !("Z" in d))
        d.Z = 0;
      if ("p" in d)
        d.H = d.H % 12 + d.p * 12;
      if (d.m === void 0)
        d.m = "q" in d ? d.q : 0;
      if ("V" in d) {
        if (d.V < 1 || d.V > 53)
          return null;
        if (!("w" in d))
          d.w = 1;
        if ("Z" in d) {
          week = utcDate(newDate(d.y, 0, 1)), day = week.getUTCDay();
          week = day > 4 || day === 0 ? utcMonday.ceil(week) : utcMonday(week);
          week = utcDay.offset(week, (d.V - 1) * 7);
          d.y = week.getUTCFullYear();
          d.m = week.getUTCMonth();
          d.d = week.getUTCDate() + (d.w + 6) % 7;
        } else {
          week = localDate(newDate(d.y, 0, 1)), day = week.getDay();
          week = day > 4 || day === 0 ? timeMonday.ceil(week) : timeMonday(week);
          week = timeDay.offset(week, (d.V - 1) * 7);
          d.y = week.getFullYear();
          d.m = week.getMonth();
          d.d = week.getDate() + (d.w + 6) % 7;
        }
      } else if ("W" in d || "U" in d) {
        if (!("w" in d))
          d.w = "u" in d ? d.u % 7 : "W" in d ? 1 : 0;
        day = "Z" in d ? utcDate(newDate(d.y, 0, 1)).getUTCDay() : localDate(newDate(d.y, 0, 1)).getDay();
        d.m = 0;
        d.d = "W" in d ? (d.w + 6) % 7 + d.W * 7 - (day + 5) % 7 : d.w + d.U * 7 - (day + 6) % 7;
      }
      if ("Z" in d) {
        d.H += d.Z / 100 | 0;
        d.M += d.Z % 100;
        return utcDate(d);
      }
      return localDate(d);
    };
  }
  function parseSpecifier(d, specifier, string2, j) {
    var i = 0, n = specifier.length, m = string2.length, c2, parse2;
    while (i < n) {
      if (j >= m)
        return -1;
      c2 = specifier.charCodeAt(i++);
      if (c2 === 37) {
        c2 = specifier.charAt(i++);
        parse2 = parses[c2 in pads ? specifier.charAt(i++) : c2];
        if (!parse2 || (j = parse2(d, string2, j)) < 0)
          return -1;
      } else if (c2 != string2.charCodeAt(j++)) {
        return -1;
      }
    }
    return j;
  }
  function parsePeriod(d, string2, i) {
    var n = periodRe.exec(string2.slice(i));
    return n ? (d.p = periodLookup.get(n[0].toLowerCase()), i + n[0].length) : -1;
  }
  function parseShortWeekday(d, string2, i) {
    var n = shortWeekdayRe.exec(string2.slice(i));
    return n ? (d.w = shortWeekdayLookup.get(n[0].toLowerCase()), i + n[0].length) : -1;
  }
  function parseWeekday(d, string2, i) {
    var n = weekdayRe.exec(string2.slice(i));
    return n ? (d.w = weekdayLookup.get(n[0].toLowerCase()), i + n[0].length) : -1;
  }
  function parseShortMonth(d, string2, i) {
    var n = shortMonthRe.exec(string2.slice(i));
    return n ? (d.m = shortMonthLookup.get(n[0].toLowerCase()), i + n[0].length) : -1;
  }
  function parseMonth(d, string2, i) {
    var n = monthRe.exec(string2.slice(i));
    return n ? (d.m = monthLookup.get(n[0].toLowerCase()), i + n[0].length) : -1;
  }
  function parseLocaleDateTime(d, string2, i) {
    return parseSpecifier(d, locale_dateTime, string2, i);
  }
  function parseLocaleDate(d, string2, i) {
    return parseSpecifier(d, locale_date, string2, i);
  }
  function parseLocaleTime(d, string2, i) {
    return parseSpecifier(d, locale_time, string2, i);
  }
  function formatShortWeekday(d) {
    return locale_shortWeekdays[d.getDay()];
  }
  function formatWeekday(d) {
    return locale_weekdays[d.getDay()];
  }
  function formatShortMonth(d) {
    return locale_shortMonths[d.getMonth()];
  }
  function formatMonth(d) {
    return locale_months[d.getMonth()];
  }
  function formatPeriod(d) {
    return locale_periods[+(d.getHours() >= 12)];
  }
  function formatQuarter(d) {
    return 1 + ~~(d.getMonth() / 3);
  }
  function formatUTCShortWeekday(d) {
    return locale_shortWeekdays[d.getUTCDay()];
  }
  function formatUTCWeekday(d) {
    return locale_weekdays[d.getUTCDay()];
  }
  function formatUTCShortMonth(d) {
    return locale_shortMonths[d.getUTCMonth()];
  }
  function formatUTCMonth(d) {
    return locale_months[d.getUTCMonth()];
  }
  function formatUTCPeriod(d) {
    return locale_periods[+(d.getUTCHours() >= 12)];
  }
  function formatUTCQuarter(d) {
    return 1 + ~~(d.getUTCMonth() / 3);
  }
  return {
    format: function(specifier) {
      var f = newFormat(specifier += "", formats);
      f.toString = function() {
        return specifier;
      };
      return f;
    },
    parse: function(specifier) {
      var p = newParse(specifier += "", false);
      p.toString = function() {
        return specifier;
      };
      return p;
    },
    utcFormat: function(specifier) {
      var f = newFormat(specifier += "", utcFormats);
      f.toString = function() {
        return specifier;
      };
      return f;
    },
    utcParse: function(specifier) {
      var p = newParse(specifier += "", true);
      p.toString = function() {
        return specifier;
      };
      return p;
    }
  };
}
var pads = { "-": "", "_": " ", "0": "0" }, numberRe = /^\s*\d+/, percentRe = /^%/, requoteRe = /[\\^$*+?|[\]().{}]/g;
function pad$1(value, fill, width) {
  var sign2 = value < 0 ? "-" : "", string2 = (sign2 ? -value : value) + "", length2 = string2.length;
  return sign2 + (length2 < width ? new Array(width - length2 + 1).join(fill) + string2 : string2);
}
function requote(s2) {
  return s2.replace(requoteRe, "\\$&");
}
function formatRe(names) {
  return new RegExp("^(?:" + names.map(requote).join("|") + ")", "i");
}
function formatLookup(names) {
  return new Map(names.map((name, i) => [name.toLowerCase(), i]));
}
function parseWeekdayNumberSunday(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 1));
  return n ? (d.w = +n[0], i + n[0].length) : -1;
}
function parseWeekdayNumberMonday(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 1));
  return n ? (d.u = +n[0], i + n[0].length) : -1;
}
function parseWeekNumberSunday(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 2));
  return n ? (d.U = +n[0], i + n[0].length) : -1;
}
function parseWeekNumberISO(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 2));
  return n ? (d.V = +n[0], i + n[0].length) : -1;
}
function parseWeekNumberMonday(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 2));
  return n ? (d.W = +n[0], i + n[0].length) : -1;
}
function parseFullYear(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 4));
  return n ? (d.y = +n[0], i + n[0].length) : -1;
}
function parseYear(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 2));
  return n ? (d.y = +n[0] + (+n[0] > 68 ? 1900 : 2e3), i + n[0].length) : -1;
}
function parseZone(d, string2, i) {
  var n = /^(Z)|([+-]\d\d)(?::?(\d\d))?/.exec(string2.slice(i, i + 6));
  return n ? (d.Z = n[1] ? 0 : -(n[2] + (n[3] || "00")), i + n[0].length) : -1;
}
function parseQuarter(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 1));
  return n ? (d.q = n[0] * 3 - 3, i + n[0].length) : -1;
}
function parseMonthNumber(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 2));
  return n ? (d.m = n[0] - 1, i + n[0].length) : -1;
}
function parseDayOfMonth(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 2));
  return n ? (d.d = +n[0], i + n[0].length) : -1;
}
function parseDayOfYear(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 3));
  return n ? (d.m = 0, d.d = +n[0], i + n[0].length) : -1;
}
function parseHour24(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 2));
  return n ? (d.H = +n[0], i + n[0].length) : -1;
}
function parseMinutes(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 2));
  return n ? (d.M = +n[0], i + n[0].length) : -1;
}
function parseSeconds(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 2));
  return n ? (d.S = +n[0], i + n[0].length) : -1;
}
function parseMilliseconds(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 3));
  return n ? (d.L = +n[0], i + n[0].length) : -1;
}
function parseMicroseconds(d, string2, i) {
  var n = numberRe.exec(string2.slice(i, i + 6));
  return n ? (d.L = Math.floor(n[0] / 1e3), i + n[0].length) : -1;
}
function parseLiteralPercent(d, string2, i) {
  var n = percentRe.exec(string2.slice(i, i + 1));
  return n ? i + n[0].length : -1;
}
function parseUnixTimestamp(d, string2, i) {
  var n = numberRe.exec(string2.slice(i));
  return n ? (d.Q = +n[0], i + n[0].length) : -1;
}
function parseUnixTimestampSeconds(d, string2, i) {
  var n = numberRe.exec(string2.slice(i));
  return n ? (d.s = +n[0], i + n[0].length) : -1;
}
function formatDayOfMonth(d, p) {
  return pad$1(d.getDate(), p, 2);
}
function formatHour24(d, p) {
  return pad$1(d.getHours(), p, 2);
}
function formatHour12(d, p) {
  return pad$1(d.getHours() % 12 || 12, p, 2);
}
function formatDayOfYear(d, p) {
  return pad$1(1 + timeDay.count(timeYear(d), d), p, 3);
}
function formatMilliseconds(d, p) {
  return pad$1(d.getMilliseconds(), p, 3);
}
function formatMicroseconds(d, p) {
  return formatMilliseconds(d, p) + "000";
}
function formatMonthNumber(d, p) {
  return pad$1(d.getMonth() + 1, p, 2);
}
function formatMinutes(d, p) {
  return pad$1(d.getMinutes(), p, 2);
}
function formatSeconds(d, p) {
  return pad$1(d.getSeconds(), p, 2);
}
function formatWeekdayNumberMonday(d) {
  var day = d.getDay();
  return day === 0 ? 7 : day;
}
function formatWeekNumberSunday(d, p) {
  return pad$1(timeSunday.count(timeYear(d) - 1, d), p, 2);
}
function dISO(d) {
  var day = d.getDay();
  return day >= 4 || day === 0 ? timeThursday(d) : timeThursday.ceil(d);
}
function formatWeekNumberISO(d, p) {
  d = dISO(d);
  return pad$1(timeThursday.count(timeYear(d), d) + (timeYear(d).getDay() === 4), p, 2);
}
function formatWeekdayNumberSunday(d) {
  return d.getDay();
}
function formatWeekNumberMonday(d, p) {
  return pad$1(timeMonday.count(timeYear(d) - 1, d), p, 2);
}
function formatYear$1(d, p) {
  return pad$1(d.getFullYear() % 100, p, 2);
}
function formatYearISO(d, p) {
  d = dISO(d);
  return pad$1(d.getFullYear() % 100, p, 2);
}
function formatFullYear(d, p) {
  return pad$1(d.getFullYear() % 1e4, p, 4);
}
function formatFullYearISO(d, p) {
  var day = d.getDay();
  d = day >= 4 || day === 0 ? timeThursday(d) : timeThursday.ceil(d);
  return pad$1(d.getFullYear() % 1e4, p, 4);
}
function formatZone(d) {
  var z = d.getTimezoneOffset();
  return (z > 0 ? "-" : (z *= -1, "+")) + pad$1(z / 60 | 0, "0", 2) + pad$1(z % 60, "0", 2);
}
function formatUTCDayOfMonth(d, p) {
  return pad$1(d.getUTCDate(), p, 2);
}
function formatUTCHour24(d, p) {
  return pad$1(d.getUTCHours(), p, 2);
}
function formatUTCHour12(d, p) {
  return pad$1(d.getUTCHours() % 12 || 12, p, 2);
}
function formatUTCDayOfYear(d, p) {
  return pad$1(1 + utcDay.count(utcYear(d), d), p, 3);
}
function formatUTCMilliseconds(d, p) {
  return pad$1(d.getUTCMilliseconds(), p, 3);
}
function formatUTCMicroseconds(d, p) {
  return formatUTCMilliseconds(d, p) + "000";
}
function formatUTCMonthNumber(d, p) {
  return pad$1(d.getUTCMonth() + 1, p, 2);
}
function formatUTCMinutes(d, p) {
  return pad$1(d.getUTCMinutes(), p, 2);
}
function formatUTCSeconds(d, p) {
  return pad$1(d.getUTCSeconds(), p, 2);
}
function formatUTCWeekdayNumberMonday(d) {
  var dow = d.getUTCDay();
  return dow === 0 ? 7 : dow;
}
function formatUTCWeekNumberSunday(d, p) {
  return pad$1(utcSunday.count(utcYear(d) - 1, d), p, 2);
}
function UTCdISO(d) {
  var day = d.getUTCDay();
  return day >= 4 || day === 0 ? utcThursday(d) : utcThursday.ceil(d);
}
function formatUTCWeekNumberISO(d, p) {
  d = UTCdISO(d);
  return pad$1(utcThursday.count(utcYear(d), d) + (utcYear(d).getUTCDay() === 4), p, 2);
}
function formatUTCWeekdayNumberSunday(d) {
  return d.getUTCDay();
}
function formatUTCWeekNumberMonday(d, p) {
  return pad$1(utcMonday.count(utcYear(d) - 1, d), p, 2);
}
function formatUTCYear(d, p) {
  return pad$1(d.getUTCFullYear() % 100, p, 2);
}
function formatUTCYearISO(d, p) {
  d = UTCdISO(d);
  return pad$1(d.getUTCFullYear() % 100, p, 2);
}
function formatUTCFullYear(d, p) {
  return pad$1(d.getUTCFullYear() % 1e4, p, 4);
}
function formatUTCFullYearISO(d, p) {
  var day = d.getUTCDay();
  d = day >= 4 || day === 0 ? utcThursday(d) : utcThursday.ceil(d);
  return pad$1(d.getUTCFullYear() % 1e4, p, 4);
}
function formatUTCZone() {
  return "+0000";
}
function formatLiteralPercent() {
  return "%";
}
function formatUnixTimestamp(d) {
  return +d;
}
function formatUnixTimestampSeconds(d) {
  return Math.floor(+d / 1e3);
}
var locale;
var timeFormat;
var utcFormat;
defaultLocale({
  dateTime: "%x, %X",
  date: "%-m/%-d/%Y",
  time: "%-I:%M:%S %p",
  periods: ["AM", "PM"],
  days: ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"],
  shortDays: ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"],
  months: ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"],
  shortMonths: ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
});
function defaultLocale(definition) {
  locale = formatLocale(definition);
  timeFormat = locale.format;
  locale.parse;
  utcFormat = locale.utcFormat;
  locale.utcParse;
  return locale;
}
function date(t) {
  return new Date(t);
}
function number$1(t) {
  return t instanceof Date ? +t : +new Date(+t);
}
function calendar(ticks2, tickInterval, year, month, week, day, hour, minute, second2, format2) {
  var scale = continuous(), invert = scale.invert, domain = scale.domain;
  var formatMillisecond = format2(".%L"), formatSecond = format2(":%S"), formatMinute = format2("%I:%M"), formatHour = format2("%I %p"), formatDay = format2("%a %d"), formatWeek = format2("%b %d"), formatMonth = format2("%B"), formatYear2 = format2("%Y");
  function tickFormat2(date2) {
    return (second2(date2) < date2 ? formatMillisecond : minute(date2) < date2 ? formatSecond : hour(date2) < date2 ? formatMinute : day(date2) < date2 ? formatHour : month(date2) < date2 ? week(date2) < date2 ? formatDay : formatWeek : year(date2) < date2 ? formatMonth : formatYear2)(date2);
  }
  scale.invert = function(y) {
    return new Date(invert(y));
  };
  scale.domain = function(_) {
    return arguments.length ? domain(Array.from(_, number$1)) : domain().map(date);
  };
  scale.ticks = function(interval2) {
    var d = domain();
    return ticks2(d[0], d[d.length - 1], interval2 == null ? 10 : interval2);
  };
  scale.tickFormat = function(count, specifier) {
    return specifier == null ? tickFormat2 : format2(specifier);
  };
  scale.nice = function(interval2) {
    var d = domain();
    if (!interval2 || typeof interval2.range !== "function")
      interval2 = tickInterval(d[0], d[d.length - 1], interval2 == null ? 10 : interval2);
    return interval2 ? domain(nice(d, interval2)) : scale;
  };
  scale.copy = function() {
    return copy$1(scale, calendar(ticks2, tickInterval, year, month, week, day, hour, minute, second2, format2));
  };
  return scale;
}
function time() {
  return initRange.apply(calendar(timeTicks, timeTickInterval, timeYear, timeMonth, timeSunday, timeDay, timeHour, timeMinute, second$1, timeFormat).domain([new Date(2e3, 0, 1), new Date(2e3, 0, 2)]), arguments);
}
function utcTime() {
  return initRange.apply(calendar(utcTicks, utcTickInterval, utcYear, utcMonth, utcSunday, utcDay, utcHour, utcMinute, second$1, utcFormat).domain([Date.UTC(2e3, 0, 1), Date.UTC(2e3, 0, 2)]), arguments);
}
function copy(source, target) {
  return target.domain(source.domain()).interpolator(source.interpolator()).clamp(source.clamp()).unknown(source.unknown());
}
function transformer() {
  var x02 = 0, x12 = 0.5, x2 = 1, s2 = 1, t02, t12, t22, k10, k21, interpolator = identity$3, transform, clamp = false, unknown;
  function scale(x) {
    return isNaN(x = +x) ? unknown : (x = 0.5 + ((x = +transform(x)) - t12) * (s2 * x < s2 * t12 ? k10 : k21), interpolator(clamp ? Math.max(0, Math.min(1, x)) : x));
  }
  scale.domain = function(_) {
    return arguments.length ? ([x02, x12, x2] = _, t02 = transform(x02 = +x02), t12 = transform(x12 = +x12), t22 = transform(x2 = +x2), k10 = t02 === t12 ? 0 : 0.5 / (t12 - t02), k21 = t12 === t22 ? 0 : 0.5 / (t22 - t12), s2 = t12 < t02 ? -1 : 1, scale) : [x02, x12, x2];
  };
  scale.clamp = function(_) {
    return arguments.length ? (clamp = !!_, scale) : clamp;
  };
  scale.interpolator = function(_) {
    return arguments.length ? (interpolator = _, scale) : interpolator;
  };
  function range2(interpolate2) {
    return function(_) {
      var r0, r1, r2;
      return arguments.length ? ([r0, r1, r2] = _, interpolator = piecewise(interpolate2, [r0, r1, r2]), scale) : [interpolator(0), interpolator(0.5), interpolator(1)];
    };
  }
  scale.range = range2(interpolate$1);
  scale.rangeRound = range2(interpolateRound);
  scale.unknown = function(_) {
    return arguments.length ? (unknown = _, scale) : unknown;
  };
  return function(t) {
    transform = t, t02 = t(x02), t12 = t(x12), t22 = t(x2), k10 = t02 === t12 ? 0 : 0.5 / (t12 - t02), k21 = t12 === t22 ? 0 : 0.5 / (t22 - t12), s2 = t12 < t02 ? -1 : 1;
    return scale;
  };
}
function diverging() {
  var scale = linearish(transformer()(identity$3));
  scale.copy = function() {
    return copy(scale, diverging());
  };
  return initInterpolator.apply(scale, arguments);
}
function divergingLog() {
  var scale = loggish(transformer()).domain([0.1, 1, 10]);
  scale.copy = function() {
    return copy(scale, divergingLog()).base(scale.base());
  };
  return initInterpolator.apply(scale, arguments);
}
function divergingSymlog() {
  var scale = symlogish(transformer());
  scale.copy = function() {
    return copy(scale, divergingSymlog()).constant(scale.constant());
  };
  return initInterpolator.apply(scale, arguments);
}
function divergingPow() {
  var scale = powish(transformer());
  scale.copy = function() {
    return copy(scale, divergingPow()).exponent(scale.exponent());
  };
  return initInterpolator.apply(scale, arguments);
}
function colors(specifier) {
  var n = specifier.length / 6 | 0, colors2 = new Array(n), i = 0;
  while (i < n)
    colors2[i] = "#" + specifier.slice(i * 6, ++i * 6);
  return colors2;
}
const schemeCategory10 = colors("1f77b4ff7f0e2ca02cd627289467bd8c564be377c27f7f7fbcbd2217becf");
const schemeAccent = colors("7fc97fbeaed4fdc086ffff99386cb0f0027fbf5b17666666");
const schemeDark2 = colors("1b9e77d95f027570b3e7298a66a61ee6ab02a6761d666666");
const schemePaired = colors("a6cee31f78b4b2df8a33a02cfb9a99e31a1cfdbf6fff7f00cab2d66a3d9affff99b15928");
const schemePastel1 = colors("fbb4aeb3cde3ccebc5decbe4fed9a6ffffcce5d8bdfddaecf2f2f2");
const schemePastel2 = colors("b3e2cdfdcdaccbd5e8f4cae4e6f5c9fff2aef1e2cccccccc");
const schemeSet1 = colors("e41a1c377eb84daf4a984ea3ff7f00ffff33a65628f781bf999999");
const schemeSet2 = colors("66c2a5fc8d628da0cbe78ac3a6d854ffd92fe5c494b3b3b3");
const schemeSet3 = colors("8dd3c7ffffb3bebadafb807280b1d3fdb462b3de69fccde5d9d9d9bc80bdccebc5ffed6f");
const schemeTableau10 = colors("4e79a7f28e2ce1575976b7b259a14fedc949af7aa1ff9da79c755fbab0ab");
const ramp$1 = (scheme2) => rgbBasis(scheme2[scheme2.length - 1]);
var scheme$q = new Array(3).concat(
  "d8b365f5f5f55ab4ac",
  "a6611adfc27d80cdc1018571",
  "a6611adfc27df5f5f580cdc1018571",
  "8c510ad8b365f6e8c3c7eae55ab4ac01665e",
  "8c510ad8b365f6e8c3f5f5f5c7eae55ab4ac01665e",
  "8c510abf812ddfc27df6e8c3c7eae580cdc135978f01665e",
  "8c510abf812ddfc27df6e8c3f5f5f5c7eae580cdc135978f01665e",
  "5430058c510abf812ddfc27df6e8c3c7eae580cdc135978f01665e003c30",
  "5430058c510abf812ddfc27df6e8c3f5f5f5c7eae580cdc135978f01665e003c30"
).map(colors);
const interpolateBrBG = ramp$1(scheme$q);
var scheme$p = new Array(3).concat(
  "af8dc3f7f7f77fbf7b",
  "7b3294c2a5cfa6dba0008837",
  "7b3294c2a5cff7f7f7a6dba0008837",
  "762a83af8dc3e7d4e8d9f0d37fbf7b1b7837",
  "762a83af8dc3e7d4e8f7f7f7d9f0d37fbf7b1b7837",
  "762a839970abc2a5cfe7d4e8d9f0d3a6dba05aae611b7837",
  "762a839970abc2a5cfe7d4e8f7f7f7d9f0d3a6dba05aae611b7837",
  "40004b762a839970abc2a5cfe7d4e8d9f0d3a6dba05aae611b783700441b",
  "40004b762a839970abc2a5cfe7d4e8f7f7f7d9f0d3a6dba05aae611b783700441b"
).map(colors);
const interpolatePRGn = ramp$1(scheme$p);
var scheme$o = new Array(3).concat(
  "e9a3c9f7f7f7a1d76a",
  "d01c8bf1b6dab8e1864dac26",
  "d01c8bf1b6daf7f7f7b8e1864dac26",
  "c51b7de9a3c9fde0efe6f5d0a1d76a4d9221",
  "c51b7de9a3c9fde0eff7f7f7e6f5d0a1d76a4d9221",
  "c51b7dde77aef1b6dafde0efe6f5d0b8e1867fbc414d9221",
  "c51b7dde77aef1b6dafde0eff7f7f7e6f5d0b8e1867fbc414d9221",
  "8e0152c51b7dde77aef1b6dafde0efe6f5d0b8e1867fbc414d9221276419",
  "8e0152c51b7dde77aef1b6dafde0eff7f7f7e6f5d0b8e1867fbc414d9221276419"
).map(colors);
const interpolatePiYG = ramp$1(scheme$o);
var scheme$n = new Array(3).concat(
  "998ec3f7f7f7f1a340",
  "5e3c99b2abd2fdb863e66101",
  "5e3c99b2abd2f7f7f7fdb863e66101",
  "542788998ec3d8daebfee0b6f1a340b35806",
  "542788998ec3d8daebf7f7f7fee0b6f1a340b35806",
  "5427888073acb2abd2d8daebfee0b6fdb863e08214b35806",
  "5427888073acb2abd2d8daebf7f7f7fee0b6fdb863e08214b35806",
  "2d004b5427888073acb2abd2d8daebfee0b6fdb863e08214b358067f3b08",
  "2d004b5427888073acb2abd2d8daebf7f7f7fee0b6fdb863e08214b358067f3b08"
).map(colors);
const interpolatePuOr = ramp$1(scheme$n);
var scheme$m = new Array(3).concat(
  "ef8a62f7f7f767a9cf",
  "ca0020f4a58292c5de0571b0",
  "ca0020f4a582f7f7f792c5de0571b0",
  "b2182bef8a62fddbc7d1e5f067a9cf2166ac",
  "b2182bef8a62fddbc7f7f7f7d1e5f067a9cf2166ac",
  "b2182bd6604df4a582fddbc7d1e5f092c5de4393c32166ac",
  "b2182bd6604df4a582fddbc7f7f7f7d1e5f092c5de4393c32166ac",
  "67001fb2182bd6604df4a582fddbc7d1e5f092c5de4393c32166ac053061",
  "67001fb2182bd6604df4a582fddbc7f7f7f7d1e5f092c5de4393c32166ac053061"
).map(colors);
const interpolateRdBu = ramp$1(scheme$m);
var scheme$l = new Array(3).concat(
  "ef8a62ffffff999999",
  "ca0020f4a582bababa404040",
  "ca0020f4a582ffffffbababa404040",
  "b2182bef8a62fddbc7e0e0e09999994d4d4d",
  "b2182bef8a62fddbc7ffffffe0e0e09999994d4d4d",
  "b2182bd6604df4a582fddbc7e0e0e0bababa8787874d4d4d",
  "b2182bd6604df4a582fddbc7ffffffe0e0e0bababa8787874d4d4d",
  "67001fb2182bd6604df4a582fddbc7e0e0e0bababa8787874d4d4d1a1a1a",
  "67001fb2182bd6604df4a582fddbc7ffffffe0e0e0bababa8787874d4d4d1a1a1a"
).map(colors);
const interpolateRdGy = ramp$1(scheme$l);
var scheme$k = new Array(3).concat(
  "fc8d59ffffbf91bfdb",
  "d7191cfdae61abd9e92c7bb6",
  "d7191cfdae61ffffbfabd9e92c7bb6",
  "d73027fc8d59fee090e0f3f891bfdb4575b4",
  "d73027fc8d59fee090ffffbfe0f3f891bfdb4575b4",
  "d73027f46d43fdae61fee090e0f3f8abd9e974add14575b4",
  "d73027f46d43fdae61fee090ffffbfe0f3f8abd9e974add14575b4",
  "a50026d73027f46d43fdae61fee090e0f3f8abd9e974add14575b4313695",
  "a50026d73027f46d43fdae61fee090ffffbfe0f3f8abd9e974add14575b4313695"
).map(colors);
const interpolateRdYlBu = ramp$1(scheme$k);
var scheme$j = new Array(3).concat(
  "fc8d59ffffbf91cf60",
  "d7191cfdae61a6d96a1a9641",
  "d7191cfdae61ffffbfa6d96a1a9641",
  "d73027fc8d59fee08bd9ef8b91cf601a9850",
  "d73027fc8d59fee08bffffbfd9ef8b91cf601a9850",
  "d73027f46d43fdae61fee08bd9ef8ba6d96a66bd631a9850",
  "d73027f46d43fdae61fee08bffffbfd9ef8ba6d96a66bd631a9850",
  "a50026d73027f46d43fdae61fee08bd9ef8ba6d96a66bd631a9850006837",
  "a50026d73027f46d43fdae61fee08bffffbfd9ef8ba6d96a66bd631a9850006837"
).map(colors);
const interpolateRdYlGn = ramp$1(scheme$j);
var scheme$i = new Array(3).concat(
  "fc8d59ffffbf99d594",
  "d7191cfdae61abdda42b83ba",
  "d7191cfdae61ffffbfabdda42b83ba",
  "d53e4ffc8d59fee08be6f59899d5943288bd",
  "d53e4ffc8d59fee08bffffbfe6f59899d5943288bd",
  "d53e4ff46d43fdae61fee08be6f598abdda466c2a53288bd",
  "d53e4ff46d43fdae61fee08bffffbfe6f598abdda466c2a53288bd",
  "9e0142d53e4ff46d43fdae61fee08be6f598abdda466c2a53288bd5e4fa2",
  "9e0142d53e4ff46d43fdae61fee08bffffbfe6f598abdda466c2a53288bd5e4fa2"
).map(colors);
const interpolateSpectral = ramp$1(scheme$i);
var scheme$h = new Array(3).concat(
  "e5f5f999d8c92ca25f",
  "edf8fbb2e2e266c2a4238b45",
  "edf8fbb2e2e266c2a42ca25f006d2c",
  "edf8fbccece699d8c966c2a42ca25f006d2c",
  "edf8fbccece699d8c966c2a441ae76238b45005824",
  "f7fcfde5f5f9ccece699d8c966c2a441ae76238b45005824",
  "f7fcfde5f5f9ccece699d8c966c2a441ae76238b45006d2c00441b"
).map(colors);
const interpolateBuGn = ramp$1(scheme$h);
var scheme$g = new Array(3).concat(
  "e0ecf49ebcda8856a7",
  "edf8fbb3cde38c96c688419d",
  "edf8fbb3cde38c96c68856a7810f7c",
  "edf8fbbfd3e69ebcda8c96c68856a7810f7c",
  "edf8fbbfd3e69ebcda8c96c68c6bb188419d6e016b",
  "f7fcfde0ecf4bfd3e69ebcda8c96c68c6bb188419d6e016b",
  "f7fcfde0ecf4bfd3e69ebcda8c96c68c6bb188419d810f7c4d004b"
).map(colors);
const interpolateBuPu = ramp$1(scheme$g);
var scheme$f = new Array(3).concat(
  "e0f3dba8ddb543a2ca",
  "f0f9e8bae4bc7bccc42b8cbe",
  "f0f9e8bae4bc7bccc443a2ca0868ac",
  "f0f9e8ccebc5a8ddb57bccc443a2ca0868ac",
  "f0f9e8ccebc5a8ddb57bccc44eb3d32b8cbe08589e",
  "f7fcf0e0f3dbccebc5a8ddb57bccc44eb3d32b8cbe08589e",
  "f7fcf0e0f3dbccebc5a8ddb57bccc44eb3d32b8cbe0868ac084081"
).map(colors);
const interpolateGnBu = ramp$1(scheme$f);
var scheme$e = new Array(3).concat(
  "fee8c8fdbb84e34a33",
  "fef0d9fdcc8afc8d59d7301f",
  "fef0d9fdcc8afc8d59e34a33b30000",
  "fef0d9fdd49efdbb84fc8d59e34a33b30000",
  "fef0d9fdd49efdbb84fc8d59ef6548d7301f990000",
  "fff7ecfee8c8fdd49efdbb84fc8d59ef6548d7301f990000",
  "fff7ecfee8c8fdd49efdbb84fc8d59ef6548d7301fb300007f0000"
).map(colors);
const interpolateOrRd = ramp$1(scheme$e);
var scheme$d = new Array(3).concat(
  "ece2f0a6bddb1c9099",
  "f6eff7bdc9e167a9cf02818a",
  "f6eff7bdc9e167a9cf1c9099016c59",
  "f6eff7d0d1e6a6bddb67a9cf1c9099016c59",
  "f6eff7d0d1e6a6bddb67a9cf3690c002818a016450",
  "fff7fbece2f0d0d1e6a6bddb67a9cf3690c002818a016450",
  "fff7fbece2f0d0d1e6a6bddb67a9cf3690c002818a016c59014636"
).map(colors);
const interpolatePuBuGn = ramp$1(scheme$d);
var scheme$c = new Array(3).concat(
  "ece7f2a6bddb2b8cbe",
  "f1eef6bdc9e174a9cf0570b0",
  "f1eef6bdc9e174a9cf2b8cbe045a8d",
  "f1eef6d0d1e6a6bddb74a9cf2b8cbe045a8d",
  "f1eef6d0d1e6a6bddb74a9cf3690c00570b0034e7b",
  "fff7fbece7f2d0d1e6a6bddb74a9cf3690c00570b0034e7b",
  "fff7fbece7f2d0d1e6a6bddb74a9cf3690c00570b0045a8d023858"
).map(colors);
const interpolatePuBu = ramp$1(scheme$c);
var scheme$b = new Array(3).concat(
  "e7e1efc994c7dd1c77",
  "f1eef6d7b5d8df65b0ce1256",
  "f1eef6d7b5d8df65b0dd1c77980043",
  "f1eef6d4b9dac994c7df65b0dd1c77980043",
  "f1eef6d4b9dac994c7df65b0e7298ace125691003f",
  "f7f4f9e7e1efd4b9dac994c7df65b0e7298ace125691003f",
  "f7f4f9e7e1efd4b9dac994c7df65b0e7298ace125698004367001f"
).map(colors);
const interpolatePuRd = ramp$1(scheme$b);
var scheme$a = new Array(3).concat(
  "fde0ddfa9fb5c51b8a",
  "feebe2fbb4b9f768a1ae017e",
  "feebe2fbb4b9f768a1c51b8a7a0177",
  "feebe2fcc5c0fa9fb5f768a1c51b8a7a0177",
  "feebe2fcc5c0fa9fb5f768a1dd3497ae017e7a0177",
  "fff7f3fde0ddfcc5c0fa9fb5f768a1dd3497ae017e7a0177",
  "fff7f3fde0ddfcc5c0fa9fb5f768a1dd3497ae017e7a017749006a"
).map(colors);
const interpolateRdPu = ramp$1(scheme$a);
var scheme$9 = new Array(3).concat(
  "edf8b17fcdbb2c7fb8",
  "ffffcca1dab441b6c4225ea8",
  "ffffcca1dab441b6c42c7fb8253494",
  "ffffccc7e9b47fcdbb41b6c42c7fb8253494",
  "ffffccc7e9b47fcdbb41b6c41d91c0225ea80c2c84",
  "ffffd9edf8b1c7e9b47fcdbb41b6c41d91c0225ea80c2c84",
  "ffffd9edf8b1c7e9b47fcdbb41b6c41d91c0225ea8253494081d58"
).map(colors);
const interpolateYlGnBu = ramp$1(scheme$9);
var scheme$8 = new Array(3).concat(
  "f7fcb9addd8e31a354",
  "ffffccc2e69978c679238443",
  "ffffccc2e69978c67931a354006837",
  "ffffccd9f0a3addd8e78c67931a354006837",
  "ffffccd9f0a3addd8e78c67941ab5d238443005a32",
  "ffffe5f7fcb9d9f0a3addd8e78c67941ab5d238443005a32",
  "ffffe5f7fcb9d9f0a3addd8e78c67941ab5d238443006837004529"
).map(colors);
const interpolateYlGn = ramp$1(scheme$8);
var scheme$7 = new Array(3).concat(
  "fff7bcfec44fd95f0e",
  "ffffd4fed98efe9929cc4c02",
  "ffffd4fed98efe9929d95f0e993404",
  "ffffd4fee391fec44ffe9929d95f0e993404",
  "ffffd4fee391fec44ffe9929ec7014cc4c028c2d04",
  "ffffe5fff7bcfee391fec44ffe9929ec7014cc4c028c2d04",
  "ffffe5fff7bcfee391fec44ffe9929ec7014cc4c02993404662506"
).map(colors);
const interpolateYlOrBr = ramp$1(scheme$7);
var scheme$6 = new Array(3).concat(
  "ffeda0feb24cf03b20",
  "ffffb2fecc5cfd8d3ce31a1c",
  "ffffb2fecc5cfd8d3cf03b20bd0026",
  "ffffb2fed976feb24cfd8d3cf03b20bd0026",
  "ffffb2fed976feb24cfd8d3cfc4e2ae31a1cb10026",
  "ffffccffeda0fed976feb24cfd8d3cfc4e2ae31a1cb10026",
  "ffffccffeda0fed976feb24cfd8d3cfc4e2ae31a1cbd0026800026"
).map(colors);
const interpolateYlOrRd = ramp$1(scheme$6);
var scheme$5 = new Array(3).concat(
  "deebf79ecae13182bd",
  "eff3ffbdd7e76baed62171b5",
  "eff3ffbdd7e76baed63182bd08519c",
  "eff3ffc6dbef9ecae16baed63182bd08519c",
  "eff3ffc6dbef9ecae16baed64292c62171b5084594",
  "f7fbffdeebf7c6dbef9ecae16baed64292c62171b5084594",
  "f7fbffdeebf7c6dbef9ecae16baed64292c62171b508519c08306b"
).map(colors);
const interpolateBlues = ramp$1(scheme$5);
var scheme$4 = new Array(3).concat(
  "e5f5e0a1d99b31a354",
  "edf8e9bae4b374c476238b45",
  "edf8e9bae4b374c47631a354006d2c",
  "edf8e9c7e9c0a1d99b74c47631a354006d2c",
  "edf8e9c7e9c0a1d99b74c47641ab5d238b45005a32",
  "f7fcf5e5f5e0c7e9c0a1d99b74c47641ab5d238b45005a32",
  "f7fcf5e5f5e0c7e9c0a1d99b74c47641ab5d238b45006d2c00441b"
).map(colors);
const interpolateGreens = ramp$1(scheme$4);
var scheme$3 = new Array(3).concat(
  "f0f0f0bdbdbd636363",
  "f7f7f7cccccc969696525252",
  "f7f7f7cccccc969696636363252525",
  "f7f7f7d9d9d9bdbdbd969696636363252525",
  "f7f7f7d9d9d9bdbdbd969696737373525252252525",
  "fffffff0f0f0d9d9d9bdbdbd969696737373525252252525",
  "fffffff0f0f0d9d9d9bdbdbd969696737373525252252525000000"
).map(colors);
const interpolateGreys = ramp$1(scheme$3);
var scheme$2 = new Array(3).concat(
  "efedf5bcbddc756bb1",
  "f2f0f7cbc9e29e9ac86a51a3",
  "f2f0f7cbc9e29e9ac8756bb154278f",
  "f2f0f7dadaebbcbddc9e9ac8756bb154278f",
  "f2f0f7dadaebbcbddc9e9ac8807dba6a51a34a1486",
  "fcfbfdefedf5dadaebbcbddc9e9ac8807dba6a51a34a1486",
  "fcfbfdefedf5dadaebbcbddc9e9ac8807dba6a51a354278f3f007d"
).map(colors);
const interpolatePurples = ramp$1(scheme$2);
var scheme$1 = new Array(3).concat(
  "fee0d2fc9272de2d26",
  "fee5d9fcae91fb6a4acb181d",
  "fee5d9fcae91fb6a4ade2d26a50f15",
  "fee5d9fcbba1fc9272fb6a4ade2d26a50f15",
  "fee5d9fcbba1fc9272fb6a4aef3b2ccb181d99000d",
  "fff5f0fee0d2fcbba1fc9272fb6a4aef3b2ccb181d99000d",
  "fff5f0fee0d2fcbba1fc9272fb6a4aef3b2ccb181da50f1567000d"
).map(colors);
const interpolateReds = ramp$1(scheme$1);
var scheme = new Array(3).concat(
  "fee6cefdae6be6550d",
  "feeddefdbe85fd8d3cd94701",
  "feeddefdbe85fd8d3ce6550da63603",
  "feeddefdd0a2fdae6bfd8d3ce6550da63603",
  "feeddefdd0a2fdae6bfd8d3cf16913d948018c2d04",
  "fff5ebfee6cefdd0a2fdae6bfd8d3cf16913d948018c2d04",
  "fff5ebfee6cefdd0a2fdae6bfd8d3cf16913d94801a636037f2704"
).map(colors);
const interpolateOranges = ramp$1(scheme);
function interpolateCividis(t) {
  t = Math.max(0, Math.min(1, t));
  return "rgb(" + Math.max(0, Math.min(255, Math.round(-4.54 - t * (35.34 - t * (2381.73 - t * (6402.7 - t * (7024.72 - t * 2710.57))))))) + ", " + Math.max(0, Math.min(255, Math.round(32.49 + t * (170.73 + t * (52.82 - t * (131.46 - t * (176.58 - t * 67.37))))))) + ", " + Math.max(0, Math.min(255, Math.round(81.24 + t * (442.36 - t * (2482.43 - t * (6167.24 - t * (6614.94 - t * 2475.67))))))) + ")";
}
const interpolateCubehelixDefault = cubehelixLong(cubehelix$1(300, 0.5, 0), cubehelix$1(-240, 0.5, 1));
var warm = cubehelixLong(cubehelix$1(-100, 0.75, 0.35), cubehelix$1(80, 1.5, 0.8));
var cool = cubehelixLong(cubehelix$1(260, 0.75, 0.35), cubehelix$1(80, 1.5, 0.8));
var c$2 = cubehelix$1();
function interpolateRainbow(t) {
  if (t < 0 || t > 1)
    t -= Math.floor(t);
  var ts = Math.abs(t - 0.5);
  c$2.h = 360 * t - 100;
  c$2.s = 1.5 - 1.5 * ts;
  c$2.l = 0.8 - 0.9 * ts;
  return c$2 + "";
}
var c$1 = rgb(), pi_1_3 = Math.PI / 3, pi_2_3 = Math.PI * 2 / 3;
function interpolateSinebow(t) {
  var x;
  t = (0.5 - t) * Math.PI;
  c$1.r = 255 * (x = Math.sin(t)) * x;
  c$1.g = 255 * (x = Math.sin(t + pi_1_3)) * x;
  c$1.b = 255 * (x = Math.sin(t + pi_2_3)) * x;
  return c$1 + "";
}
function interpolateTurbo(t) {
  t = Math.max(0, Math.min(1, t));
  return "rgb(" + Math.max(0, Math.min(255, Math.round(34.61 + t * (1172.33 - t * (10793.56 - t * (33300.12 - t * (38394.49 - t * 14825.05))))))) + ", " + Math.max(0, Math.min(255, Math.round(23.31 + t * (557.33 + t * (1225.33 - t * (3574.96 - t * (1073.77 + t * 707.56))))))) + ", " + Math.max(0, Math.min(255, Math.round(27.2 + t * (3211.1 - t * (15327.97 - t * (27814 - t * (22569.18 - t * 6838.66))))))) + ")";
}
function ramp(range2) {
  var n = range2.length;
  return function(t) {
    return range2[Math.max(0, Math.min(n - 1, Math.floor(t * n)))];
  };
}
const interpolateViridis = ramp(colors("44015444025645045745055946075a46085c460a5d460b5e470d60470e6147106347116447136548146748166848176948186a481a6c481b6d481c6e481d6f481f70482071482173482374482475482576482677482878482979472a7a472c7a472d7b472e7c472f7d46307e46327e46337f463480453581453781453882443983443a83443b84433d84433e85423f854240864241864142874144874045884046883f47883f48893e49893e4a893e4c8a3d4d8a3d4e8a3c4f8a3c508b3b518b3b528b3a538b3a548c39558c39568c38588c38598c375a8c375b8d365c8d365d8d355e8d355f8d34608d34618d33628d33638d32648e32658e31668e31678e31688e30698e306a8e2f6b8e2f6c8e2e6d8e2e6e8e2e6f8e2d708e2d718e2c718e2c728e2c738e2b748e2b758e2a768e2a778e2a788e29798e297a8e297b8e287c8e287d8e277e8e277f8e27808e26818e26828e26828e25838e25848e25858e24868e24878e23888e23898e238a8d228b8d228c8d228d8d218e8d218f8d21908d21918c20928c20928c20938c1f948c1f958b1f968b1f978b1f988b1f998a1f9a8a1e9b8a1e9c891e9d891f9e891f9f881fa0881fa1881fa1871fa28720a38620a48621a58521a68522a78522a88423a98324aa8325ab8225ac8226ad8127ad8128ae8029af7f2ab07f2cb17e2db27d2eb37c2fb47c31b57b32b67a34b67935b77937b87838b9773aba763bbb753dbc743fbc7340bd7242be7144bf7046c06f48c16e4ac16d4cc26c4ec36b50c46a52c56954c56856c66758c7655ac8645cc8635ec96260ca6063cb5f65cb5e67cc5c69cd5b6ccd5a6ece5870cf5773d05675d05477d1537ad1517cd2507fd34e81d34d84d44b86d54989d5488bd6468ed64590d74393d74195d84098d83e9bd93c9dd93ba0da39a2da37a5db36a8db34aadc32addc30b0dd2fb2dd2db5de2bb8de29bade28bddf26c0df25c2df23c5e021c8e020cae11fcde11dd0e11cd2e21bd5e21ad8e219dae319dde318dfe318e2e418e5e419e7e419eae51aece51befe51cf1e51df4e61ef6e620f8e621fbe723fde725"));
var magma = ramp(colors("00000401000501010601010802010902020b02020d03030f03031204041405041606051806051a07061c08071e0907200a08220b09240c09260d0a290e0b2b100b2d110c2f120d31130d34140e36150e38160f3b180f3d19103f1a10421c10441d11471e114920114b21114e22115024125325125527125829115a2a115c2c115f2d11612f116331116533106734106936106b38106c390f6e3b0f703d0f713f0f72400f74420f75440f764510774710784910784a10794c117a4e117b4f127b51127c52137c54137d56147d57157e59157e5a167e5c167f5d177f5f187f601880621980641a80651a80671b80681c816a1c816b1d816d1d816e1e81701f81721f817320817521817621817822817922827b23827c23827e24828025828125818326818426818627818827818928818b29818c29818e2a81902a81912b81932b80942c80962c80982d80992d809b2e7f9c2e7f9e2f7fa02f7fa1307ea3307ea5317ea6317da8327daa337dab337cad347cae347bb0357bb2357bb3367ab5367ab73779b83779ba3878bc3978bd3977bf3a77c03a76c23b75c43c75c53c74c73d73c83e73ca3e72cc3f71cd4071cf4070d0416fd2426fd3436ed5446dd6456cd8456cd9466bdb476adc4869de4968df4a68e04c67e24d66e34e65e44f64e55064e75263e85362e95462ea5661eb5760ec5860ed5a5fee5b5eef5d5ef05f5ef1605df2625df2645cf3655cf4675cf4695cf56b5cf66c5cf66e5cf7705cf7725cf8745cf8765cf9785df9795df97b5dfa7d5efa7f5efa815ffb835ffb8560fb8761fc8961fc8a62fc8c63fc8e64fc9065fd9266fd9467fd9668fd9869fd9a6afd9b6bfe9d6cfe9f6dfea16efea36ffea571fea772fea973feaa74feac76feae77feb078feb27afeb47bfeb67cfeb77efeb97ffebb81febd82febf84fec185fec287fec488fec68afec88cfeca8dfecc8ffecd90fecf92fed194fed395fed597fed799fed89afdda9cfddc9efddea0fde0a1fde2a3fde3a5fde5a7fde7a9fde9aafdebacfcecaefceeb0fcf0b2fcf2b4fcf4b6fcf6b8fcf7b9fcf9bbfcfbbdfcfdbf"));
var inferno = ramp(colors("00000401000501010601010802010a02020c02020e03021004031204031405041706041907051b08051d09061f0a07220b07240c08260d08290e092b10092d110a30120a32140b34150b37160b39180c3c190c3e1b0c411c0c431e0c451f0c48210c4a230c4c240c4f260c51280b53290b552b0b572d0b592f0a5b310a5c320a5e340a5f3609613809623909633b09643d09653e0966400a67420a68440a68450a69470b6a490b6a4a0c6b4c0c6b4d0d6c4f0d6c510e6c520e6d540f6d550f6d57106e59106e5a116e5c126e5d126e5f136e61136e62146e64156e65156e67166e69166e6a176e6c186e6d186e6f196e71196e721a6e741a6e751b6e771c6d781c6d7a1d6d7c1d6d7d1e6d7f1e6c801f6c82206c84206b85216b87216b88226a8a226a8c23698d23698f24699025689225689326679526679727669827669a28659b29649d29649f2a63a02a63a22b62a32c61a52c60a62d60a82e5fa92e5eab2f5ead305dae305cb0315bb1325ab3325ab43359b63458b73557b93556ba3655bc3754bd3853bf3952c03a51c13a50c33b4fc43c4ec63d4dc73e4cc83f4bca404acb4149cc4248ce4347cf4446d04545d24644d34743d44842d54a41d74b3fd84c3ed94d3dda4e3cdb503bdd513ade5238df5337e05536e15635e25734e35933e45a31e55c30e65d2fe75e2ee8602de9612bea632aeb6429eb6628ec6726ed6925ee6a24ef6c23ef6e21f06f20f1711ff1731df2741cf3761bf37819f47918f57b17f57d15f67e14f68013f78212f78410f8850ff8870ef8890cf98b0bf98c0af98e09fa9008fa9207fa9407fb9606fb9706fb9906fb9b06fb9d07fc9f07fca108fca309fca50afca60cfca80dfcaa0ffcac11fcae12fcb014fcb216fcb418fbb61afbb81dfbba1ffbbc21fbbe23fac026fac228fac42afac62df9c72ff9c932f9cb35f8cd37f8cf3af7d13df7d340f6d543f6d746f5d949f5db4cf4dd4ff4df53f4e156f3e35af3e55df2e661f2e865f2ea69f1ec6df1ed71f1ef75f1f179f2f27df2f482f3f586f3f68af4f88ef5f992f6fa96f8fb9af9fc9dfafda1fcffa4"));
var plasma = ramp(colors("0d088710078813078916078a19068c1b068d1d068e20068f2206902406912605912805922a05932c05942e05952f059631059733059735049837049938049a3a049a3c049b3e049c3f049c41049d43039e44039e46039f48039f4903a04b03a14c02a14e02a25002a25102a35302a35502a45601a45801a45901a55b01a55c01a65e01a66001a66100a76300a76400a76600a76700a86900a86a00a86c00a86e00a86f00a87100a87201a87401a87501a87701a87801a87a02a87b02a87d03a87e03a88004a88104a78305a78405a78606a68707a68808a68a09a58b0aa58d0ba58e0ca48f0da4910ea3920fa39410a29511a19613a19814a099159f9a169f9c179e9d189d9e199da01a9ca11b9ba21d9aa31e9aa51f99a62098a72197a82296aa2395ab2494ac2694ad2793ae2892b02991b12a90b22b8fb32c8eb42e8db52f8cb6308bb7318ab83289ba3388bb3488bc3587bd3786be3885bf3984c03a83c13b82c23c81c33d80c43e7fc5407ec6417dc7427cc8437bc9447aca457acb4679cc4778cc4977cd4a76ce4b75cf4c74d04d73d14e72d24f71d35171d45270d5536fd5546ed6556dd7566cd8576bd9586ada5a6ada5b69db5c68dc5d67dd5e66de5f65de6164df6263e06363e16462e26561e26660e3685fe4695ee56a5de56b5de66c5ce76e5be76f5ae87059e97158e97257ea7457eb7556eb7655ec7754ed7953ed7a52ee7b51ef7c51ef7e50f07f4ff0804ef1814df1834cf2844bf3854bf3874af48849f48948f58b47f58c46f68d45f68f44f79044f79143f79342f89441f89540f9973ff9983ef99a3efa9b3dfa9c3cfa9e3bfb9f3afba139fba238fca338fca537fca636fca835fca934fdab33fdac33fdae32fdaf31fdb130fdb22ffdb42ffdb52efeb72dfeb82cfeba2cfebb2bfebd2afebe2afec029fdc229fdc328fdc527fdc627fdc827fdca26fdcb26fccd25fcce25fcd025fcd225fbd324fbd524fbd724fad824fada24f9dc24f9dd25f8df25f8e125f7e225f7e425f6e626f6e826f5e926f5eb27f4ed27f3ee27f3f027f2f227f1f426f1f525f0f724f0f921"));
const cos = Math.cos;
const min = Math.min;
const sin = Math.sin;
const sqrt = Math.sqrt;
const pi$1 = Math.PI;
const tau$1 = 2 * pi$1;
const sqrt3$3 = sqrt(3);
const symbolAsterisk = {
  draw(context, size) {
    const r = sqrt(size + min(size / 28, 0.75)) * 0.59436;
    const t = r / 2;
    const u = t * sqrt3$3;
    context.moveTo(0, r);
    context.lineTo(0, -r);
    context.moveTo(-u, -t);
    context.lineTo(u, t);
    context.moveTo(-u, t);
    context.lineTo(u, -t);
  }
};
const symbolCircle = {
  draw(context, size) {
    const r = sqrt(size / pi$1);
    context.moveTo(r, 0);
    context.arc(0, 0, r, 0, tau$1);
  }
};
const symbolCross = {
  draw(context, size) {
    const r = sqrt(size / 5) / 2;
    context.moveTo(-3 * r, -r);
    context.lineTo(-r, -r);
    context.lineTo(-r, -3 * r);
    context.lineTo(r, -3 * r);
    context.lineTo(r, -r);
    context.lineTo(3 * r, -r);
    context.lineTo(3 * r, r);
    context.lineTo(r, r);
    context.lineTo(r, 3 * r);
    context.lineTo(-r, 3 * r);
    context.lineTo(-r, r);
    context.lineTo(-3 * r, r);
    context.closePath();
  }
};
const tan30 = sqrt(1 / 3);
const tan30_2 = tan30 * 2;
const symbolDiamond = {
  draw(context, size) {
    const y = sqrt(size / tan30_2);
    const x = y * tan30;
    context.moveTo(0, -y);
    context.lineTo(x, 0);
    context.lineTo(0, y);
    context.lineTo(-x, 0);
    context.closePath();
  }
};
const symbolDiamond2 = {
  draw(context, size) {
    const r = sqrt(size) * 0.62625;
    context.moveTo(0, -r);
    context.lineTo(r, 0);
    context.lineTo(0, r);
    context.lineTo(-r, 0);
    context.closePath();
  }
};
const symbolPlus = {
  draw(context, size) {
    const r = sqrt(size - min(size / 7, 2)) * 0.87559;
    context.moveTo(-r, 0);
    context.lineTo(r, 0);
    context.moveTo(0, r);
    context.lineTo(0, -r);
  }
};
const symbolSquare = {
  draw(context, size) {
    const w = sqrt(size);
    const x = -w / 2;
    context.rect(x, x, w, w);
  }
};
const symbolSquare2 = {
  draw(context, size) {
    const r = sqrt(size) * 0.4431;
    context.moveTo(r, r);
    context.lineTo(r, -r);
    context.lineTo(-r, -r);
    context.lineTo(-r, r);
    context.closePath();
  }
};
const ka = 0.8908130915292852;
const kr = sin(pi$1 / 10) / sin(7 * pi$1 / 10);
const kx = sin(tau$1 / 10) * kr;
const ky = -cos(tau$1 / 10) * kr;
const symbolStar = {
  draw(context, size) {
    const r = sqrt(size * ka);
    const x = kx * r;
    const y = ky * r;
    context.moveTo(0, -r);
    context.lineTo(x, y);
    for (let i = 1; i < 5; ++i) {
      const a2 = tau$1 * i / 5;
      const c2 = cos(a2);
      const s2 = sin(a2);
      context.lineTo(s2 * r, -c2 * r);
      context.lineTo(c2 * x - s2 * y, s2 * x + c2 * y);
    }
    context.closePath();
  }
};
const sqrt3$2 = sqrt(3);
const symbolTriangle = {
  draw(context, size) {
    const y = -sqrt(size / (sqrt3$2 * 3));
    context.moveTo(0, y * 2);
    context.lineTo(-sqrt3$2 * y, -y);
    context.lineTo(sqrt3$2 * y, -y);
    context.closePath();
  }
};
const sqrt3$1 = sqrt(3);
const symbolTriangle2 = {
  draw(context, size) {
    const s2 = sqrt(size) * 0.6824;
    const t = s2 / 2;
    const u = s2 * sqrt3$1 / 2;
    context.moveTo(0, -s2);
    context.lineTo(u, t);
    context.lineTo(-u, t);
    context.closePath();
  }
};
const c = -0.5;
const s = sqrt(3) / 2;
const k = 1 / sqrt(12);
const a = (k / 2 + 1) * 3;
const symbolWye = {
  draw(context, size) {
    const r = sqrt(size / a);
    const x02 = r / 2, y02 = r * k;
    const x12 = x02, y12 = r * k + r;
    const x2 = -x12, y2 = y12;
    context.moveTo(x02, y02);
    context.lineTo(x12, y12);
    context.lineTo(x2, y2);
    context.lineTo(c * x02 - s * y02, s * x02 + c * y02);
    context.lineTo(c * x12 - s * y12, s * x12 + c * y12);
    context.lineTo(c * x2 - s * y2, s * x2 + c * y2);
    context.lineTo(c * x02 + s * y02, c * y02 - s * x02);
    context.lineTo(c * x12 + s * y12, c * y12 - s * x12);
    context.lineTo(c * x2 + s * y2, c * y2 - s * x2);
    context.closePath();
  }
};
const symbolTimes = {
  draw(context, size) {
    const r = sqrt(size - min(size / 6, 1.7)) * 0.6189;
    context.moveTo(-r, -r);
    context.lineTo(r, r);
    context.moveTo(-r, r);
    context.lineTo(r, -r);
  }
};
const symbolsFill = [
  symbolCircle,
  symbolCross,
  symbolDiamond,
  symbolSquare,
  symbolStar,
  symbolTriangle,
  symbolWye
];
const symbolsStroke = [
  symbolCircle,
  symbolPlus,
  symbolTimes,
  symbolTriangle2,
  symbolAsterisk,
  symbolSquare2,
  symbolDiamond2
];
function Transform(k2, x, y) {
  this.k = k2;
  this.x = x;
  this.y = y;
}
Transform.prototype = {
  constructor: Transform,
  scale: function(k2) {
    return k2 === 1 ? this : new Transform(this.k * k2, this.x, this.y);
  },
  translate: function(x, y) {
    return x === 0 & y === 0 ? this : new Transform(this.k, this.x + this.k * x, this.y + this.k * y);
  },
  apply: function(point2) {
    return [point2[0] * this.k + this.x, point2[1] * this.k + this.y];
  },
  applyX: function(x) {
    return x * this.k + this.x;
  },
  applyY: function(y) {
    return y * this.k + this.y;
  },
  invert: function(location) {
    return [(location[0] - this.x) / this.k, (location[1] - this.y) / this.k];
  },
  invertX: function(x) {
    return (x - this.x) / this.k;
  },
  invertY: function(y) {
    return (y - this.y) / this.k;
  },
  rescaleX: function(x) {
    return x.copy().domain(x.range().map(this.invertX, this).map(x.invert, x));
  },
  rescaleY: function(y) {
    return y.copy().domain(y.range().map(this.invertY, this).map(y.invert, y));
  },
  toString: function() {
    return "translate(" + this.x + "," + this.y + ") scale(" + this.k + ")";
  }
};
Transform.prototype;
function format(date2, fallback) {
  if (!(date2 instanceof Date))
    date2 = new Date(+date2);
  if (isNaN(date2))
    return typeof fallback === "function" ? fallback(date2) : fallback;
  const hours = date2.getUTCHours();
  const minutes = date2.getUTCMinutes();
  const seconds = date2.getUTCSeconds();
  const milliseconds = date2.getUTCMilliseconds();
  return `${formatYear(date2.getUTCFullYear())}-${pad(date2.getUTCMonth() + 1, 2)}-${pad(date2.getUTCDate(), 2)}${hours || minutes || seconds || milliseconds ? `T${pad(hours, 2)}:${pad(minutes, 2)}${seconds || milliseconds ? `:${pad(seconds, 2)}${milliseconds ? `.${pad(milliseconds, 3)}` : ``}` : ``}Z` : ``}`;
}
function formatYear(year) {
  return year < 0 ? `-${pad(-year, 6)}` : year > 9999 ? `+${pad(year, 6)}` : pad(year, 4);
}
function pad(value, width) {
  return `${value}`.padStart(width, "0");
}
const re = /^(?:[-+]\d{2})?\d{4}(?:-\d{2}(?:-\d{2})?)?(?:T\d{2}:\d{2}(?::\d{2}(?:\.\d{3})?)?(?:Z|[-+]\d{2}:?\d{2})?)?$/;
function parse(string2, fallback) {
  if (!re.test(string2 += ""))
    return typeof fallback === "function" ? fallback(string2) : fallback;
  return new Date(string2);
}
const timeIntervals = /* @__PURE__ */ new Map([
  ["second", second$1],
  ["minute", timeMinute],
  ["hour", timeHour],
  ["day", timeDay],
  ["week", timeSunday],
  ["month", timeMonth],
  ["year", timeYear],
  ["monday", timeMonday],
  ["tuesday", timeTuesday],
  ["wednesday", timeWednesday],
  ["thursday", timeThursday],
  ["friday", timeFriday],
  ["saturday", timeSaturday],
  ["sunday", timeSunday]
]);
const utcIntervals = /* @__PURE__ */ new Map([
  ["second", second$1],
  ["minute", utcMinute],
  ["hour", utcHour],
  ["day", utcDay],
  ["week", utcSunday],
  ["month", utcMonth],
  ["year", utcYear],
  ["monday", utcMonday],
  ["tuesday", utcTuesday],
  ["wednesday", utcWednesday],
  ["thursday", utcThursday],
  ["friday", utcFriday],
  ["saturday", utcSaturday],
  ["sunday", utcSunday]
]);
function maybeTimeInterval(interval2) {
  const i = timeIntervals.get(`${interval2}`.toLowerCase());
  if (!i)
    throw new Error(`unknown interval: ${interval2}`);
  return i;
}
function maybeUtcInterval(interval2) {
  const i = utcIntervals.get(`${interval2}`.toLowerCase());
  if (!i)
    throw new Error(`unknown interval: ${interval2}`);
  return i;
}
const TypedArray = Object.getPrototypeOf(Uint8Array);
const objectToString = Object.prototype.toString;
function valueof(data, value, type) {
  const valueType = typeof value;
  return valueType === "string" ? map(data, field(value), type) : valueType === "function" ? map(data, value, type) : valueType === "number" || value instanceof Date || valueType === "boolean" ? map(data, constant(value), type) : value && typeof value.transform === "function" ? arrayify(value.transform(data), type) : arrayify(value, type);
}
const field = (name) => (d) => d[name];
const indexOf = (d, i) => i;
const identity$1 = { transform: (d) => d };
const one = () => 1;
const yes = () => true;
const string = (x) => x == null ? x : `${x}`;
const number = (x) => x == null ? x : +x;
const first = (x) => x ? x[0] : void 0;
const second = (x) => x ? x[1] : void 0;
const constant = (x) => () => x;
function percentile(reduce) {
  const p = +`${reduce}`.slice(1) / 100;
  return (I, f) => quantile$1(I, p, f);
}
function maybeColorChannel(value, defaultValue) {
  if (value === void 0)
    value = defaultValue;
  return value === null ? [void 0, "none"] : isColor(value) ? [void 0, value] : [value, void 0];
}
function maybeNumberChannel(value, defaultValue) {
  if (value === void 0)
    value = defaultValue;
  return value === null || typeof value === "number" ? [void 0, value] : [value, void 0];
}
function maybeKeyword(input, name, allowed) {
  if (input != null)
    return keyword(input, name, allowed);
}
function keyword(input, name, allowed) {
  const i = `${input}`.toLowerCase();
  if (!allowed.includes(i))
    throw new Error(`invalid ${name}: ${input}`);
  return i;
}
function arrayify(data, type) {
  return data == null ? data : type === void 0 ? data instanceof Array || data instanceof TypedArray ? data : Array.from(data) : data instanceof type ? data : type.from(data);
}
function map(values2, f, type = Array) {
  return values2 instanceof type ? values2.map(f) : type.from(values2, f);
}
function slice(values2, type = Array) {
  return values2 instanceof type ? values2.slice() : type.from(values2);
}
function isTypedArray(values2) {
  return values2 instanceof TypedArray;
}
function isObject(option) {
  return (option == null ? void 0 : option.toString) === objectToString;
}
function isScaleOptions(option) {
  return isObject(option) && (option.type !== void 0 || option.domain !== void 0);
}
function isOptions(option) {
  return isObject(option) && typeof option.transform !== "function";
}
function isDomainSort(sort2) {
  return isOptions(sort2) && sort2.value === void 0 && sort2.channel === void 0;
}
function maybeZero(x, x12, x2, x3 = identity$1) {
  if (x12 === void 0 && x2 === void 0) {
    x12 = 0, x2 = x === void 0 ? x3 : x;
  } else if (x12 === void 0) {
    x12 = x === void 0 ? 0 : x;
  } else if (x2 === void 0) {
    x2 = x === void 0 ? 0 : x;
  }
  return [x12, x2];
}
function maybeTuple(x, y) {
  return x === void 0 && y === void 0 ? [first, second] : [x, y];
}
function maybeZ({ z, fill, stroke } = {}) {
  if (z === void 0)
    [z] = maybeColorChannel(fill);
  if (z === void 0)
    [z] = maybeColorChannel(stroke);
  return z;
}
function range(data) {
  const n = data.length;
  const r = new Uint32Array(n);
  for (let i = 0; i < n; ++i)
    r[i] = i;
  return r;
}
function take(values2, index2) {
  return map(index2, (i) => values2[i]);
}
function column(source) {
  let value;
  return [
    {
      transform: () => value,
      label: labelof(source)
    },
    (v) => value = v
  ];
}
function maybeColumn(source) {
  return source == null ? [source] : column(source);
}
function labelof(value, defaultValue) {
  return typeof value === "string" ? value : value && value.label !== void 0 ? value.label : defaultValue;
}
function mid$1(x12, x2) {
  return {
    transform(data) {
      const X12 = x12.transform(data);
      const X22 = x2.transform(data);
      return isTemporal(X12) || isTemporal(X22) ? map(X12, (_, i) => new Date((+X12[i] + +X22[i]) / 2)) : map(X12, (_, i) => (+X12[i] + +X22[i]) / 2, Float64Array);
    },
    label: x12.label
  };
}
function maybeInterval(interval2, type) {
  if (interval2 == null)
    return;
  if (typeof interval2 === "number") {
    const n = interval2;
    return {
      floor: (d) => n * Math.floor(d / n),
      offset: (d) => d + n,
      range: (lo, hi) => range$1(Math.ceil(lo / n), hi / n).map((x) => n * x)
    };
  }
  if (typeof interval2 === "string")
    return (type === "time" ? maybeTimeInterval : maybeUtcInterval)(interval2);
  if (typeof interval2.floor !== "function")
    throw new Error("invalid interval; missing floor method");
  if (typeof interval2.offset !== "function")
    throw new Error("invalid interval; missing offset method");
  return interval2;
}
function maybeValue(value) {
  return value === void 0 || isOptions(value) ? value : { value };
}
function numberChannel(source) {
  return source == null ? null : {
    transform: (data) => valueof(data, source, Float64Array),
    label: labelof(source)
  };
}
function isIterable(value) {
  return value && typeof value[Symbol.iterator] === "function";
}
function isTextual(values2) {
  for (const value of values2) {
    if (value == null)
      continue;
    return typeof value !== "object" || value instanceof Date;
  }
}
function isOrdinal(values2) {
  for (const value of values2) {
    if (value == null)
      continue;
    const type = typeof value;
    return type === "string" || type === "boolean";
  }
}
function isTemporal(values2) {
  for (const value of values2) {
    if (value == null)
      continue;
    return value instanceof Date;
  }
}
function isTemporalString(values2) {
  for (const value of values2) {
    if (value == null)
      continue;
    return typeof value === "string" && isNaN(value) && parse(value);
  }
}
function isNumericString(values2) {
  for (const value of values2) {
    if (value == null)
      continue;
    if (typeof value !== "string")
      return false;
    if (!value.trim())
      continue;
    return !isNaN(value);
  }
}
function isNumeric(values2) {
  for (const value of values2) {
    if (value == null)
      continue;
    return typeof value === "number";
  }
}
function isEvery(values2, is) {
  let every;
  for (const value of values2) {
    if (value == null)
      continue;
    if (!is(value))
      return false;
    every = true;
  }
  return every;
}
function isColor(value) {
  if (typeof value !== "string")
    return false;
  value = value.toLowerCase().trim();
  return value === "none" || value === "currentcolor" || value.startsWith("url(") && value.endsWith(")") || // <funciri>, e.g. pattern or gradient
  value.startsWith("var(") && value.endsWith(")") || // CSS variable
  color$1(value) !== null;
}
function isNoneish(value) {
  return value == null || isNone(value);
}
function isNone(value) {
  return /^\s*none\s*$/i.test(value);
}
function isRound(value) {
  return /^\s*round\s*$/i.test(value);
}
function maybeFrameAnchor(value = "middle") {
  return keyword(value, "frameAnchor", [
    "middle",
    "top-left",
    "top",
    "top-right",
    "right",
    "bottom-right",
    "bottom",
    "bottom-left",
    "left"
  ]);
}
function orderof(values2) {
  if (values2 == null)
    return;
  const first2 = values2[0];
  const last = values2[values2.length - 1];
  return descending(first2, last);
}
function inherit(options = {}, ...rest) {
  let o = options;
  for (const defaults2 of rest) {
    for (const key in defaults2) {
      if (o[key] === void 0) {
        const value = defaults2[key];
        if (o === options)
          o = { ...o, [key]: value };
        else
          o[key] = value;
      }
    }
  }
  return o;
}
function Named(things) {
  console.warn("named iterables are deprecated; please use an object instead");
  const names = /* @__PURE__ */ new Set();
  return Object.fromEntries(Array.from(things, (thing) => {
    const { name } = thing;
    if (name == null)
      throw new Error("missing name");
    const key = `${name}`;
    if (key === "__proto__")
      throw new Error(`illegal name: ${key}`);
    if (names.has(key))
      throw new Error(`duplicate name: ${key}`);
    names.add(key);
    return [name, thing];
  }));
}
function maybeNamed(things) {
  return isIterable(things) ? Named(things) : things;
}
const position = Symbol("position");
const color = Symbol("color");
const radius = Symbol("radius");
const length = Symbol("length");
const opacity = Symbol("opacity");
const symbol = Symbol("symbol");
const registry = /* @__PURE__ */ new Map([
  ["x", position],
  ["y", position],
  ["fx", position],
  ["fy", position],
  ["r", radius],
  ["color", color],
  ["opacity", opacity],
  ["symbol", symbol],
  ["length", length]
]);
const sqrt3 = Math.sqrt(3);
const sqrt4_3 = 2 / sqrt3;
const symbolHexagon = {
  draw(context, size) {
    const rx = Math.sqrt(size / Math.PI), ry = rx * sqrt4_3, hy = ry / 2;
    context.moveTo(0, ry);
    context.lineTo(rx, hy);
    context.lineTo(rx, -hy);
    context.lineTo(0, -ry);
    context.lineTo(-rx, -hy);
    context.lineTo(-rx, hy);
    context.closePath();
  }
};
const symbols = /* @__PURE__ */ new Map([
  ["asterisk", symbolAsterisk],
  ["circle", symbolCircle],
  ["cross", symbolCross],
  ["diamond", symbolDiamond],
  ["diamond2", symbolDiamond2],
  ["hexagon", symbolHexagon],
  ["plus", symbolPlus],
  ["square", symbolSquare],
  ["square2", symbolSquare2],
  ["star", symbolStar],
  ["times", symbolTimes],
  ["triangle", symbolTriangle],
  ["triangle2", symbolTriangle2],
  ["wye", symbolWye]
]);
function isSymbolObject(value) {
  return value && typeof value.draw === "function";
}
function isSymbol(value) {
  if (isSymbolObject(value))
    return true;
  if (typeof value !== "string")
    return false;
  return symbols.has(value.toLowerCase());
}
function maybeSymbol(symbol2) {
  if (symbol2 == null || isSymbolObject(symbol2))
    return symbol2;
  const value = symbols.get(`${symbol2}`.toLowerCase());
  if (value)
    return value;
  throw new Error(`invalid symbol: ${symbol2}`);
}
function maybeSymbolChannel(symbol2) {
  if (symbol2 == null || isSymbolObject(symbol2))
    return [void 0, symbol2];
  if (typeof symbol2 === "string") {
    const value = symbols.get(`${symbol2}`.toLowerCase());
    if (value)
      return [void 0, value];
  }
  return [symbol2, void 0];
}
function defined(x) {
  return x != null && !Number.isNaN(x);
}
function ascendingDefined(a2, b) {
  return +defined(b) - +defined(a2) || ascending$1(a2, b);
}
function descendingDefined(a2, b) {
  return +defined(b) - +defined(a2) || descending(a2, b);
}
function nonempty(x) {
  return x != null && `${x}` !== "";
}
function finite(x) {
  return isFinite(x) ? x : NaN;
}
function positive(x) {
  return x > 0 && isFinite(x) ? x : NaN;
}
function negative(x) {
  return x < 0 && isFinite(x) ? x : NaN;
}
function basic(options = {}, transform) {
  let { filter: f1, sort: s1, reverse: r1, transform: t12, initializer: i1, ...remainingOptions } = options;
  if (t12 === void 0) {
    if (f1 != null)
      t12 = filterTransform(f1);
    if (s1 != null && !isDomainSort(s1))
      t12 = composeTransform(t12, sortTransform(s1));
    if (r1)
      t12 = composeTransform(t12, reverseTransform);
  }
  if (transform != null && i1 != null)
    throw new Error("transforms cannot be applied after initializers");
  return {
    ...remainingOptions,
    ...(s1 === null || isDomainSort(s1)) && { sort: s1 },
    transform: composeTransform(t12, transform)
  };
}
function initializer(options = {}, initializer2) {
  let { filter: f1, sort: s1, reverse: r1, initializer: i1, ...remainingOptions } = options;
  if (i1 === void 0) {
    if (f1 != null)
      i1 = filterTransform(f1);
    if (s1 != null && !isDomainSort(s1))
      i1 = composeInitializer(i1, sortTransform(s1));
    if (r1)
      i1 = composeInitializer(i1, reverseTransform);
  }
  return {
    ...remainingOptions,
    ...(s1 === null || isDomainSort(s1)) && { sort: s1 },
    initializer: composeInitializer(i1, initializer2)
  };
}
function composeTransform(t12, t22) {
  if (t12 == null)
    return t22 === null ? void 0 : t22;
  if (t22 == null)
    return t12 === null ? void 0 : t12;
  return function(data, facets) {
    ({ data, facets } = t12.call(this, data, facets));
    return t22.call(this, arrayify(data), facets);
  };
}
function composeInitializer(i1, i2) {
  if (i1 == null)
    return i2 === null ? void 0 : i2;
  if (i2 == null)
    return i1 === null ? void 0 : i1;
  return function(data, facets, channels, ...args) {
    let c1, d1, f1, c2, d2, f2;
    ({ data: d1 = data, facets: f1 = facets, channels: c1 } = i1.call(this, data, facets, channels, ...args));
    ({ data: d2 = d1, facets: f2 = f1, channels: c2 } = i2.call(this, d1, f1, { ...channels, ...c1 }, ...args));
    return { data: d2, facets: f2, channels: { ...c1, ...c2 } };
  };
}
function apply(options, t) {
  return (options.initializer != null ? initializer : basic)(options, t);
}
function filterTransform(value) {
  return (data, facets) => {
    const V = valueof(data, value);
    return { data, facets: facets.map((I) => I.filter((i) => V[i])) };
  };
}
function reverseTransform(data, facets) {
  return { data, facets: facets.map((I) => I.slice().reverse()) };
}
function sort(order, options) {
  return {
    ...(isOptions(order) && order.channel !== void 0 ? initializer : apply)(options, sortTransform(order)),
    sort: null
  };
}
function sortTransform(value) {
  return (typeof value === "function" && value.length !== 1 ? sortData : sortValue)(value);
}
function sortData(compare) {
  return (data, facets) => {
    const compareData = (i, j) => compare(data[i], data[j]);
    return { data, facets: facets.map((I) => I.slice().sort(compareData)) };
  };
}
function sortValue(value) {
  let channel, order;
  ({ channel, value, order = ascendingDefined } = { ...maybeValue(value) });
  if (typeof order !== "function") {
    switch (`${order}`.toLowerCase()) {
      case "ascending":
        order = ascendingDefined;
        break;
      case "descending":
        order = descendingDefined;
        break;
      default:
        throw new Error(`invalid order: ${order}`);
    }
  }
  return (data, facets, channels) => {
    let V;
    if (channel === void 0) {
      V = valueof(data, value);
    } else {
      if (channels === void 0)
        throw new Error("channel sort requires an initializer");
      V = channels[channel];
      if (!V)
        return {};
      V = V.value;
    }
    const compareValue = (i, j) => order(V[i], V[j]);
    return { data, facets: facets.map((I) => I.slice().sort(compareValue)) };
  };
}
function maybeReduce(reduce, value) {
  if (typeof (reduce == null ? void 0 : reduce.reduce) === "function" && isObject(reduce))
    return reduce;
  if (typeof reduce === "function")
    return reduceFunction(reduce);
  if (/^p\d{2}$/i.test(reduce))
    return reduceAccessor(percentile(reduce));
  switch (`${reduce}`.toLowerCase()) {
    case "first":
      return reduceFirst;
    case "last":
      return reduceLast;
    case "count":
      return reduceCount;
    case "distinct":
      return reduceDistinct;
    case "sum":
      return value == null ? reduceCount : reduceSum;
    case "proportion":
      return reduceProportion(value, "data");
    case "proportion-facet":
      return reduceProportion(value, "facet");
    case "deviation":
      return reduceAccessor(deviation);
    case "min":
      return reduceAccessor(min$1);
    case "min-index":
      return reduceAccessor(minIndex);
    case "max":
      return reduceAccessor(max);
    case "max-index":
      return reduceAccessor(maxIndex);
    case "mean":
      return reduceMaybeTemporalAccessor(mean);
    case "median":
      return reduceMaybeTemporalAccessor(median);
    case "variance":
      return reduceAccessor(variance);
    case "mode":
      return reduceAccessor(mode);
    case "x":
      return reduceX;
    case "x1":
      return reduceX1;
    case "x2":
      return reduceX2;
    case "y":
      return reduceY;
    case "y1":
      return reduceY1;
    case "y2":
      return reduceY2;
  }
  throw new Error(`invalid reduce: ${reduce}`);
}
function reduceFunction(f) {
  return {
    reduce(I, X, extent2) {
      return f(take(X, I), extent2);
    }
  };
}
function reduceAccessor(f) {
  return {
    reduce(I, X) {
      return f(I, (i) => X[i]);
    }
  };
}
function reduceMaybeTemporalAccessor(f) {
  return {
    reduce(I, X) {
      const x = f(I, (i) => X[i]);
      return isTemporal(X) ? new Date(x) : x;
    }
  };
}
const reduceFirst = {
  reduce(I, X) {
    return X[I[0]];
  }
};
const reduceLast = {
  reduce(I, X) {
    return X[I[I.length - 1]];
  }
};
const reduceCount = {
  label: "Frequency",
  reduce(I) {
    return I.length;
  }
};
const reduceDistinct = {
  label: "Distinct",
  reduce: (I, X) => {
    const s2 = new InternSet();
    for (const i of I)
      s2.add(X[i]);
    return s2.size;
  }
};
const reduceSum = reduceAccessor(sum);
function reduceProportion(value, scope) {
  return value == null ? { scope, label: "Frequency", reduce: (I, V, basis2 = 1) => I.length / basis2 } : { scope, reduce: (I, V, basis2 = 1) => sum(I, (i) => V[i]) / basis2 };
}
function mid(x12, x2) {
  const m = (+x12 + +x2) / 2;
  return x12 instanceof Date ? new Date(m) : m;
}
const reduceX = {
  reduce(I, X, { x1: x12, x2 }) {
    return mid(x12, x2);
  }
};
const reduceY = {
  reduce(I, X, { y1: y12, y2 }) {
    return mid(y12, y2);
  }
};
const reduceX1 = {
  reduce(I, X, { x1: x12 }) {
    return x12;
  }
};
const reduceX2 = {
  reduce(I, X, { x2 }) {
    return x2;
  }
};
const reduceY1 = {
  reduce(I, X, { y1: y12 }) {
    return y12;
  }
};
const reduceY2 = {
  reduce(I, X, { y2 }) {
    return y2;
  }
};
function Channel(data, { scale, type, value, filter: filter2, hint }, name) {
  return inferChannelScale(name, {
    scale,
    type,
    value: valueof(data, value),
    label: labelof(value),
    filter: filter2,
    hint
  });
}
function Channels(channels, data) {
  return Object.fromEntries(Object.entries(channels).map(([name, channel]) => [name, Channel(data, channel, name)]));
}
function valueObject(channels, scales) {
  return Object.fromEntries(Object.entries(channels).map(([name, { scale: scaleName, value }]) => {
    const scale = scaleName == null ? null : scales[scaleName];
    return [name, scale == null ? value : map(value, scale)];
  }));
}
function inferChannelScale(name, channel) {
  const { scale, value } = channel;
  if (scale === true || scale === "auto") {
    switch (name) {
      case "fill":
      case "stroke":
      case "color":
        channel.scale = isEvery(value, isColor) ? null : "color";
        break;
      case "fillOpacity":
      case "strokeOpacity":
        channel.scale = "opacity";
        break;
      case "symbol":
        if (isEvery(value, isSymbol)) {
          channel.scale = null;
          channel.value = map(value, maybeSymbol);
        } else {
          channel.scale = "symbol";
        }
        break;
      default:
        channel.scale = registry.has(name) ? name : null;
        break;
    }
  } else if (scale != null && !registry.has(scale)) {
    throw new Error(`unknown scale: ${scale}`);
  }
  return channel;
}
function channelDomain(channels, facetChannels, data, options) {
  const { reverse: defaultReverse, reduce: defaultReduce = true, limit: defaultLimit } = options;
  for (const x in options) {
    if (!registry.has(x))
      continue;
    let { value: y, reverse: reverse2 = defaultReverse, reduce = defaultReduce, limit = defaultLimit } = maybeValue(options[x]);
    if (reverse2 === void 0)
      reverse2 = y === "width" || y === "height";
    if (reduce == null || reduce === false)
      continue;
    const X = findScaleChannel(channels, x) || facetChannels && findScaleChannel(facetChannels, x);
    if (!X)
      throw new Error(`missing channel for scale: ${x}`);
    const XV = X.value;
    const [lo = 0, hi = Infinity] = isIterable(limit) ? limit : limit < 0 ? [limit] : [0, limit];
    if (y == null) {
      X.domain = () => {
        let domain = XV;
        if (reverse2)
          domain = domain.slice().reverse();
        if (lo !== 0 || hi !== Infinity)
          domain = domain.slice(lo, hi);
        return domain;
      };
    } else {
      const YV = y === "data" ? data : y === "height" ? difference(channels, "y1", "y2") : y === "width" ? difference(channels, "x1", "x2") : values(channels, y, y === "y" ? "y2" : y === "x" ? "x2" : void 0);
      const reducer2 = maybeReduce(reduce === true ? "max" : reduce, YV);
      X.domain = () => {
        let domain = rollup(range(XV), (I) => reducer2.reduce(I, YV), (i) => XV[i]);
        domain = sort$1(domain, reverse2 ? descendingGroup : ascendingGroup);
        if (lo !== 0 || hi !== Infinity)
          domain = domain.slice(lo, hi);
        return domain.map(first);
      };
    }
  }
}
function findScaleChannel(channels, scale) {
  for (const name in channels) {
    const channel = channels[name];
    if (channel.scale === scale)
      return channel;
  }
}
function difference(channels, k1, k2) {
  const X12 = values(channels, k1);
  const X22 = values(channels, k2);
  return map(X22, (x2, i) => Math.abs(x2 - X12[i]), Float64Array);
}
function values(channels, name, alias) {
  let channel = channels[name];
  if (!channel && alias !== void 0)
    channel = channels[alias];
  if (channel)
    return channel.value;
  throw new Error(`missing channel: ${name}`);
}
function ascendingGroup([ak, av], [bk, bv]) {
  return ascending$1(av, bv) || ascending$1(ak, bk);
}
function descendingGroup([ak, av], [bk, bv]) {
  return descending(av, bv) || ascending$1(ak, bk);
}
const ordinalSchemes = /* @__PURE__ */ new Map([
  // categorical
  ["accent", schemeAccent],
  ["category10", schemeCategory10],
  ["dark2", schemeDark2],
  ["paired", schemePaired],
  ["pastel1", schemePastel1],
  ["pastel2", schemePastel2],
  ["set1", schemeSet1],
  ["set2", schemeSet2],
  ["set3", schemeSet3],
  ["tableau10", schemeTableau10],
  // diverging
  ["brbg", scheme11(scheme$q, interpolateBrBG)],
  ["prgn", scheme11(scheme$p, interpolatePRGn)],
  ["piyg", scheme11(scheme$o, interpolatePiYG)],
  ["puor", scheme11(scheme$n, interpolatePuOr)],
  ["rdbu", scheme11(scheme$m, interpolateRdBu)],
  ["rdgy", scheme11(scheme$l, interpolateRdGy)],
  ["rdylbu", scheme11(scheme$k, interpolateRdYlBu)],
  ["rdylgn", scheme11(scheme$j, interpolateRdYlGn)],
  ["spectral", scheme11(scheme$i, interpolateSpectral)],
  // reversed diverging (for temperature data)
  ["burd", scheme11r(scheme$m, interpolateRdBu)],
  ["buylrd", scheme11r(scheme$k, interpolateRdYlBu)],
  // sequential (single-hue)
  ["blues", scheme9(scheme$5, interpolateBlues)],
  ["greens", scheme9(scheme$4, interpolateGreens)],
  ["greys", scheme9(scheme$3, interpolateGreys)],
  ["oranges", scheme9(scheme, interpolateOranges)],
  ["purples", scheme9(scheme$2, interpolatePurples)],
  ["reds", scheme9(scheme$1, interpolateReds)],
  // sequential (multi-hue)
  ["turbo", schemei(interpolateTurbo)],
  ["viridis", schemei(interpolateViridis)],
  ["magma", schemei(magma)],
  ["inferno", schemei(inferno)],
  ["plasma", schemei(plasma)],
  ["cividis", schemei(interpolateCividis)],
  ["cubehelix", schemei(interpolateCubehelixDefault)],
  ["warm", schemei(warm)],
  ["cool", schemei(cool)],
  ["bugn", scheme9(scheme$h, interpolateBuGn)],
  ["bupu", scheme9(scheme$g, interpolateBuPu)],
  ["gnbu", scheme9(scheme$f, interpolateGnBu)],
  ["orrd", scheme9(scheme$e, interpolateOrRd)],
  ["pubu", scheme9(scheme$c, interpolatePuBu)],
  ["pubugn", scheme9(scheme$d, interpolatePuBuGn)],
  ["purd", scheme9(scheme$b, interpolatePuRd)],
  ["rdpu", scheme9(scheme$a, interpolateRdPu)],
  ["ylgn", scheme9(scheme$8, interpolateYlGn)],
  ["ylgnbu", scheme9(scheme$9, interpolateYlGnBu)],
  ["ylorbr", scheme9(scheme$7, interpolateYlOrBr)],
  ["ylorrd", scheme9(scheme$6, interpolateYlOrRd)],
  // cyclical
  ["rainbow", schemeicyclical(interpolateRainbow)],
  ["sinebow", schemeicyclical(interpolateSinebow)]
]);
function scheme9(scheme2, interpolate2) {
  return ({ length: n }) => {
    if (n === 1)
      return [scheme2[3][1]];
    if (n === 2)
      return [scheme2[3][1], scheme2[3][2]];
    n = Math.max(3, Math.floor(n));
    return n > 9 ? quantize(interpolate2, n) : scheme2[n];
  };
}
function scheme11(scheme2, interpolate2) {
  return ({ length: n }) => {
    if (n === 2)
      return [scheme2[3][0], scheme2[3][2]];
    n = Math.max(3, Math.floor(n));
    return n > 11 ? quantize(interpolate2, n) : scheme2[n];
  };
}
function scheme11r(scheme2, interpolate2) {
  return ({ length: n }) => {
    if (n === 2)
      return [scheme2[3][2], scheme2[3][0]];
    n = Math.max(3, Math.floor(n));
    return n > 11 ? quantize((t) => interpolate2(1 - t), n) : scheme2[n].slice().reverse();
  };
}
function schemei(interpolate2) {
  return ({ length: n }) => quantize(interpolate2, Math.max(2, Math.floor(n)));
}
function schemeicyclical(interpolate2) {
  return ({ length: n }) => quantize(interpolate2, Math.floor(n) + 1).slice(0, -1);
}
function ordinalScheme(scheme2) {
  const s2 = `${scheme2}`.toLowerCase();
  if (!ordinalSchemes.has(s2))
    throw new Error(`unknown ordinal scheme: ${s2}`);
  return ordinalSchemes.get(s2);
}
function ordinalRange(scheme2, length2) {
  const s2 = ordinalScheme(scheme2);
  const r = typeof s2 === "function" ? s2({ length: length2 }) : s2;
  return r.length !== length2 ? r.slice(0, length2) : r;
}
function maybeBooleanRange(domain, scheme2 = "greys") {
  const range2 = /* @__PURE__ */ new Set();
  const [f, t] = ordinalRange(scheme2, 2);
  for (const value of domain) {
    if (value == null)
      continue;
    if (value === true)
      range2.add(t);
    else if (value === false)
      range2.add(f);
    else
      return;
  }
  return [...range2];
}
const quantitativeSchemes = /* @__PURE__ */ new Map([
  // diverging
  ["brbg", interpolateBrBG],
  ["prgn", interpolatePRGn],
  ["piyg", interpolatePiYG],
  ["puor", interpolatePuOr],
  ["rdbu", interpolateRdBu],
  ["rdgy", interpolateRdGy],
  ["rdylbu", interpolateRdYlBu],
  ["rdylgn", interpolateRdYlGn],
  ["spectral", interpolateSpectral],
  // reversed diverging (for temperature data)
  ["burd", (t) => interpolateRdBu(1 - t)],
  ["buylrd", (t) => interpolateRdYlBu(1 - t)],
  // sequential (single-hue)
  ["blues", interpolateBlues],
  ["greens", interpolateGreens],
  ["greys", interpolateGreys],
  ["purples", interpolatePurples],
  ["reds", interpolateReds],
  ["oranges", interpolateOranges],
  // sequential (multi-hue)
  ["turbo", interpolateTurbo],
  ["viridis", interpolateViridis],
  ["magma", magma],
  ["inferno", inferno],
  ["plasma", plasma],
  ["cividis", interpolateCividis],
  ["cubehelix", interpolateCubehelixDefault],
  ["warm", warm],
  ["cool", cool],
  ["bugn", interpolateBuGn],
  ["bupu", interpolateBuPu],
  ["gnbu", interpolateGnBu],
  ["orrd", interpolateOrRd],
  ["pubugn", interpolatePuBuGn],
  ["pubu", interpolatePuBu],
  ["purd", interpolatePuRd],
  ["rdpu", interpolateRdPu],
  ["ylgnbu", interpolateYlGnBu],
  ["ylgn", interpolateYlGn],
  ["ylorbr", interpolateYlOrBr],
  ["ylorrd", interpolateYlOrRd],
  // cyclical
  ["rainbow", interpolateRainbow],
  ["sinebow", interpolateSinebow]
]);
function quantitativeScheme(scheme2) {
  const s2 = `${scheme2}`.toLowerCase();
  if (!quantitativeSchemes.has(s2))
    throw new Error(`unknown quantitative scheme: ${s2}`);
  return quantitativeSchemes.get(s2);
}
const divergingSchemes = /* @__PURE__ */ new Set([
  "brbg",
  "prgn",
  "piyg",
  "puor",
  "rdbu",
  "rdgy",
  "rdylbu",
  "rdylgn",
  "spectral",
  "burd",
  "buylrd"
]);
function isDivergingScheme(scheme2) {
  return scheme2 != null && divergingSchemes.has(`${scheme2}`.toLowerCase());
}
const flip = (i) => (t) => i(1 - t);
const unit = [0, 1];
const interpolators = /* @__PURE__ */ new Map([
  // numbers
  ["number", interpolateNumber],
  // color spaces
  ["rgb", interpolateRgb],
  ["hsl", interpolateHsl],
  ["hcl", interpolateHcl],
  ["lab", lab]
]);
function Interpolator(interpolate2) {
  const i = `${interpolate2}`.toLowerCase();
  if (!interpolators.has(i))
    throw new Error(`unknown interpolator: ${i}`);
  return interpolators.get(i);
}
function ScaleQ(key, scale, channels, { type, nice: nice2, clamp, zero: zero2, domain = inferAutoDomain(key, channels), unknown, round, scheme: scheme2, interval: interval2, range: range2 = registry.get(key) === radius ? inferRadialRange(channels, domain) : registry.get(key) === length ? inferLengthRange(channels, domain) : registry.get(key) === opacity ? unit : void 0, interpolate: interpolate2 = registry.get(key) === color ? scheme2 == null && range2 !== void 0 ? interpolateRgb : quantitativeScheme(scheme2 !== void 0 ? scheme2 : type === "cyclical" ? "rainbow" : "turbo") : round ? interpolateRound : interpolateNumber, reverse: reverse$1 }) {
  interval2 = maybeInterval(interval2, type);
  if (type === "cyclical" || type === "sequential")
    type = "linear";
  reverse$1 = !!reverse$1;
  if (typeof interpolate2 !== "function") {
    interpolate2 = Interpolator(interpolate2);
  }
  if (interpolate2.length === 1) {
    if (reverse$1) {
      interpolate2 = flip(interpolate2);
      reverse$1 = false;
    }
    if (range2 === void 0) {
      range2 = Float64Array.from(domain, (_, i) => i / (domain.length - 1));
      if (range2.length === 2)
        range2 = unit;
    }
    scale.interpolate((range2 === unit ? constant : interpolatePiecewise)(interpolate2));
  } else {
    scale.interpolate(interpolate2);
  }
  if (zero2) {
    const [min2, max2] = extent$1(domain);
    if (min2 > 0 || max2 < 0) {
      domain = slice(domain);
      if (orderof(domain) !== Math.sign(min2))
        domain[domain.length - 1] = 0;
      else
        domain[0] = 0;
    }
  }
  if (reverse$1)
    domain = reverse(domain);
  scale.domain(domain).unknown(unknown);
  if (nice2)
    scale.nice(nice2 === true ? void 0 : nice2), domain = scale.domain();
  if (range2 !== void 0)
    scale.range(range2);
  if (clamp)
    scale.clamp(clamp);
  return { type, domain, range: range2, scale, interpolate: interpolate2, interval: interval2 };
}
function ScaleLinear(key, channels, options) {
  return ScaleQ(key, linear(), channels, options);
}
function ScaleSqrt(key, channels, options) {
  return ScalePow(key, channels, { ...options, exponent: 0.5 });
}
function ScalePow(key, channels, { exponent: exponent2 = 1, ...options }) {
  return ScaleQ(key, pow().exponent(exponent2), channels, { ...options, type: "pow" });
}
function ScaleLog(key, channels, { base = 10, domain = inferLogDomain(channels), ...options }) {
  return ScaleQ(key, log().base(base), channels, { ...options, domain });
}
function ScaleSymlog(key, channels, { constant: constant2 = 1, ...options }) {
  return ScaleQ(key, symlog().constant(constant2), channels, options);
}
function ScaleQuantile(key, channels, {
  range: range2,
  quantiles = range2 === void 0 ? 5 : (range2 = [...range2]).length,
  // deprecated; use n instead
  n = quantiles,
  scheme: scheme2 = "rdylbu",
  domain = inferQuantileDomain(channels),
  interpolate: interpolate2,
  reverse: reverse2
}) {
  if (range2 === void 0) {
    range2 = interpolate2 !== void 0 ? quantize(interpolate2, n) : registry.get(key) === color ? ordinalRange(scheme2, n) : void 0;
  }
  if (domain.length > 0) {
    domain = quantile(domain, range2 === void 0 ? { length: n } : range2).quantiles();
  }
  return ScaleThreshold(key, channels, { domain, range: range2, reverse: reverse2 });
}
function ScaleQuantize(key, channels, { range: range2, n = range2 === void 0 ? 5 : (range2 = [...range2]).length, scheme: scheme2 = "rdylbu", domain = inferAutoDomain(key, channels), unknown, interpolate: interpolate2, reverse: reverse2 }) {
  const [min2, max2] = extent$1(domain);
  let thresholds;
  if (range2 === void 0) {
    thresholds = ticks(min2, max2, n);
    if (thresholds[0] <= min2)
      thresholds.splice(0, 1);
    if (thresholds[thresholds.length - 1] >= max2)
      thresholds.pop();
    n = thresholds.length + 1;
    range2 = interpolate2 !== void 0 ? quantize(interpolate2, n) : registry.get(key) === color ? ordinalRange(scheme2, n) : void 0;
  } else {
    thresholds = quantize(interpolateNumber(min2, max2), n + 1).slice(1, -1);
    if (min2 instanceof Date)
      thresholds = thresholds.map((x) => new Date(x));
  }
  if (orderof(arrayify(domain)) < 0)
    thresholds.reverse();
  return ScaleThreshold(key, channels, { domain: thresholds, range: range2, reverse: reverse2, unknown });
}
function ScaleThreshold(key, channels, {
  domain = [0],
  // explicit thresholds in ascending order
  unknown,
  scheme: scheme2 = "rdylbu",
  interpolate: interpolate2,
  range: range2 = interpolate2 !== void 0 ? quantize(interpolate2, domain.length + 1) : registry.get(key) === color ? ordinalRange(scheme2, domain.length + 1) : void 0,
  reverse: reverse$1
}) {
  domain = arrayify(domain);
  const sign2 = orderof(domain);
  if (!isOrdered(domain, sign2))
    throw new Error(`the ${key} scale has a non-monotonic domain`);
  if (reverse$1)
    range2 = reverse(range2);
  return {
    type: "threshold",
    scale: threshold(sign2 < 0 ? reverse(domain) : domain, range2 === void 0 ? [] : range2).unknown(unknown),
    domain,
    range: range2
  };
}
function isOrdered(domain, sign2) {
  for (let i = 1, n = domain.length, d = domain[0]; i < n; ++i) {
    const s2 = descending(d, d = domain[i]);
    if (s2 !== 0 && s2 !== sign2)
      return false;
  }
  return true;
}
function ScaleIdentity() {
  return { type: "identity", scale: identity$2() };
}
function inferDomain$1(channels, f = finite) {
  return channels.length ? [
    min$1(channels, ({ value }) => value === void 0 ? value : min$1(value, f)),
    max(channels, ({ value }) => value === void 0 ? value : max(value, f))
  ] : [0, 1];
}
function inferAutoDomain(key, channels) {
  const type = registry.get(key);
  return (type === radius || type === opacity || type === length ? inferZeroDomain : inferDomain$1)(channels);
}
function inferZeroDomain(channels) {
  return [0, channels.length ? max(channels, ({ value }) => value === void 0 ? value : max(value, finite)) : 1];
}
function inferRadialRange(channels, domain) {
  const hint = channels.find(({ radius: radius2 }) => radius2 !== void 0);
  if (hint !== void 0)
    return [0, hint.radius];
  const h25 = quantile$1(channels, 0.5, ({ value }) => value === void 0 ? NaN : quantile$1(value, 0.25, positive));
  const range2 = domain.map((d) => 3 * Math.sqrt(d / h25));
  const k2 = 30 / max(range2);
  return k2 < 1 ? range2.map((r) => r * k2) : range2;
}
function inferLengthRange(channels, domain) {
  const h50 = median(channels, ({ value }) => value === void 0 ? NaN : median(value, Math.abs));
  const range2 = domain.map((d) => 12 * d / h50);
  const k2 = 60 / max(range2);
  return k2 < 1 ? range2.map((r) => r * k2) : range2;
}
function inferLogDomain(channels) {
  for (const { value } of channels) {
    if (value !== void 0) {
      for (let v of value) {
        if (v > 0)
          return inferDomain$1(channels, positive);
        if (v < 0)
          return inferDomain$1(channels, negative);
      }
    }
  }
  return [1, 10];
}
function inferQuantileDomain(channels) {
  const domain = [];
  for (const { value } of channels) {
    if (value === void 0)
      continue;
    for (const v of value)
      domain.push(v);
  }
  return domain;
}
function interpolatePiecewise(interpolate2) {
  return (i, j) => (t) => interpolate2(i + t * (j - i));
}
function ScaleD(key, scale, transform, channels, { type, nice: nice2, clamp, domain = inferDomain$1(channels), unknown, pivot = 0, scheme: scheme2, range: range2, symmetric = true, interpolate: interpolate2 = registry.get(key) === color ? scheme2 == null && range2 !== void 0 ? interpolateRgb : quantitativeScheme(scheme2 !== void 0 ? scheme2 : "rdbu") : interpolateNumber, reverse: reverse2 }) {
  pivot = +pivot;
  let [min2, max2] = domain;
  if (descending(min2, max2) < 0)
    [min2, max2] = [max2, min2], reverse2 = !reverse2;
  min2 = Math.min(min2, pivot);
  max2 = Math.max(max2, pivot);
  if (typeof interpolate2 !== "function") {
    interpolate2 = Interpolator(interpolate2);
  }
  if (range2 !== void 0) {
    interpolate2 = interpolate2.length === 1 ? interpolatePiecewise(interpolate2)(...range2) : piecewise(interpolate2, range2);
  }
  if (reverse2)
    interpolate2 = flip(interpolate2);
  if (symmetric) {
    const mid2 = transform.apply(pivot);
    const mindelta = mid2 - transform.apply(min2);
    const maxdelta = transform.apply(max2) - mid2;
    if (mindelta < maxdelta)
      min2 = transform.invert(mid2 - maxdelta);
    else if (mindelta > maxdelta)
      max2 = transform.invert(mid2 + mindelta);
  }
  scale.domain([min2, pivot, max2]).unknown(unknown).interpolator(interpolate2);
  if (clamp)
    scale.clamp(clamp);
  if (nice2)
    scale.nice(nice2);
  return { type, domain: [min2, max2], pivot, interpolate: interpolate2, scale };
}
function ScaleDiverging(key, channels, options) {
  return ScaleD(key, diverging(), transformIdentity, channels, options);
}
function ScaleDivergingSqrt(key, channels, options) {
  return ScaleDivergingPow(key, channels, { ...options, exponent: 0.5 });
}
function ScaleDivergingPow(key, channels, { exponent: exponent2 = 1, ...options }) {
  return ScaleD(key, divergingPow().exponent(exponent2 = +exponent2), transformPow(exponent2), channels, {
    ...options,
    type: "diverging-pow"
  });
}
function ScaleDivergingLog(key, channels, { base = 10, pivot = 1, domain = inferDomain$1(channels, pivot < 0 ? negative : positive), ...options }) {
  return ScaleD(key, divergingLog().base(base = +base), transformLog, channels, { domain, pivot, ...options });
}
function ScaleDivergingSymlog(key, channels, { constant: constant2 = 1, ...options }) {
  return ScaleD(key, divergingSymlog().constant(constant2 = +constant2), transformSymlog(constant2), channels, options);
}
const transformIdentity = {
  apply(x) {
    return x;
  },
  invert(x) {
    return x;
  }
};
const transformLog = {
  apply: Math.log,
  invert: Math.exp
};
const transformSqrt = {
  apply(x) {
    return Math.sign(x) * Math.sqrt(Math.abs(x));
  },
  invert(x) {
    return Math.sign(x) * (x * x);
  }
};
function transformPow(exponent2) {
  return exponent2 === 0.5 ? transformSqrt : {
    apply(x) {
      return Math.sign(x) * Math.pow(Math.abs(x), exponent2);
    },
    invert(x) {
      return Math.sign(x) * Math.pow(Math.abs(x), 1 / exponent2);
    }
  };
}
function transformSymlog(constant2) {
  return {
    apply(x) {
      return Math.sign(x) * Math.log1p(Math.abs(x / constant2));
    },
    invert(x) {
      return Math.sign(x) * Math.expm1(Math.abs(x)) * constant2;
    }
  };
}
function ScaleT(key, scale, channels, options) {
  return ScaleQ(key, scale, channels, options);
}
function ScaleTime(key, channels, options) {
  return ScaleT(key, time(), channels, options);
}
function ScaleUtc(key, channels, options) {
  return ScaleT(key, utcTime(), channels, options);
}
const ordinalImplicit = Symbol("ordinal");
function ScaleO(key, scale, channels, { type, interval: interval2, domain, range: range2, reverse: reverse$1, hint }) {
  interval2 = maybeInterval(interval2, type);
  if (domain === void 0)
    domain = inferDomain(channels, interval2, key);
  if (type === "categorical" || type === ordinalImplicit)
    type = "ordinal";
  if (reverse$1)
    domain = reverse(domain);
  scale.domain(domain);
  if (range2 !== void 0) {
    if (typeof range2 === "function")
      range2 = range2(domain);
    scale.range(range2);
  }
  return { type, domain, range: range2, scale, hint, interval: interval2 };
}
function ScaleOrdinal(key, channels, { type, interval: interval2, domain, range: range2, scheme: scheme2, unknown, ...options }) {
  interval2 = maybeInterval(interval2, type);
  if (domain === void 0)
    domain = inferDomain(channels, interval2, key);
  let hint;
  if (registry.get(key) === symbol) {
    hint = inferSymbolHint(channels);
    range2 = range2 === void 0 ? inferSymbolRange(hint) : map(range2, maybeSymbol);
  } else if (registry.get(key) === color) {
    if (range2 === void 0 && (type === "ordinal" || type === ordinalImplicit)) {
      range2 = maybeBooleanRange(domain, scheme2);
      if (range2 !== void 0)
        scheme2 = void 0;
    }
    if (scheme2 === void 0 && range2 === void 0) {
      scheme2 = type === "ordinal" ? "turbo" : "tableau10";
    }
    if (scheme2 !== void 0) {
      if (range2 !== void 0) {
        const interpolate2 = quantitativeScheme(scheme2);
        const t02 = range2[0], d = range2[1] - range2[0];
        range2 = ({ length: n }) => quantize((t) => interpolate2(t02 + d * t), n);
      } else {
        range2 = ordinalScheme(scheme2);
      }
    }
  }
  if (unknown === implicit) {
    throw new Error(`implicit unknown on ${key} scale is not supported`);
  }
  return ScaleO(key, ordinal().unknown(unknown), channels, { ...options, type, domain, range: range2, hint });
}
function ScalePoint(key, channels, { align = 0.5, padding = 0.5, ...options }) {
  return maybeRound(point().align(align).padding(padding), channels, options, key);
}
function ScaleBand(key, channels, { align = 0.5, padding = 0.1, paddingInner = padding, paddingOuter = key === "fx" || key === "fy" ? 0 : padding, ...options }) {
  return maybeRound(band().align(align).paddingInner(paddingInner).paddingOuter(paddingOuter), channels, options, key);
}
function maybeRound(scale, channels, options, key) {
  let { round } = options;
  if (round !== void 0)
    scale.round(round = !!round);
  scale = ScaleO(key, scale, channels, options);
  scale.round = round;
  return scale;
}
function inferDomain(channels, interval2, key) {
  const values2 = new InternSet();
  for (const { value, domain } of channels) {
    if (domain !== void 0)
      return domain();
    if (value === void 0)
      continue;
    for (const v of value)
      values2.add(v);
  }
  if (interval2 !== void 0) {
    const [min2, max2] = extent$1(values2).map(interval2.floor, interval2);
    return interval2.range(min2, interval2.offset(max2));
  }
  if (values2.size > 1e4 && registry.get(key) === position) {
    throw new Error(`implicit ordinal domain of ${key} scale has more than 10,000 values`);
  }
  return sort$1(values2, ascendingDefined);
}
function inferHint(channels, key) {
  let value;
  for (const { hint } of channels) {
    const candidate = hint == null ? void 0 : hint[key];
    if (candidate === void 0)
      continue;
    if (value === void 0)
      value = candidate;
    else if (value !== candidate)
      return;
  }
  return value;
}
function inferSymbolHint(channels) {
  return {
    fill: inferHint(channels, "fill"),
    stroke: inferHint(channels, "stroke")
  };
}
function inferSymbolRange(hint) {
  return isNoneish(hint.fill) ? symbolsStroke : symbolsFill;
}
let warnings = 0;
function consumeWarnings() {
  const w = warnings;
  warnings = 0;
  return w;
}
function warn(message) {
  console.warn(message);
  ++warnings;
}
function Scales(channelsByScale, { label: globalLabel, inset: globalInset = 0, insetTop: globalInsetTop = globalInset, insetRight: globalInsetRight = globalInset, insetBottom: globalInsetBottom = globalInset, insetLeft: globalInsetLeft = globalInset, round, nice: nice2, clamp, zero: zero2, align, padding, projection: projection2, facet: { label: facetLabel = globalLabel } = {}, ...options } = {}) {
  const scales = {};
  for (const [key, channels] of channelsByScale) {
    const scaleOptions = options[key];
    const scale = Scale(key, channels, {
      round: registry.get(key) === position ? round : void 0,
      nice: nice2,
      clamp,
      zero: zero2,
      align,
      padding,
      projection: projection2,
      ...scaleOptions
    });
    if (scale) {
      let {
        label = key === "fx" || key === "fy" ? facetLabel : globalLabel,
        percent,
        transform,
        inset,
        insetTop = inset !== void 0 ? inset : key === "y" ? globalInsetTop : 0,
        // not fy
        insetRight = inset !== void 0 ? inset : key === "x" ? globalInsetRight : 0,
        // not fx
        insetBottom = inset !== void 0 ? inset : key === "y" ? globalInsetBottom : 0,
        // not fy
        insetLeft = inset !== void 0 ? inset : key === "x" ? globalInsetLeft : 0
        // not fx
      } = scaleOptions || {};
      if (transform == null)
        transform = void 0;
      else if (typeof transform !== "function")
        throw new Error("invalid scale transform; not a function");
      scale.percent = !!percent;
      scale.label = label === void 0 ? inferScaleLabel(channels, scale) : label;
      scale.transform = transform;
      if (key === "x" || key === "fx") {
        scale.insetLeft = +insetLeft;
        scale.insetRight = +insetRight;
      } else if (key === "y" || key === "fy") {
        scale.insetTop = +insetTop;
        scale.insetBottom = +insetBottom;
      }
      scales[key] = scale;
    }
  }
  return scales;
}
function ScaleFunctions(scales) {
  return Object.fromEntries(Object.entries(scales).filter(([, { scale }]) => scale).map(([name, { scale, type, interval: interval2, label }]) => {
    scale.type = type;
    if (interval2 != null)
      scale.interval = interval2;
    if (label != null)
      scale.label = label;
    return [name, scale];
  }));
}
function autoScaleRange(scales, dimensions) {
  const { x, y, fx, fy } = scales;
  const superdimensions = fx || fy ? outerDimensions(dimensions) : dimensions;
  if (fx)
    autoScaleRangeX(fx, superdimensions);
  if (fy)
    autoScaleRangeY(fy, superdimensions);
  const subdimensions = fx || fy ? innerDimensions(scales, dimensions) : dimensions;
  if (x)
    autoScaleRangeX(x, subdimensions);
  if (y)
    autoScaleRangeY(y, subdimensions);
}
function inferScaleLabel(channels = [], scale) {
  let label;
  for (const { label: l } of channels) {
    if (l === void 0)
      continue;
    if (label === void 0)
      label = l;
    else if (label !== l)
      return;
  }
  if (label === void 0)
    return;
  if (isTemporalScale(scale) && /^(date|time|year)$/i.test(label))
    return;
  if (!isOrdinalScale(scale) && scale.percent)
    label = `${label} (%)`;
  return { inferred: true, toString: () => label };
}
function outerDimensions(dimensions) {
  const { marginTop, marginRight, marginBottom, marginLeft, width, height, facet: { marginTop: facetMarginTop, marginRight: facetMarginRight, marginBottom: facetMarginBottom, marginLeft: facetMarginLeft } } = dimensions;
  return {
    marginTop: Math.max(marginTop, facetMarginTop),
    marginRight: Math.max(marginRight, facetMarginRight),
    marginBottom: Math.max(marginBottom, facetMarginBottom),
    marginLeft: Math.max(marginLeft, facetMarginLeft),
    width,
    height
  };
}
function innerDimensions({ fx, fy }, dimensions) {
  const { marginTop, marginRight, marginBottom, marginLeft, width, height } = outerDimensions(dimensions);
  return {
    marginTop,
    marginRight,
    marginBottom,
    marginLeft,
    width: fx ? fx.scale.bandwidth() + marginLeft + marginRight : width,
    height: fy ? fy.scale.bandwidth() + marginTop + marginBottom : height
  };
}
function autoScaleRangeX(scale, dimensions) {
  if (scale.range === void 0) {
    const { insetLeft, insetRight } = scale;
    const { width, marginLeft = 0, marginRight = 0 } = dimensions;
    const left2 = marginLeft + insetLeft;
    const right2 = width - marginRight - insetRight;
    scale.range = [left2, Math.max(left2, right2)];
    if (!isOrdinalScale(scale))
      scale.range = piecewiseRange(scale);
    scale.scale.range(scale.range);
  }
  autoScaleRound(scale);
}
function autoScaleRangeY(scale, dimensions) {
  if (scale.range === void 0) {
    const { insetTop, insetBottom } = scale;
    const { height, marginTop = 0, marginBottom = 0 } = dimensions;
    const top2 = marginTop + insetTop;
    const bottom2 = height - marginBottom - insetBottom;
    scale.range = [Math.max(top2, bottom2), top2];
    if (!isOrdinalScale(scale))
      scale.range = piecewiseRange(scale);
    else
      scale.range.reverse();
    scale.scale.range(scale.range);
  }
  autoScaleRound(scale);
}
function autoScaleRound(scale) {
  if (scale.round === void 0 && isBandScale(scale) && roundError(scale) <= 30) {
    scale.scale.round(true);
  }
}
function roundError({ scale }) {
  const n = scale.domain().length;
  const [start2, stop] = scale.range();
  const paddingInner = scale.paddingInner ? scale.paddingInner() : 1;
  const paddingOuter = scale.paddingOuter ? scale.paddingOuter() : scale.padding();
  const m = n - paddingInner;
  const step = Math.abs(stop - start2) / Math.max(1, m + paddingOuter * 2);
  return (step - Math.floor(step)) * m;
}
function piecewiseRange(scale) {
  const length2 = scale.scale.domain().length + isThresholdScale(scale);
  if (!(length2 > 2))
    return scale.range;
  const [start2, end] = scale.range;
  return Array.from({ length: length2 }, (_, i) => start2 + i / (length2 - 1) * (end - start2));
}
function Scale(key, channels = [], options = {}) {
  const type = inferScaleType(key, channels, options);
  if (options.type === void 0 && options.domain === void 0 && options.range === void 0 && options.interval == null && key !== "fx" && key !== "fy" && isOrdinalScale({ type })) {
    const values2 = channels.map(({ value }) => value).filter((value) => value !== void 0);
    if (values2.some(isTemporal))
      warn(`Warning: some data associated with the ${key} scale are dates. Dates are typically associated with a "utc" or "time" scale rather than a "${formatScaleType(type)}" scale. If you are using a bar mark, you probably want a rect mark with the interval option instead; if you are using a group transform, you probably want a bin transform instead. If you want to treat this data as ordinal, you can specify the interval of the ${key} scale (e.g., d3.utcDay), or you can suppress this warning by setting the type of the ${key} scale to "${formatScaleType(type)}".`);
    else if (values2.some(isTemporalString))
      warn(`Warning: some data associated with the ${key} scale are strings that appear to be dates (e.g., YYYY-MM-DD). If these strings represent dates, you should parse them to Date objects. Dates are typically associated with a "utc" or "time" scale rather than a "${formatScaleType(type)}" scale. If you are using a bar mark, you probably want a rect mark with the interval option instead; if you are using a group transform, you probably want a bin transform instead. If you want to treat this data as ordinal, you can suppress this warning by setting the type of the ${key} scale to "${formatScaleType(type)}".`);
    else if (values2.some(isNumericString))
      warn(`Warning: some data associated with the ${key} scale are strings that appear to be numbers. If these strings represent numbers, you should parse or coerce them to numbers. Numbers are typically associated with a "linear" scale rather than a "${formatScaleType(type)}" scale. If you want to treat this data as ordinal, you can specify the interval of the ${key} scale (e.g., 1 for integers), or you can suppress this warning by setting the type of the ${key} scale to "${formatScaleType(type)}".`);
  }
  options.type = type;
  switch (type) {
    case "diverging":
    case "diverging-sqrt":
    case "diverging-pow":
    case "diverging-log":
    case "diverging-symlog":
    case "cyclical":
    case "sequential":
    case "linear":
    case "sqrt":
    case "threshold":
    case "quantile":
    case "pow":
    case "log":
    case "symlog":
      options = coerceType(channels, options, coerceNumbers);
      break;
    case "identity":
      switch (registry.get(key)) {
        case position:
          options = coerceType(channels, options, coerceNumbers);
          break;
        case symbol:
          options = coerceType(channels, options, coerceSymbols);
          break;
      }
      break;
    case "utc":
    case "time":
      options = coerceType(channels, options, coerceDates);
      break;
  }
  switch (type) {
    case "diverging":
      return ScaleDiverging(key, channels, options);
    case "diverging-sqrt":
      return ScaleDivergingSqrt(key, channels, options);
    case "diverging-pow":
      return ScaleDivergingPow(key, channels, options);
    case "diverging-log":
      return ScaleDivergingLog(key, channels, options);
    case "diverging-symlog":
      return ScaleDivergingSymlog(key, channels, options);
    case "categorical":
    case "ordinal":
    case ordinalImplicit:
      return ScaleOrdinal(key, channels, options);
    case "cyclical":
    case "sequential":
    case "linear":
      return ScaleLinear(key, channels, options);
    case "sqrt":
      return ScaleSqrt(key, channels, options);
    case "threshold":
      return ScaleThreshold(key, channels, options);
    case "quantile":
      return ScaleQuantile(key, channels, options);
    case "quantize":
      return ScaleQuantize(key, channels, options);
    case "pow":
      return ScalePow(key, channels, options);
    case "log":
      return ScaleLog(key, channels, options);
    case "symlog":
      return ScaleSymlog(key, channels, options);
    case "utc":
      return ScaleUtc(key, channels, options);
    case "time":
      return ScaleTime(key, channels, options);
    case "point":
      return ScalePoint(key, channels, options);
    case "band":
      return ScaleBand(key, channels, options);
    case "identity":
      return registry.get(key) === position ? ScaleIdentity() : { type: "identity" };
    case void 0:
      return;
    default:
      throw new Error(`unknown scale type: ${type}`);
  }
}
function formatScaleType(type) {
  return typeof type === "symbol" ? type.description : type;
}
const typeProjection = { toString: () => "projection" };
function inferScaleType(key, channels, { type, domain, range: range2, scheme: scheme2, pivot, projection: projection2 }) {
  if (key === "fx" || key === "fy")
    return "band";
  if ((key === "x" || key === "y") && projection2 != null)
    type = typeProjection;
  for (const { type: t } of channels) {
    if (t === void 0)
      continue;
    else if (type === void 0)
      type = t;
    else if (type !== t)
      throw new Error(`scale incompatible with channel: ${type} !== ${t}`);
  }
  if (type === typeProjection)
    return;
  if (type !== void 0)
    return type;
  if (domain === void 0 && !channels.some(({ value }) => value !== void 0))
    return;
  const kind = registry.get(key);
  if (kind === radius)
    return "sqrt";
  if (kind === opacity || kind === length)
    return "linear";
  if (kind === symbol)
    return "ordinal";
  if ((domain || range2 || []).length > 2)
    return asOrdinalType(kind);
  if (domain !== void 0) {
    if (isOrdinal(domain))
      return asOrdinalType(kind);
    if (isTemporal(domain))
      return "utc";
    if (kind === color && (pivot != null || isDivergingScheme(scheme2)))
      return "diverging";
    return "linear";
  }
  const values2 = channels.map(({ value }) => value).filter((value) => value !== void 0);
  if (values2.some(isOrdinal))
    return asOrdinalType(kind);
  if (values2.some(isTemporal))
    return "utc";
  if (kind === color && (pivot != null || isDivergingScheme(scheme2)))
    return "diverging";
  return "linear";
}
function asOrdinalType(kind) {
  switch (kind) {
    case position:
      return "point";
    case color:
      return ordinalImplicit;
    default:
      return "ordinal";
  }
}
function isTemporalScale({ type }) {
  return type === "time" || type === "utc";
}
function isOrdinalScale({ type }) {
  return type === "ordinal" || type === "point" || type === "band" || type === ordinalImplicit;
}
function isThresholdScale({ type }) {
  return type === "threshold";
}
function isBandScale({ type }) {
  return type === "point" || type === "band";
}
function isCollapsed(scale) {
  if (scale === void 0)
    return true;
  const domain = scale.domain();
  const value = scale(domain[0]);
  for (let i = 1, n = domain.length; i < n; ++i) {
    if (scale(domain[i]) - value) {
      return false;
    }
  }
  return true;
}
function coerceType(channels, { domain, ...options }, coerceValues) {
  for (const c2 of channels) {
    if (c2.value !== void 0) {
      c2.value = coerceValues(c2.value);
    }
  }
  return {
    domain: domain === void 0 ? domain : coerceValues(domain),
    ...options
  };
}
function coerceSymbols(values2) {
  return map(values2, maybeSymbol);
}
function coerceDates(values2) {
  return map(values2, coerceDate);
}
function coerceNumbers(values2) {
  return isTypedArray(values2) ? values2 : map(values2, coerceNumber, Float64Array);
}
function coerceNumber(x) {
  return x == null ? NaN : Number(x);
}
function coerceDate(x) {
  return x instanceof Date && !isNaN(x) ? x : typeof x === "string" ? parse(x) : x == null || isNaN(x = +x) ? void 0 : new Date(x);
}
function exposeScales(scaleDescriptors) {
  return (key) => {
    if (!registry.has(key = `${key}`))
      throw new Error(`unknown scale: ${key}`);
    return key in scaleDescriptors ? exposeScale(scaleDescriptors[key]) : void 0;
  };
}
function exposeScale({ scale, type, domain, range: range2, interpolate: interpolate2, interval: interval2, transform, percent, pivot }) {
  if (type === "identity")
    return { type: "identity", apply: (d) => d, invert: (d) => d };
  const unknown = scale.unknown ? scale.unknown() : void 0;
  return {
    type,
    domain: slice(domain),
    ...range2 !== void 0 && { range: slice(range2) },
    ...transform !== void 0 && { transform },
    ...percent && { percent },
    ...unknown !== void 0 && { unknown },
    ...interval2 !== void 0 && { interval: interval2 },
    // quantitative
    ...interpolate2 !== void 0 && { interpolate: interpolate2 },
    ...scale.clamp && { clamp: scale.clamp() },
    // diverging (always asymmetric; we never want to apply the symmetric transform twice)
    ...pivot !== void 0 && { pivot, symmetric: false },
    // log, diverging-log
    ...scale.base && { base: scale.base() },
    // pow, diverging-pow
    ...scale.exponent && { exponent: scale.exponent() },
    // symlog, diverging-symlog
    ...scale.constant && { constant: scale.constant() },
    // band, point
    ...scale.align && { align: scale.align(), round: scale.round() },
    ...scale.padding && (scale.paddingInner ? { paddingInner: scale.paddingInner(), paddingOuter: scale.paddingOuter() } : { padding: scale.padding() }),
    ...scale.bandwidth && { bandwidth: scale.bandwidth(), step: scale.step() },
    // utilities
    apply: (t) => scale(t),
    ...scale.invert && { invert: (t) => scale.invert(t) }
  };
}
const pi = Math.PI;
const tau = 2 * pi;
const defaultAspectRatio = 0.618;
function Projection({ projection: projection2, inset: globalInset = 0, insetTop = globalInset, insetRight = globalInset, insetBottom = globalInset, insetLeft = globalInset } = {}, dimensions) {
  if (projection2 == null)
    return;
  if (typeof projection2.stream === "function")
    return projection2;
  let options;
  let domain;
  let clip2 = "frame";
  if (isObject(projection2)) {
    let inset;
    ({
      type: projection2,
      domain,
      inset,
      insetTop = inset !== void 0 ? inset : insetTop,
      insetRight = inset !== void 0 ? inset : insetRight,
      insetBottom = inset !== void 0 ? inset : insetBottom,
      insetLeft = inset !== void 0 ? inset : insetLeft,
      clip: clip2 = clip2,
      ...options
    } = projection2);
    if (projection2 == null)
      return;
  }
  if (typeof projection2 !== "function")
    ({ type: projection2 } = namedProjection(projection2));
  const { width, height, marginLeft, marginRight, marginTop, marginBottom } = dimensions;
  const dx = width - marginLeft - marginRight - insetLeft - insetRight;
  const dy = height - marginTop - marginBottom - insetTop - insetBottom;
  projection2 = projection2 == null ? void 0 : projection2({ width: dx, height: dy, clip: clip2, ...options });
  if (projection2 == null)
    return;
  clip2 = maybePostClip(clip2, marginLeft, marginTop, width - marginRight, height - marginBottom);
  let tx = marginLeft + insetLeft;
  let ty = marginTop + insetTop;
  let transform;
  if (domain != null) {
    const [[x02, y02], [x12, y12]] = geoPath(projection2).bounds(domain);
    const k2 = Math.min(dx / (x12 - x02), dy / (y12 - y02));
    if (k2 > 0) {
      tx -= (k2 * (x02 + x12) - dx) / 2;
      ty -= (k2 * (y02 + y12) - dy) / 2;
      transform = geoTransform({
        point(x, y) {
          this.stream.point(x * k2 + tx, y * k2 + ty);
        }
      });
    } else {
      warn(`Warning: the projection could not be fit to the specified domain; using the default scale.`);
    }
  }
  transform ?? (transform = tx === 0 && ty === 0 ? identity() : geoTransform({
    point(x, y) {
      this.stream.point(x + tx, y + ty);
    }
  }));
  return { stream: (s2) => projection2.stream(transform.stream(clip2(s2))) };
}
function namedProjection(projection2) {
  switch (`${projection2}`.toLowerCase()) {
    case "albers-usa":
      return scaleProjection(geoAlbersUsa, 0.7463, 0.4673);
    case "albers":
      return conicProjection(geoAlbers, 0.7463, 0.4673);
    case "azimuthal-equal-area":
      return scaleProjection(geoAzimuthalEqualArea, 4, 4);
    case "azimuthal-equidistant":
      return scaleProjection(geoAzimuthalEquidistant, tau, tau);
    case "conic-conformal":
      return conicProjection(geoConicConformal, tau, tau);
    case "conic-equal-area":
      return conicProjection(geoConicEqualArea, 6.1702, 2.9781);
    case "conic-equidistant":
      return conicProjection(geoConicEquidistant, 7.312, 3.6282);
    case "equal-earth":
      return scaleProjection(geoEqualEarth, 5.4133, 2.6347);
    case "equirectangular":
      return scaleProjection(geoEquirectangular, tau, pi);
    case "gnomonic":
      return scaleProjection(geoGnomonic, 3.4641, 3.4641);
    case "identity":
      return { type: identity };
    case "reflect-y":
      return { type: reflectY };
    case "mercator":
      return scaleProjection(geoMercator, tau, tau);
    case "orthographic":
      return scaleProjection(geoOrthographic, 2, 2);
    case "stereographic":
      return scaleProjection(geoStereographic, 2, 2);
    case "transverse-mercator":
      return scaleProjection(geoTransverseMercator, tau, tau);
    default:
      throw new Error(`unknown projection type: ${projection2}`);
  }
}
function maybePostClip(clip2, x12, y12, x2, y2) {
  if (clip2 === false || clip2 == null || typeof clip2 === "number")
    return (s2) => s2;
  if (clip2 === true)
    clip2 = "frame";
  switch (`${clip2}`.toLowerCase()) {
    case "frame":
      return clipRectangle(x12, y12, x2, y2);
    default:
      throw new Error(`unknown projection clip type: ${clip2}`);
  }
}
function scaleProjection(createProjection, kx2, ky2) {
  return {
    type: ({ width, height, rotate, precision = 0.15, clip: clip2 }) => {
      var _a, _b, _c;
      const projection2 = createProjection();
      if (precision != null)
        (_a = projection2.precision) == null ? void 0 : _a.call(projection2, precision);
      if (rotate != null)
        (_b = projection2.rotate) == null ? void 0 : _b.call(projection2, rotate);
      if (typeof clip2 === "number")
        (_c = projection2.clipAngle) == null ? void 0 : _c.call(projection2, clip2);
      projection2.scale(Math.min(width / kx2, height / ky2));
      projection2.translate([width / 2, height / 2]);
      return projection2;
    },
    aspectRatio: ky2 / kx2
  };
}
function conicProjection(createProjection, kx2, ky2) {
  const { type, aspectRatio } = scaleProjection(createProjection, kx2, ky2);
  return {
    type: (options) => {
      const { parallels, domain, width, height } = options;
      const projection2 = type(options);
      if (parallels != null) {
        projection2.parallels(parallels);
        if (domain === void 0) {
          projection2.fitSize([width, height], { type: "Sphere" });
        }
      }
      return projection2;
    },
    aspectRatio
  };
}
const identity = constant({ stream: (stream) => stream });
const reflectY = constant(geoTransform({
  point(x, y) {
    this.stream.point(x, -y);
  }
}));
function maybeProject(cx, cy, channels, values2, context) {
  const x = channels[cx] && channels[cx].scale === "x";
  const y = channels[cy] && channels[cy].scale === "y";
  if (x && y) {
    project(cx, cy, values2, context.projection);
  } else if (x) {
    throw new Error(`projection requires paired x and y channels; ${cx} is missing ${cy}`);
  } else if (y) {
    throw new Error(`projection requires paired x and y channels; ${cy} is missing ${cx}`);
  }
}
function project(cx, cy, values2, projection2) {
  const x = values2[cx];
  const y = values2[cy];
  const n = x.length;
  const X = values2[cx] = new Float64Array(n).fill(NaN);
  const Y = values2[cy] = new Float64Array(n).fill(NaN);
  let i;
  const stream = projection2.stream({
    point(x2, y2) {
      X[i] = x2;
      Y[i] = y2;
    }
  });
  for (i = 0; i < n; ++i) {
    stream.point(x[i], y[i]);
  }
}
function projectionAspectRatio(projection2, marks2) {
  if (typeof (projection2 == null ? void 0 : projection2.stream) === "function")
    return defaultAspectRatio;
  if (isObject(projection2))
    projection2 = projection2.type;
  if (projection2 == null)
    return hasGeometry(marks2) ? defaultAspectRatio : void 0;
  if (typeof projection2 !== "function") {
    const { aspectRatio } = namedProjection(projection2);
    if (aspectRatio)
      return aspectRatio;
  }
  return defaultAspectRatio;
}
function hasGeometry(marks2) {
  for (const mark of marks2)
    if (mark.channels.geometry)
      return true;
  return false;
}
function Context(options = {}, dimensions) {
  const { document: document2 = typeof window !== "undefined" ? window.document : void 0 } = options;
  return { document: document2, projection: Projection(options, dimensions) };
}
function create(name, { document: document2 }) {
  return select(creator(name).call(document2.documentElement));
}
function memoize1(compute) {
  let cacheValue, cacheKeys;
  return (...keys) => {
    if ((cacheKeys == null ? void 0 : cacheKeys.length) !== keys.length || cacheKeys.some((k2, i) => k2 !== keys[i])) {
      cacheKeys = keys;
      cacheValue = compute(...keys);
    }
    return cacheValue;
  };
}
const numberFormat = memoize1((locale2) => new Intl.NumberFormat(locale2));
function formatNumber(locale2 = "en-US") {
  const format2 = numberFormat(locale2);
  return (i) => i != null && !isNaN(i) ? format2.format(i) : void 0;
}
function formatIsoDate(date2) {
  return format(date2, "Invalid Date");
}
function formatAuto(locale2 = "en-US") {
  const number2 = formatNumber(locale2);
  return (v) => (v instanceof Date ? formatIsoDate : typeof v === "number" ? number2 : string)(v);
}
const formatDefault = formatAuto();
const offset = typeof window !== "undefined" && window.devicePixelRatio > 1 ? 0 : 0.5;
let nextClipId = 0;
function getClipId() {
  return `plot-clip-${++nextClipId}`;
}
function styles(mark, { title, href, ariaLabel: variaLabel, ariaDescription, ariaHidden, target, fill, fillOpacity, stroke, strokeWidth, strokeOpacity, strokeLinejoin, strokeLinecap, strokeMiterlimit, strokeDasharray, strokeDashoffset, opacity: opacity2, mixBlendMode, paintOrder, pointerEvents, shapeRendering }, { ariaLabel: cariaLabel, fill: defaultFill = "currentColor", fillOpacity: defaultFillOpacity, stroke: defaultStroke = "none", strokeOpacity: defaultStrokeOpacity, strokeWidth: defaultStrokeWidth, strokeLinecap: defaultStrokeLinecap, strokeLinejoin: defaultStrokeLinejoin, strokeMiterlimit: defaultStrokeMiterlimit, paintOrder: defaultPaintOrder }) {
  if (defaultFill === null) {
    fill = null;
    fillOpacity = null;
  }
  if (defaultStroke === null) {
    stroke = null;
    strokeOpacity = null;
  }
  if (isNoneish(defaultFill)) {
    if (!isNoneish(defaultStroke) && !isNoneish(fill))
      defaultStroke = "none";
  } else {
    if (isNoneish(defaultStroke) && !isNoneish(stroke))
      defaultFill = "none";
  }
  const [vfill, cfill] = maybeColorChannel(fill, defaultFill);
  const [vfillOpacity, cfillOpacity] = maybeNumberChannel(fillOpacity, defaultFillOpacity);
  const [vstroke, cstroke] = maybeColorChannel(stroke, defaultStroke);
  const [vstrokeOpacity, cstrokeOpacity] = maybeNumberChannel(strokeOpacity, defaultStrokeOpacity);
  const [vopacity, copacity] = maybeNumberChannel(opacity2);
  if (!isNone(cstroke)) {
    if (strokeWidth === void 0)
      strokeWidth = defaultStrokeWidth;
    if (strokeLinecap === void 0)
      strokeLinecap = defaultStrokeLinecap;
    if (strokeLinejoin === void 0)
      strokeLinejoin = defaultStrokeLinejoin;
    if (strokeMiterlimit === void 0 && !isRound(strokeLinejoin))
      strokeMiterlimit = defaultStrokeMiterlimit;
    if (!isNone(cfill) && paintOrder === void 0)
      paintOrder = defaultPaintOrder;
  }
  const [vstrokeWidth, cstrokeWidth] = maybeNumberChannel(strokeWidth);
  if (defaultFill !== null) {
    mark.fill = impliedString(cfill, "currentColor");
    mark.fillOpacity = impliedNumber(cfillOpacity, 1);
  }
  if (defaultStroke !== null) {
    mark.stroke = impliedString(cstroke, "none");
    mark.strokeWidth = impliedNumber(cstrokeWidth, 1);
    mark.strokeOpacity = impliedNumber(cstrokeOpacity, 1);
    mark.strokeLinejoin = impliedString(strokeLinejoin, "miter");
    mark.strokeLinecap = impliedString(strokeLinecap, "butt");
    mark.strokeMiterlimit = impliedNumber(strokeMiterlimit, 4);
    mark.strokeDasharray = impliedString(strokeDasharray, "none");
    mark.strokeDashoffset = impliedString(strokeDashoffset, "0");
  }
  mark.target = string(target);
  mark.ariaLabel = string(cariaLabel);
  mark.ariaDescription = string(ariaDescription);
  mark.ariaHidden = string(ariaHidden);
  mark.opacity = impliedNumber(copacity, 1);
  mark.mixBlendMode = impliedString(mixBlendMode, "normal");
  mark.paintOrder = impliedString(paintOrder, "normal");
  mark.pointerEvents = impliedString(pointerEvents, "auto");
  mark.shapeRendering = impliedString(shapeRendering, "auto");
  return {
    title: { value: title, optional: true },
    href: { value: href, optional: true },
    ariaLabel: { value: variaLabel, optional: true },
    fill: { value: vfill, scale: "auto", optional: true },
    fillOpacity: { value: vfillOpacity, scale: "opacity", optional: true },
    stroke: { value: vstroke, scale: "auto", optional: true },
    strokeOpacity: { value: vstrokeOpacity, scale: "opacity", optional: true },
    strokeWidth: { value: vstrokeWidth, optional: true },
    opacity: { value: vopacity, scale: "opacity", optional: true }
  };
}
function applyTitle(selection2, L) {
  if (L)
    selection2.filter((i) => nonempty(L[i])).append("title").call(applyText, L);
}
function applyText(selection2, T) {
  if (T)
    selection2.text((i) => formatDefault(T[i]));
}
function applyChannelStyles(selection2, { target }, { ariaLabel: AL, title: T, fill: F, fillOpacity: FO, stroke: S, strokeOpacity: SO, strokeWidth: SW, opacity: O, href: H }) {
  if (AL)
    applyAttr(selection2, "aria-label", (i) => AL[i]);
  if (F)
    applyAttr(selection2, "fill", (i) => F[i]);
  if (FO)
    applyAttr(selection2, "fill-opacity", (i) => FO[i]);
  if (S)
    applyAttr(selection2, "stroke", (i) => S[i]);
  if (SO)
    applyAttr(selection2, "stroke-opacity", (i) => SO[i]);
  if (SW)
    applyAttr(selection2, "stroke-width", (i) => SW[i]);
  if (O)
    applyAttr(selection2, "opacity", (i) => O[i]);
  if (H)
    applyHref(selection2, (i) => H[i], target);
  applyTitle(selection2, T);
}
function maybeClip(clip2) {
  if (clip2 === true)
    clip2 = "frame";
  else if (clip2 === false)
    clip2 = null;
  return maybeKeyword(clip2, "clip", ["frame", "sphere"]);
}
function applyClip(selection2, mark, dimensions, context) {
  let clipUrl;
  switch (mark.clip) {
    case "frame": {
      const { width, height, marginLeft, marginRight, marginTop, marginBottom } = dimensions;
      const id2 = getClipId();
      clipUrl = `url(#${id2})`;
      selection2 = create("svg:g", context).call((g) => g.append("svg:clipPath").attr("id", id2).append("rect").attr("x", marginLeft).attr("y", marginTop).attr("width", width - marginRight - marginLeft).attr("height", height - marginTop - marginBottom)).each(function() {
        this.appendChild(selection2.node());
        selection2.node = () => this;
      });
      break;
    }
    case "sphere": {
      const { projection: projection2 } = context;
      if (!projection2)
        throw new Error(`the "sphere" clip option requires a projection`);
      const id2 = getClipId();
      clipUrl = `url(#${id2})`;
      selection2.append("clipPath").attr("id", id2).append("path").attr("d", geoPath(projection2)({ type: "Sphere" }));
      break;
    }
  }
  applyAttr(selection2, "aria-label", mark.ariaLabel);
  applyAttr(selection2, "aria-description", mark.ariaDescription);
  applyAttr(selection2, "aria-hidden", mark.ariaHidden);
  applyAttr(selection2, "clip-path", clipUrl);
}
function applyIndirectStyles(selection2, mark, dimensions, context) {
  applyClip(selection2, mark, dimensions, context);
  applyAttr(selection2, "fill", mark.fill);
  applyAttr(selection2, "fill-opacity", mark.fillOpacity);
  applyAttr(selection2, "stroke", mark.stroke);
  applyAttr(selection2, "stroke-width", mark.strokeWidth);
  applyAttr(selection2, "stroke-opacity", mark.strokeOpacity);
  applyAttr(selection2, "stroke-linejoin", mark.strokeLinejoin);
  applyAttr(selection2, "stroke-linecap", mark.strokeLinecap);
  applyAttr(selection2, "stroke-miterlimit", mark.strokeMiterlimit);
  applyAttr(selection2, "stroke-dasharray", mark.strokeDasharray);
  applyAttr(selection2, "stroke-dashoffset", mark.strokeDashoffset);
  applyAttr(selection2, "shape-rendering", mark.shapeRendering);
  applyAttr(selection2, "paint-order", mark.paintOrder);
  applyAttr(selection2, "pointer-events", mark.pointerEvents);
}
function applyDirectStyles(selection2, mark) {
  applyStyle(selection2, "mix-blend-mode", mark.mixBlendMode);
  applyAttr(selection2, "opacity", mark.opacity);
}
function applyHref(selection2, href, target) {
  selection2.each(function(i) {
    const h = href(i);
    if (h != null) {
      const a2 = this.ownerDocument.createElementNS(namespaces.svg, "a");
      a2.setAttribute("fill", "inherit");
      a2.setAttributeNS(namespaces.xlink, "href", h);
      if (target != null)
        a2.setAttribute("target", target);
      this.parentNode.insertBefore(a2, this).appendChild(this);
    }
  });
}
function applyAttr(selection2, name, value) {
  if (value != null)
    selection2.attr(name, value);
}
function applyStyle(selection2, name, value) {
  if (value != null)
    selection2.style(name, value);
}
function applyTransform(selection2, mark, { x, y }, tx = offset, ty = offset) {
  tx += mark.dx;
  ty += mark.dy;
  if (x == null ? void 0 : x.bandwidth)
    tx += x.bandwidth() / 2;
  if (y == null ? void 0 : y.bandwidth)
    ty += y.bandwidth() / 2;
  if (tx || ty)
    selection2.attr("transform", `translate(${tx},${ty})`);
}
function impliedString(value, impliedValue) {
  if ((value = string(value)) !== impliedValue)
    return value;
}
function impliedNumber(value, impliedValue) {
  if ((value = number(value)) !== impliedValue)
    return value;
}
const validClassName = /^-?([_a-z]|[\240-\377]|\\[0-9a-f]{1,6}(\r\n|[ \t\r\n\f])?|\\[^\r\n\f0-9a-f])([_a-z0-9-]|[\240-\377]|\\[0-9a-f]{1,6}(\r\n|[ \t\r\n\f])?|\\[^\r\n\f0-9a-f])*$/;
function maybeClassName(name) {
  if (name === void 0)
    return `plot-${Math.random().toString(16).slice(2)}`;
  name = `${name}`;
  if (!validClassName.test(name))
    throw new Error(`invalid class name: ${name}`);
  return name;
}
function applyInlineStyles(selection2, style) {
  if (typeof style === "string") {
    selection2.property("style", style);
  } else if (style != null) {
    for (const element of selection2) {
      Object.assign(element.style, style);
    }
  }
}
function applyFrameAnchor({ frameAnchor }, { width, height, marginTop, marginRight, marginBottom, marginLeft }) {
  return [
    /left$/.test(frameAnchor) ? marginLeft : /right$/.test(frameAnchor) ? width - marginRight : (marginLeft + width - marginRight) / 2,
    /^top/.test(frameAnchor) ? marginTop : /^bottom/.test(frameAnchor) ? height - marginBottom : (marginTop + height - marginBottom) / 2
  ];
}
function Dimensions(scales, marks2, options = {}) {
  let marginTopDefault = 0.5 - offset, marginRightDefault = 0.5 + offset, marginBottomDefault = 0.5 + offset, marginLeftDefault = 0.5 - offset;
  for (const { marginTop: marginTop2, marginRight: marginRight2, marginBottom: marginBottom2, marginLeft: marginLeft2 } of marks2) {
    if (marginTop2 > marginTopDefault)
      marginTopDefault = marginTop2;
    if (marginRight2 > marginRightDefault)
      marginRightDefault = marginRight2;
    if (marginBottom2 > marginBottomDefault)
      marginBottomDefault = marginBottom2;
    if (marginLeft2 > marginLeftDefault)
      marginLeftDefault = marginLeft2;
  }
  let { margin, marginTop = margin !== void 0 ? margin : marginTopDefault, marginRight = margin !== void 0 ? margin : marginRightDefault, marginBottom = margin !== void 0 ? margin : marginBottomDefault, marginLeft = margin !== void 0 ? margin : marginLeftDefault } = options;
  marginTop = +marginTop;
  marginRight = +marginRight;
  marginBottom = +marginBottom;
  marginLeft = +marginLeft;
  let { width = 640, height = autoHeight(scales, marks2, options, {
    width,
    marginTopDefault,
    marginRightDefault,
    marginBottomDefault,
    marginLeftDefault
  }) + Math.max(0, marginTop - marginTopDefault + marginBottom - marginBottomDefault) } = options;
  width = +width;
  height = +height;
  const dimensions = {
    width,
    height,
    marginTop,
    marginRight,
    marginBottom,
    marginLeft
  };
  if (scales.fx || scales.fy) {
    let { margin: facetMargin, marginTop: facetMarginTop = facetMargin !== void 0 ? facetMargin : marginTop, marginRight: facetMarginRight = facetMargin !== void 0 ? facetMargin : marginRight, marginBottom: facetMarginBottom = facetMargin !== void 0 ? facetMargin : marginBottom, marginLeft: facetMarginLeft = facetMargin !== void 0 ? facetMargin : marginLeft } = options.facet ?? {};
    facetMarginTop = +facetMarginTop;
    facetMarginRight = +facetMarginRight;
    facetMarginBottom = +facetMarginBottom;
    facetMarginLeft = +facetMarginLeft;
    dimensions.facet = {
      marginTop: facetMarginTop,
      marginRight: facetMarginRight,
      marginBottom: facetMarginBottom,
      marginLeft: facetMarginLeft
    };
  }
  return dimensions;
}
function autoHeight({ x, y, fy, fx }, marks2, { projection: projection2, aspectRatio }, { width, marginTopDefault, marginRightDefault, marginBottomDefault, marginLeftDefault }) {
  const nfy = fy ? fy.scale.domain().length : 1;
  const ar = projectionAspectRatio(projection2, marks2);
  if (ar) {
    const nfx = fx ? fx.scale.domain().length : 1;
    const far = (1.1 * nfy - 0.1) / (1.1 * nfx - 0.1) * ar;
    const lar = Math.max(0.1, Math.min(10, far));
    return Math.round((width - marginLeftDefault - marginRightDefault) * lar + marginTopDefault + marginBottomDefault);
  }
  const ny = y ? isOrdinalScale(y) ? y.scale.domain().length : Math.max(7, 17 / nfy) : 1;
  if (aspectRatio != null) {
    aspectRatio = +aspectRatio;
    if (!(isFinite(aspectRatio) && aspectRatio > 0))
      throw new Error(`invalid aspectRatio: ${aspectRatio}`);
    const ratio = aspectRatioLength("y", y) / (aspectRatioLength("x", x) * aspectRatio);
    const fxb = fx ? fx.scale.bandwidth() : 1;
    const fyb = fy ? fy.scale.bandwidth() : 1;
    const w = fxb * (width - marginLeftDefault - marginRightDefault) - x.insetLeft - x.insetRight;
    return (ratio * w + y.insetTop + y.insetBottom) / fyb + marginTopDefault + marginBottomDefault;
  }
  return !!(y || fy) * Math.max(1, Math.min(60, ny * nfy)) * 20 + !!fx * 30 + 60;
}
function aspectRatioLength(k2, scale) {
  if (!scale)
    throw new Error(`aspectRatio requires ${k2} scale`);
  const { type, domain } = scale;
  let transform;
  switch (type) {
    case "linear":
    case "utc":
    case "time":
      transform = Number;
      break;
    case "pow": {
      const exponent2 = scale.scale.exponent();
      transform = (x) => Math.pow(x, exponent2);
      break;
    }
    case "log":
      transform = Math.log;
      break;
    case "point":
    case "band":
      return domain.length;
    default:
      throw new Error(`unsupported ${k2} scale for aspectRatio: ${type}`);
  }
  const [min2, max2] = extent$1(domain);
  return Math.abs(transform(max2) - transform(min2));
}
function Facets(channelsByScale, options) {
  const { fx, fy } = Scales(channelsByScale, options);
  const fxDomain = fx == null ? void 0 : fx.scale.domain();
  const fyDomain = fy == null ? void 0 : fy.scale.domain();
  return fxDomain && fyDomain ? cross(fxDomain, fyDomain).map(([x, y], i) => ({ x, y, i })) : fxDomain ? fxDomain.map((x, i) => ({ x, i })) : fyDomain ? fyDomain.map((y, i) => ({ y, i })) : void 0;
}
function facetOrder({ x: X, y: Y }) {
  const xi = X && new Map(X.map((v, i) => [v, i]));
  const yi = Y && new Map(Y.map((v, i) => [v, i]));
  return X && Y ? (a2, b) => xi.get(a2.x) - xi.get(b.x) || yi.get(a2.y) - yi.get(b.y) : X ? (a2, b) => xi.get(a2.x) - xi.get(b.x) : (a2, b) => yi.get(a2.y) - yi.get(b.y);
}
function facetGroups(data, { fx, fy }) {
  const I = range(data);
  const FX = fx == null ? void 0 : fx.value;
  const FY = fy == null ? void 0 : fy.value;
  return fx && fy ? rollup(I, (G) => (G.fx = FX[G[0]], G.fy = FY[G[0]], G), (i) => FX[i], (i) => FY[i]) : fx ? rollup(I, (G) => (G.fx = FX[G[0]], G), (i) => FX[i]) : rollup(I, (G) => (G.fy = FY[G[0]], G), (i) => FY[i]);
}
function facetTranslate(fx, fy, { marginTop, marginLeft }) {
  return fx && fy ? ({ x, y }) => `translate(${fx(x) - marginLeft},${fy(y) - marginTop})` : fx ? ({ x }) => `translate(${fx(x) - marginLeft},0)` : ({ y }) => `translate(0,${fy(y) - marginTop})`;
}
function facetExclude(index2) {
  const ex = [];
  const e = new Uint32Array(sum(index2, (d) => d.length));
  for (const i of index2) {
    let n = 0;
    for (const j of index2) {
      if (i === j)
        continue;
      e.set(j, n);
      n += j.length;
    }
    ex.push(e.slice(0, n));
  }
  return ex;
}
const facetAnchors = /* @__PURE__ */ new Map([
  ["top", facetAnchorTop],
  ["right", facetAnchorRight],
  ["bottom", facetAnchorBottom],
  ["left", facetAnchorLeft],
  ["top-left", and(facetAnchorTop, facetAnchorLeft)],
  ["top-right", and(facetAnchorTop, facetAnchorRight)],
  ["bottom-left", and(facetAnchorBottom, facetAnchorLeft)],
  ["bottom-right", and(facetAnchorBottom, facetAnchorRight)],
  ["top-empty", facetAnchorTopEmpty],
  ["right-empty", facetAnchorRightEmpty],
  ["bottom-empty", facetAnchorBottomEmpty],
  ["left-empty", facetAnchorLeftEmpty],
  ["empty", facetAnchorEmpty]
]);
function maybeFacetAnchor(facetAnchor) {
  if (facetAnchor == null)
    return null;
  const anchor = facetAnchors.get(`${facetAnchor}`.toLowerCase());
  if (anchor)
    return anchor;
  throw new Error(`invalid facet anchor: ${facetAnchor}`);
}
function facetAnchorTop(facets, { y: Y }, { y }) {
  return Y ? Y.indexOf(y) === 0 : true;
}
function facetAnchorBottom(facets, { y: Y }, { y }) {
  return Y ? Y.indexOf(y) === Y.length - 1 : true;
}
function facetAnchorLeft(facets, { x: X }, { x }) {
  return X ? X.indexOf(x) === 0 : true;
}
function facetAnchorRight(facets, { x: X }, { x }) {
  return X ? X.indexOf(x) === X.length - 1 : true;
}
function facetAnchorTopEmpty(facets, { y: Y }, { x, y, empty: empty2 }) {
  var _a;
  if (empty2)
    return false;
  const i = Y == null ? void 0 : Y.indexOf(y);
  if (i > 0) {
    const y2 = Y[i - 1];
    return (_a = facets.find((f) => f.x === x && f.y === y2)) == null ? void 0 : _a.empty;
  }
}
function facetAnchorBottomEmpty(facets, { y: Y }, { x, y, empty: empty2 }) {
  var _a;
  if (empty2)
    return false;
  const i = Y == null ? void 0 : Y.indexOf(y);
  if (i < (Y == null ? void 0 : Y.length) - 1) {
    const y2 = Y[i + 1];
    return (_a = facets.find((f) => f.x === x && f.y === y2)) == null ? void 0 : _a.empty;
  }
}
function facetAnchorLeftEmpty(facets, { x: X }, { x, y, empty: empty2 }) {
  var _a;
  if (empty2)
    return false;
  const i = X == null ? void 0 : X.indexOf(x);
  if (i > 0) {
    const x2 = X[i - 1];
    return (_a = facets.find((f) => f.x === x2 && f.y === y)) == null ? void 0 : _a.empty;
  }
}
function facetAnchorRightEmpty(facets, { x: X }, { x, y, empty: empty2 }) {
  var _a;
  if (empty2)
    return false;
  const i = X == null ? void 0 : X.indexOf(x);
  if (i < (X == null ? void 0 : X.length) - 1) {
    const x2 = X[i + 1];
    return (_a = facets.find((f) => f.x === x2 && f.y === y)) == null ? void 0 : _a.empty;
  }
}
function facetAnchorEmpty(facets, channels, { empty: empty2 }) {
  return empty2;
}
function and(a2, b) {
  return function() {
    return a2.apply(null, arguments) && b.apply(null, arguments);
  };
}
function facetFilter(facets, { channels: { fx, fy }, groups: groups2 }) {
  return fx && fy ? facets.map(({ x, y }) => {
    var _a;
    return ((_a = groups2.get(x)) == null ? void 0 : _a.get(y)) ?? [];
  }) : fx ? facets.map(({ x }) => groups2.get(x) ?? []) : facets.map(({ y }) => groups2.get(y) ?? []);
}
function inferFontVariant$2(scale) {
  return isOrdinalScale(scale) && scale.interval === void 0 ? void 0 : "tabular-nums";
}
function maybeAutoTickFormat(tickFormat2, domain) {
  return tickFormat2 === void 0 ? isTemporal(domain) ? formatIsoDate : string : typeof tickFormat2 === "function" ? tickFormat2 : (typeof tickFormat2 === "string" ? isTemporal(domain) ? utcFormat : format$1 : constant)(tickFormat2);
}
function legendRamp(color2, options) {
  let { label = color2.label, tickSize = 6, width = 240, height = 44 + tickSize, marginTop = 18, marginRight = 0, marginBottom = 16 + tickSize, marginLeft = 0, style, ticks: ticks2 = (width - marginLeft - marginRight) / 64, tickFormat: tickFormat2, fontVariant = inferFontVariant$2(color2), round = true, className } = options;
  const context = Context(options);
  className = maybeClassName(className);
  if (tickFormat2 === null)
    tickFormat2 = () => null;
  const svg = create("svg", context).attr("class", className).attr("font-family", "system-ui, sans-serif").attr("font-size", 10).attr("width", width).attr("height", height).attr("viewBox", `0 0 ${width} ${height}`).call((svg2) => svg2.append("style").text(`
        .${className} {
          display: block;
          background: white;
          height: auto;
          height: intrinsic;
          max-width: 100%;
          overflow: visible;
        }
        .${className} text {
          white-space: pre;
        }
      `)).call(applyInlineStyles, style);
  let tickAdjust = (g) => g.selectAll(".tick line").attr("y1", marginTop + marginBottom - height);
  let x;
  const applyRange = round ? (x2, range3) => x2.rangeRound(range3) : (x2, range3) => x2.range(range3);
  const { type, domain, range: range2, interpolate: interpolate2, scale, pivot } = color2;
  if (interpolate2) {
    const interpolator = range2 === void 0 ? interpolate2 : piecewise(interpolate2.length === 1 ? interpolatePiecewise(interpolate2) : interpolate2, range2);
    x = applyRange(scale.copy(), quantize(interpolateNumber(marginLeft, width - marginRight), Math.min(domain.length + (pivot !== void 0), range2 === void 0 ? Infinity : range2.length)));
    const n = 256;
    const canvas = context.document.createElement("canvas");
    canvas.width = n;
    canvas.height = 1;
    const context2 = canvas.getContext("2d");
    for (let i = 0, j = n - 1; i < n; ++i) {
      context2.fillStyle = interpolator(i / j);
      context2.fillRect(i, 0, 1, 1);
    }
    svg.append("image").attr("x", marginLeft).attr("y", marginTop).attr("width", width - marginLeft - marginRight).attr("height", height - marginTop - marginBottom).attr("preserveAspectRatio", "none").attr("xlink:href", canvas.toDataURL());
  } else if (type === "threshold") {
    const thresholds = domain;
    const thresholdFormat = tickFormat2 === void 0 ? (d) => d : typeof tickFormat2 === "string" ? format$1(tickFormat2) : tickFormat2;
    x = applyRange(linear().domain([-1, range2.length - 1]), [marginLeft, width - marginRight]);
    svg.append("g").selectAll().data(range2).enter().append("rect").attr("x", (d, i) => x(i - 1)).attr("y", marginTop).attr("width", (d, i) => x(i) - x(i - 1)).attr("height", height - marginTop - marginBottom).attr("fill", (d) => d);
    ticks2 = map(thresholds, (_, i) => i);
    tickFormat2 = (i) => thresholdFormat(thresholds[i], i);
  } else {
    x = applyRange(band().domain(domain), [marginLeft, width - marginRight]);
    svg.append("g").selectAll().data(domain).enter().append("rect").attr("x", x).attr("y", marginTop).attr("width", Math.max(0, x.bandwidth() - 1)).attr("height", height - marginTop - marginBottom).attr("fill", scale);
    tickAdjust = () => {
    };
  }
  svg.append("g").attr("transform", `translate(0,${height - marginBottom})`).call(axisBottom(x).ticks(Array.isArray(ticks2) ? null : ticks2, typeof tickFormat2 === "string" ? tickFormat2 : void 0).tickFormat(typeof tickFormat2 === "function" ? tickFormat2 : void 0).tickSize(tickSize).tickValues(Array.isArray(ticks2) ? ticks2 : null)).attr("font-size", null).attr("font-family", null).attr("font-variant", impliedString(fontVariant, "normal")).call(tickAdjust).call((g) => g.select(".domain").remove());
  if (label !== void 0) {
    svg.append("text").attr("x", marginLeft).attr("y", marginTop - 6).attr("fill", "currentColor").attr("font-weight", "bold").text(label);
  }
  return svg.node();
}
function maybeScale(scale, key) {
  if (key == null)
    return key;
  const s2 = scale(key);
  if (!s2)
    throw new Error(`scale not found: ${key}`);
  return s2;
}
function legendSwatches(color2, options) {
  if (!isOrdinalScale(color2) && !isThresholdScale(color2))
    throw new Error(`swatches legend requires ordinal or threshold color scale (not ${color2.type})`);
  return legendItems(color2, options, (selection2, scale) => selection2.append("svg").attr("fill", scale.scale).append("rect").attr("width", "100%").attr("height", "100%"), (className) => `.${className}-swatch svg {
        width: var(--swatchWidth);
        height: var(--swatchHeight);
        margin-right: 0.5em;
      }`);
}
function legendSymbols(symbol2, { fill = ((_a) => (_a = symbol2.hint) == null ? void 0 : _a.fill)() !== void 0 ? symbol2.hint.fill : "none", fillOpacity = 1, stroke = ((_b) => (_b = symbol2.hint) == null ? void 0 : _b.stroke)() !== void 0 ? symbol2.hint.stroke : isNoneish(fill) ? "currentColor" : "none", strokeOpacity = 1, strokeWidth = 1.5, r = 4.5, ...options } = {}, scale) {
  const [vf, cf] = maybeColorChannel(fill);
  const [vs, cs] = maybeColorChannel(stroke);
  const sf = maybeScale(scale, vf);
  const ss = maybeScale(scale, vs);
  const size = r * r * Math.PI;
  fillOpacity = maybeNumberChannel(fillOpacity)[1];
  strokeOpacity = maybeNumberChannel(strokeOpacity)[1];
  strokeWidth = maybeNumberChannel(strokeWidth)[1];
  return legendItems(symbol2, options, (selection2) => selection2.append("svg").attr("viewBox", "-8 -8 16 16").attr("fill", vf === "color" ? (d) => sf.scale(d) : null).attr("stroke", vs === "color" ? (d) => ss.scale(d) : null).append("path").attr("d", (d) => {
    const p = pathRound();
    symbol2.scale(d).draw(p, size);
    return p;
  }), (className) => `.${className}-swatch > svg {
        width: var(--swatchWidth);
        height: var(--swatchHeight);
        margin-right: 0.5em;
        overflow: visible;
        fill: ${cf};
        fill-opacity: ${fillOpacity};
        stroke: ${cs};
        stroke-width: ${strokeWidth}px;
        stroke-opacity: ${strokeOpacity};
      }`);
}
function legendItems(scale, options = {}, swatch, swatchStyle) {
  let {
    columns,
    tickFormat: tickFormat2,
    fontVariant = inferFontVariant$2(scale),
    // TODO label,
    swatchSize = 15,
    swatchWidth = swatchSize,
    swatchHeight = swatchSize,
    marginLeft = 0,
    className,
    style,
    width
  } = options;
  const context = Context(options);
  className = maybeClassName(className);
  tickFormat2 = maybeAutoTickFormat(tickFormat2, scale.domain);
  const swatches = create("div", context).attr("class", className).attr("style", `
        --swatchWidth: ${+swatchWidth}px;
        --swatchHeight: ${+swatchHeight}px;
      `);
  let extraStyle;
  if (columns != null) {
    extraStyle = `
      .${className}-swatch {
        display: flex;
        align-items: center;
        break-inside: avoid;
        padding-bottom: 1px;
      }
      .${className}-swatch::before {
        flex-shrink: 0;
      }
      .${className}-label {
        white-space: nowrap;
        overflow: hidden;
        text-overflow: ellipsis;
      }
    `;
    swatches.style("columns", columns).selectAll().data(scale.domain).enter().append("div").attr("class", `${className}-swatch`).call(swatch, scale).call((item) => item.append("div").attr("class", `${className}-label`).attr("title", tickFormat2).text(tickFormat2));
  } else {
    extraStyle = `
      .${className} {
        display: flex;
        align-items: center;
        min-height: 33px;
        flex-wrap: wrap;
      }
      .${className}-swatch {
        display: inline-flex;
        align-items: center;
        margin-right: 1em;
      }
    `;
    swatches.selectAll().data(scale.domain).enter().append("span").attr("class", `${className}-swatch`).call(swatch, scale).append(function() {
      return this.ownerDocument.createTextNode(tickFormat2.apply(this, arguments));
    });
  }
  return swatches.call((div) => div.insert("style", "*").text(`
        .${className} {
          font-family: system-ui, sans-serif;
          font-size: 10px;
          margin-bottom: 0.5em;${marginLeft === void 0 ? "" : `
          margin-left: ${+marginLeft}px;`}${width === void 0 ? "" : `
          width: ${width}px;`}
        }
        ${swatchStyle(className)}
        ${extraStyle}
      `)).style("font-variant", impliedString(fontVariant, "normal")).call(applyInlineStyles, style).node();
}
const legendRegistry = /* @__PURE__ */ new Map([
  ["symbol", legendSymbols],
  ["color", legendColor],
  ["opacity", legendOpacity]
]);
function exposeLegends(scales, context, defaults2 = {}) {
  return (key, options) => {
    if (!legendRegistry.has(key))
      throw new Error(`unknown legend type: ${key}`);
    if (!(key in scales))
      return;
    return legendRegistry.get(key)(scales[key], legendOptions(context, defaults2[key], options), (key2) => scales[key2]);
  };
}
function legendOptions(context, { label, ticks: ticks2, tickFormat: tickFormat2 } = {}, options) {
  return inherit(options, context, { label, ticks: ticks2, tickFormat: tickFormat2 });
}
function legendColor(color2, { legend = true, ...options }) {
  if (legend === true)
    legend = color2.type === "ordinal" ? "swatches" : "ramp";
  if (color2.domain === void 0)
    return;
  switch (`${legend}`.toLowerCase()) {
    case "swatches":
      return legendSwatches(color2, options);
    case "ramp":
      return legendRamp(color2, options);
    default:
      throw new Error(`unknown legend type: ${legend}`);
  }
}
function legendOpacity({ type, interpolate: interpolate2, ...scale }, { legend = true, color: color2 = rgb(0, 0, 0), ...options }) {
  if (!interpolate2)
    throw new Error(`${type} opacity scales are not supported`);
  if (legend === true)
    legend = "ramp";
  if (`${legend}`.toLowerCase() !== "ramp")
    throw new Error(`${legend} opacity legends are not supported`);
  return legendColor({ type, ...scale, interpolate: interpolateOpacity(color2) }, { legend, ...options });
}
function interpolateOpacity(color2) {
  const { r, g, b } = rgb(color2) || rgb(0, 0, 0);
  return (t) => `rgba(${r},${g},${b},${t})`;
}
function Legends(scales, context, options) {
  const legends = [];
  for (const [key, value] of legendRegistry) {
    const o = options[key];
    if ((o == null ? void 0 : o.legend) && key in scales) {
      const legend = value(scales[key], legendOptions(context, scales[key], o), (key2) => scales[key2]);
      if (legend != null)
        legends.push(legend);
    }
  }
  return legends;
}
class Mark {
  constructor(data, channels = {}, options = {}, defaults2) {
    const { facet = "auto", facetAnchor, fx, fy, sort: sort2, dx = 0, dy = 0, margin = 0, marginTop = margin, marginRight = margin, marginBottom = margin, marginLeft = margin, clip: clip2, channels: extraChannels } = options;
    this.data = data;
    this.sort = isDomainSort(sort2) ? sort2 : null;
    this.initializer = initializer(options).initializer;
    this.transform = this.initializer ? options.transform : basic(options).transform;
    if (facet === null || facet === false) {
      this.facet = null;
    } else {
      this.facet = keyword(facet === true ? "include" : facet, "facet", ["auto", "include", "exclude", "super"]);
      this.fx = fx;
      this.fy = fy;
    }
    this.facetAnchor = maybeFacetAnchor(facetAnchor);
    channels = maybeNamed(channels);
    if (extraChannels !== void 0)
      channels = { ...maybeNamed(extraChannels), ...channels };
    if (defaults2 !== void 0)
      channels = { ...styles(this, options, defaults2), ...channels };
    this.channels = Object.fromEntries(Object.entries(channels).map(([name, channel]) => {
      const { value } = channel;
      if (isOptions(value)) {
        channel = { ...channel, value: value.value };
        if (value.scale !== void 0)
          channel.scale = value.scale;
      }
      return [name, channel];
    }).filter(([name, { value, optional }]) => {
      if (value != null)
        return true;
      if (optional)
        return false;
      throw new Error(`missing channel value: ${name}`);
    }));
    this.dx = +dx;
    this.dy = +dy;
    this.marginTop = +marginTop;
    this.marginRight = +marginRight;
    this.marginBottom = +marginBottom;
    this.marginLeft = +marginLeft;
    this.clip = maybeClip(clip2);
    if (this.facet === "super") {
      if (fx || fy)
        throw new Error(`super-faceting cannot use fx or fy`);
      for (const name in this.channels) {
        const { scale } = channels[name];
        if (scale !== "x" && scale !== "y")
          continue;
        throw new Error(`super-faceting cannot use x or y`);
      }
    }
  }
  initialize(facets, facetChannels) {
    let data = arrayify(this.data);
    if (facets === void 0 && data != null)
      facets = [range(data)];
    if (this.transform != null)
      ({ facets, data } = this.transform(data, facets)), data = arrayify(data);
    const channels = Channels(this.channels, data);
    if (this.sort != null)
      channelDomain(channels, facetChannels, data, this.sort);
    return { data, facets, channels };
  }
  filter(index2, channels, values2) {
    for (const name in channels) {
      const { filter: filter2 = defined } = channels[name];
      if (filter2 !== null) {
        const value = values2[name];
        index2 = index2.filter((i) => filter2(value[i]));
      }
    }
    return index2;
  }
  // If there is a projection, and there are both x and y channels (or x1 and
  // y1, or x2 and y2 channels), and those channels are associated with the x
  // and y scale respectively (and not already in screen coordinates as with an
  // initializer), then apply the projection, replacing the x and y values. Note
  // that the x and y scales themselves don’t exist if there is a projection,
  // but whether the channels are associated with scales still determines
  // whether the projection should apply; think of the projection as a
  // combination xy-scale.
  project(channels, values2, context) {
    maybeProject("x", "y", channels, values2, context);
    maybeProject("x1", "y1", channels, values2, context);
    maybeProject("x2", "y2", channels, values2, context);
  }
  scale(channels, scales, context) {
    const values2 = valueObject(channels, scales);
    if (context.projection)
      this.project(channels, values2, context);
    return values2;
  }
}
function marks(...marks2) {
  marks2.plot = Mark.prototype.plot;
  return marks2;
}
const radians = Math.PI / 180;
function maybeInsetX({ inset, insetLeft, insetRight, ...options } = {}) {
  [insetLeft, insetRight] = maybeInset(inset, insetLeft, insetRight);
  return { inset, insetLeft, insetRight, ...options };
}
function maybeInsetY({ inset, insetTop, insetBottom, ...options } = {}) {
  [insetTop, insetBottom] = maybeInset(inset, insetTop, insetBottom);
  return { inset, insetTop, insetBottom, ...options };
}
function maybeInset(inset, inset1, inset2) {
  return inset === void 0 && inset1 === void 0 && inset2 === void 0 ? offset ? [1, 0] : [0.5, 0.5] : [inset1, inset2];
}
function maybeIntervalValue(value, { interval: interval2 }) {
  value = { ...maybeValue(value) };
  value.interval = maybeInterval(value.interval === void 0 ? interval2 : value.interval);
  return value;
}
function maybeIntervalK(k2, maybeInsetK, options, trivial) {
  const { [k2]: v, [`${k2}1`]: v1, [`${k2}2`]: v2 } = options;
  const { value, interval: interval2 } = maybeIntervalValue(v, options);
  if (value == null || interval2 == null && !trivial)
    return options;
  const label = labelof(v);
  if (interval2 == null) {
    let V;
    const kv = { transform: (data) => V || (V = valueof(data, value)), label };
    return {
      ...options,
      [k2]: void 0,
      [`${k2}1`]: v1 === void 0 ? kv : v1,
      [`${k2}2`]: v2 === void 0 ? kv : v2
    };
  }
  let D1, V1;
  function transform(data) {
    if (V1 !== void 0 && data === D1)
      return V1;
    return V1 = map(valueof(D1 = data, value), (v3) => interval2.floor(v3));
  }
  return maybeInsetK({
    ...options,
    [k2]: void 0,
    [`${k2}1`]: v1 === void 0 ? { transform, label } : v1,
    [`${k2}2`]: v2 === void 0 ? { transform: (data) => transform(data).map((v3) => interval2.offset(v3)), label } : v2
  });
}
function maybeIntervalMidK(k2, maybeInsetK, options) {
  const { [k2]: v } = options;
  const { value, interval: interval2 } = maybeIntervalValue(v, options);
  if (value == null || interval2 == null)
    return options;
  return maybeInsetK({
    ...options,
    [k2]: {
      label: labelof(v),
      transform: (data) => {
        const V1 = map(valueof(data, value), (v2) => interval2.floor(v2));
        const V2 = V1.map((v2) => interval2.offset(v2));
        return V1.map(isTemporal(V1) ? (v1, v2) => v1 == null || isNaN(v1 = +v1) || (v2 = V2[v2], v2 == null) || isNaN(v2 = +v2) ? void 0 : new Date((v1 + v2) / 2) : (v1, v2) => v1 == null || (v2 = V2[v2], v2 == null) ? NaN : (+v1 + +v2) / 2);
      }
    }
  });
}
function maybeIntervalX(options = {}) {
  return maybeIntervalK("x", maybeInsetX, options);
}
function maybeIntervalY(options = {}) {
  return maybeIntervalK("y", maybeInsetY, options);
}
function maybeIntervalMidX(options = {}) {
  return maybeIntervalMidK("x", maybeInsetX, options);
}
function maybeIntervalMidY(options = {}) {
  return maybeIntervalMidK("y", maybeInsetY, options);
}
const defaults$5 = {
  ariaLabel: "rule",
  fill: null,
  stroke: "currentColor"
};
class RuleX extends Mark {
  constructor(data, options = {}) {
    const { x, y1: y12, y2, inset = 0, insetTop = inset, insetBottom = inset } = options;
    super(data, {
      x: { value: x, scale: "x", optional: true },
      y1: { value: y12, scale: "y", optional: true },
      y2: { value: y2, scale: "y", optional: true }
    }, options, defaults$5);
    this.insetTop = number(insetTop);
    this.insetBottom = number(insetBottom);
  }
  render(index2, scales, channels, dimensions, context) {
    const { x, y } = scales;
    const { x: X, y1: Y12, y2: Y22 } = channels;
    const { width, height, marginTop, marginRight, marginLeft, marginBottom } = dimensions;
    const { insetTop, insetBottom } = this;
    return create("svg:g", context).call(applyIndirectStyles, this, dimensions).call(applyTransform, this, { x: X && x }, offset, 0).call((g) => g.selectAll().data(index2).enter().append("line").call(applyDirectStyles, this).attr("x1", X ? (i) => X[i] : (marginLeft + width - marginRight) / 2).attr("x2", X ? (i) => X[i] : (marginLeft + width - marginRight) / 2).attr("y1", Y12 && !isCollapsed(y) ? (i) => Y12[i] + insetTop : marginTop + insetTop).attr("y2", Y22 && !isCollapsed(y) ? y.bandwidth ? (i) => Y22[i] + y.bandwidth() - insetBottom : (i) => Y22[i] - insetBottom : height - marginBottom - insetBottom).call(applyChannelStyles, this, channels)).node();
  }
}
class RuleY extends Mark {
  constructor(data, options = {}) {
    const { x1: x12, x2, y, inset = 0, insetRight = inset, insetLeft = inset } = options;
    super(data, {
      y: { value: y, scale: "y", optional: true },
      x1: { value: x12, scale: "x", optional: true },
      x2: { value: x2, scale: "x", optional: true }
    }, options, defaults$5);
    this.insetRight = number(insetRight);
    this.insetLeft = number(insetLeft);
  }
  render(index2, scales, channels, dimensions, context) {
    const { x, y } = scales;
    const { y: Y, x1: X12, x2: X22 } = channels;
    const { width, height, marginTop, marginRight, marginLeft, marginBottom } = dimensions;
    const { insetLeft, insetRight } = this;
    return create("svg:g", context).call(applyIndirectStyles, this, dimensions, context).call(applyTransform, this, { y: Y && y }, 0, offset).call((g) => g.selectAll().data(index2).enter().append("line").call(applyDirectStyles, this).attr("x1", X12 && !isCollapsed(x) ? (i) => X12[i] + insetLeft : marginLeft + insetLeft).attr("x2", X22 && !isCollapsed(x) ? x.bandwidth ? (i) => X22[i] + x.bandwidth() - insetRight : (i) => X22[i] - insetRight : width - marginRight - insetRight).attr("y1", Y ? (i) => Y[i] : (marginTop + height - marginBottom) / 2).attr("y2", Y ? (i) => Y[i] : (marginTop + height - marginBottom) / 2).call(applyChannelStyles, this, channels)).node();
  }
}
function ruleX(data, options) {
  let { x = identity$1, y, y1: y12, y2, ...rest } = maybeIntervalY(options);
  [y12, y2] = maybeOptionalZero(y, y12, y2);
  return new RuleX(data, { ...rest, x, y1: y12, y2 });
}
function ruleY(data, options) {
  let { y = identity$1, x, x1: x12, x2, ...rest } = maybeIntervalX(options);
  [x12, x2] = maybeOptionalZero(x, x12, x2);
  return new RuleY(data, { ...rest, y, x1: x12, x2 });
}
function maybeOptionalZero(x, x12, x2) {
  if (x === void 0) {
    if (x12 === void 0) {
      if (x2 !== void 0)
        return [0, x2];
    } else {
      if (x2 === void 0)
        return [0, x12];
    }
  } else if (x12 === void 0) {
    return x2 === void 0 ? [0, x] : [x, x2];
  } else if (x2 === void 0) {
    return [x, x12];
  }
  return [x12, x2];
}
function template(strings, ...parts) {
  let n = parts.length;
  for (let j = 0, copy2 = true; j < n; ++j) {
    if (typeof parts[j] !== "function") {
      if (copy2) {
        strings = strings.slice();
        copy2 = false;
      }
      strings.splice(j, 2, strings[j] + parts[j] + strings[j + 1]);
      parts.splice(j, 1);
      --j, --n;
    }
  }
  return (i) => {
    let s2 = strings[0];
    for (let j = 0; j < n; ++j) {
      s2 += parts[j](i) + strings[j + 1];
    }
    return s2;
  };
}
const defaults$4 = {
  ariaLabel: "text",
  strokeLinejoin: "round",
  strokeWidth: 3,
  paintOrder: "stroke"
};
const softHyphen = "­";
class Text extends Mark {
  constructor(data, options = {}) {
    const { x, y, text: text2 = isIterable(data) && isTextual(data) ? identity$1 : indexOf, frameAnchor, textAnchor = /right$/i.test(frameAnchor) ? "end" : /left$/i.test(frameAnchor) ? "start" : "middle", lineAnchor = /^top/i.test(frameAnchor) ? "top" : /^bottom/i.test(frameAnchor) ? "bottom" : "middle", lineHeight = 1, lineWidth = Infinity, textOverflow, monospace, fontFamily = monospace ? "ui-monospace, monospace" : void 0, fontSize, fontStyle, fontVariant, fontWeight, rotate } = options;
    const [vrotate, crotate] = maybeNumberChannel(rotate, 0);
    const [vfontSize, cfontSize] = maybeFontSizeChannel(fontSize);
    super(data, {
      x: { value: x, scale: "x", optional: true },
      y: { value: y, scale: "y", optional: true },
      fontSize: { value: vfontSize, optional: true },
      rotate: { value: numberChannel(vrotate), optional: true },
      text: { value: text2, filter: nonempty, optional: true }
    }, options, defaults$4);
    this.rotate = crotate;
    this.textAnchor = impliedString(textAnchor, "middle");
    this.lineAnchor = keyword(lineAnchor, "lineAnchor", ["top", "middle", "bottom"]);
    this.lineHeight = +lineHeight;
    this.lineWidth = +lineWidth;
    this.textOverflow = maybeTextOverflow(textOverflow);
    this.monospace = !!monospace;
    this.fontFamily = string(fontFamily);
    this.fontSize = cfontSize;
    this.fontStyle = string(fontStyle);
    this.fontVariant = string(fontVariant);
    this.fontWeight = string(fontWeight);
    this.frameAnchor = maybeFrameAnchor(frameAnchor);
    if (!(this.lineWidth >= 0))
      throw new Error(`invalid lineWidth: ${lineWidth}`);
    this.splitLines = splitter(this);
    this.clipLine = clipper(this);
  }
  render(index2, scales, channels, dimensions, context) {
    const { x, y } = scales;
    const { x: X, y: Y, rotate: R, text: T, title: TL, fontSize: FS } = channels;
    const { rotate } = this;
    const [cx, cy] = applyFrameAnchor(this, dimensions);
    return create("svg:g", context).call(applyIndirectStyles, this, dimensions, context).call(applyIndirectTextStyles, this, T, dimensions).call(applyTransform, this, { x: X && x, y: Y && y }).call((g) => g.selectAll().data(index2).enter().append("text").call(applyDirectStyles, this).call(applyMultilineText, this, T, TL).attr("transform", template`translate(${X ? (i) => X[i] : cx},${Y ? (i) => Y[i] : cy})${R ? (i) => ` rotate(${R[i]})` : rotate ? ` rotate(${rotate})` : ``}`).call(applyAttr, "font-size", FS && ((i) => FS[i])).call(applyChannelStyles, this, channels)).node();
  }
}
function maybeTextOverflow(textOverflow) {
  return textOverflow == null ? null : keyword(textOverflow, "textOverflow", [
    "clip",
    "ellipsis",
    "clip-start",
    "clip-end",
    "ellipsis-start",
    "ellipsis-middle",
    "ellipsis-end"
  ]).replace(/^(clip|ellipsis)$/, "$1-end");
}
function applyMultilineText(selection2, mark, T, TL) {
  if (!T)
    return;
  const { lineAnchor, lineHeight, textOverflow, splitLines, clipLine: clipLine2 } = mark;
  selection2.each(function(i) {
    const lines = splitLines(formatDefault(T[i])).map(clipLine2);
    const n = lines.length;
    const y = lineAnchor === "top" ? 0.71 : lineAnchor === "bottom" ? 1 - n : (164 - n * 100) / 200;
    if (n > 1) {
      for (let i2 = 0; i2 < n; ++i2) {
        if (!lines[i2])
          continue;
        const tspan = this.ownerDocument.createElementNS(namespaces.svg, "tspan");
        tspan.setAttribute("x", 0);
        tspan.setAttribute("y", `${(y + i2) * lineHeight}em`);
        tspan.textContent = lines[i2];
        this.appendChild(tspan);
      }
    } else {
      if (y)
        this.setAttribute("y", `${y * lineHeight}em`);
      this.textContent = lines[0];
    }
    if (textOverflow && !TL && lines[0] !== T[i]) {
      const title = this.ownerDocument.createElementNS(namespaces.svg, "title");
      title.textContent = T[i];
      this.appendChild(title);
    }
  });
}
function text(data, options = {}) {
  let { x, y, ...remainingOptions } = options;
  if (options.frameAnchor === void 0)
    [x, y] = maybeTuple(x, y);
  return new Text(data, { ...remainingOptions, x, y });
}
function textX(data, options = {}) {
  const { x = identity$1, ...remainingOptions } = options;
  return new Text(data, maybeIntervalMidY({ ...remainingOptions, x }));
}
function textY(data, options = {}) {
  const { y = identity$1, ...remainingOptions } = options;
  return new Text(data, maybeIntervalMidX({ ...remainingOptions, y }));
}
function applyIndirectTextStyles(selection2, mark, T) {
  applyAttr(selection2, "text-anchor", mark.textAnchor);
  applyAttr(selection2, "font-family", mark.fontFamily);
  applyAttr(selection2, "font-size", mark.fontSize);
  applyAttr(selection2, "font-style", mark.fontStyle);
  applyAttr(selection2, "font-variant", mark.fontVariant === void 0 ? inferFontVariant$1(T) : mark.fontVariant);
  applyAttr(selection2, "font-weight", mark.fontWeight);
}
function inferFontVariant$1(T) {
  return isNumeric(T) || isTemporal(T) ? "tabular-nums" : void 0;
}
const fontSizes = /* @__PURE__ */ new Set([
  // global keywords
  "inherit",
  "initial",
  "revert",
  "unset",
  // absolute keywords
  "xx-small",
  "x-small",
  "small",
  "medium",
  "large",
  "x-large",
  "xx-large",
  "xxx-large",
  // relative keywords
  "larger",
  "smaller"
]);
function maybeFontSizeChannel(fontSize) {
  if (fontSize == null || typeof fontSize === "number")
    return [void 0, fontSize];
  if (typeof fontSize !== "string")
    return [fontSize, void 0];
  fontSize = fontSize.trim().toLowerCase();
  return fontSizes.has(fontSize) || /^[+-]?\d*\.?\d+(e[+-]?\d+)?(\w*|%)$/.test(fontSize) ? [void 0, fontSize] : [fontSize, void 0];
}
function lineWrap(input, maxWidth, widthof) {
  const lines = [];
  let lineStart, lineEnd = 0;
  for (const [wordStart, wordEnd, required] of lineBreaks(input)) {
    if (lineStart === void 0)
      lineStart = wordStart;
    if (lineEnd > lineStart && widthof(input, lineStart, wordEnd) > maxWidth) {
      lines.push(input.slice(lineStart, lineEnd) + (input[lineEnd - 1] === softHyphen ? "-" : ""));
      lineStart = wordStart;
    }
    if (required) {
      lines.push(input.slice(lineStart, wordEnd));
      lineStart = void 0;
      continue;
    }
    lineEnd = wordEnd;
  }
  return lines;
}
function* lineBreaks(input) {
  let i = 0, j = 0;
  const n = input.length;
  while (j < n) {
    let k2 = 1;
    switch (input[j]) {
      case softHyphen:
      case "-":
        ++j;
        yield [i, j, false];
        i = j;
        break;
      case " ":
        yield [i, j, false];
        while (input[++j] === " ")
          ;
        i = j;
        break;
      case "\r":
        if (input[j + 1] === "\n")
          ++k2;
      case "\n":
        yield [i, j, true];
        j += k2;
        i = j;
        break;
      default:
        ++j;
        break;
    }
  }
  yield [i, j, true];
}
const defaultWidthMap = {
  a: 56,
  b: 63,
  c: 57,
  d: 63,
  e: 58,
  f: 37,
  g: 62,
  h: 60,
  i: 26,
  j: 26,
  k: 55,
  l: 26,
  m: 88,
  n: 60,
  o: 60,
  p: 62,
  q: 62,
  r: 39,
  s: 54,
  t: 38,
  u: 60,
  v: 55,
  w: 79,
  x: 54,
  y: 55,
  z: 55,
  A: 69,
  B: 67,
  C: 73,
  D: 74,
  E: 61,
  F: 58,
  G: 76,
  H: 75,
  I: 28,
  J: 55,
  K: 67,
  L: 58,
  M: 89,
  N: 75,
  O: 78,
  P: 65,
  Q: 78,
  R: 67,
  S: 65,
  T: 65,
  U: 75,
  V: 69,
  W: 98,
  X: 69,
  Y: 67,
  Z: 67,
  0: 64,
  1: 48,
  2: 62,
  3: 64,
  4: 66,
  5: 63,
  6: 65,
  7: 58,
  8: 65,
  9: 65,
  " ": 29,
  "!": 32,
  '"': 49,
  "'": 31,
  "(": 39,
  ")": 39,
  ",": 31,
  "-": 48,
  ".": 31,
  "/": 32,
  ":": 31,
  ";": 31,
  "?": 52,
  "‘": 31,
  "’": 31,
  "“": 47,
  "”": 47,
  "…": 82
};
function defaultWidth(text2, start2 = 0, end = text2.length) {
  let sum2 = 0;
  for (let i = start2; i < end; i = readCharacter(text2, i)) {
    sum2 += defaultWidthMap[text2[i]] ?? (isPictographic(text2, i) ? 120 : defaultWidthMap.e);
  }
  return sum2;
}
function monospaceWidth(text2, start2 = 0, end = text2.length) {
  let sum2 = 0;
  for (let i = start2; i < end; i = readCharacter(text2, i)) {
    sum2 += isPictographic(text2, i) ? 200 : 100;
  }
  return sum2;
}
function splitter({ monospace, lineWidth, textOverflow }) {
  if (textOverflow != null || lineWidth == Infinity)
    return (text2) => text2.split(/\r\n?|\n/g);
  const widthof = monospace ? monospaceWidth : defaultWidth;
  const maxWidth = lineWidth * 100;
  return (text2) => lineWrap(text2, maxWidth, widthof);
}
function clipper({ monospace, lineWidth, textOverflow }) {
  if (textOverflow == null || lineWidth == Infinity)
    return (text2) => text2;
  const widthof = monospace ? monospaceWidth : defaultWidth;
  const maxWidth = lineWidth * 100;
  switch (textOverflow) {
    case "clip-start":
      return (text2) => clipStart(text2, maxWidth, widthof, "");
    case "clip-end":
      return (text2) => clipEnd(text2, maxWidth, widthof, "");
    case "ellipsis-start":
      return (text2) => clipStart(text2, maxWidth, widthof, "…");
    case "ellipsis-middle":
      return (text2) => clipMiddle(text2, maxWidth, widthof, "…");
    case "ellipsis-end":
      return (text2) => clipEnd(text2, maxWidth, widthof, "…");
  }
}
function cut(text2, width, widthof, inset) {
  const I = [];
  let w = 0;
  for (let i = 0, j = 0, n = text2.length; i < n; i = j) {
    j = readCharacter(text2, i);
    const l = widthof(text2, i, j);
    if (w + l > width) {
      w += inset;
      while (w > width && i > 0)
        j = i, i = I.pop(), w -= widthof(text2, i, j);
      return [i, width - w];
    }
    w += l;
    I.push(i);
  }
  return [-1, 0];
}
function clipEnd(text2, width, widthof, ellipsis) {
  text2 = text2.trim();
  const e = widthof(ellipsis);
  const [i] = cut(text2, width, widthof, e);
  return i < 0 ? text2 : text2.slice(0, i).trimEnd() + ellipsis;
}
function clipMiddle(text2, width, widthof, ellipsis) {
  text2 = text2.trim();
  const w = widthof(text2);
  if (w <= width)
    return text2;
  const e = widthof(ellipsis) / 2;
  const [i, ei] = cut(text2, width / 2, widthof, e);
  const [j] = cut(text2, w - width / 2 - ei + e, widthof, -e);
  return j < 0 ? ellipsis : text2.slice(0, i).trimEnd() + ellipsis + text2.slice(readCharacter(text2, j)).trimStart();
}
function clipStart(text2, width, widthof, ellipsis) {
  text2 = text2.trim();
  const w = widthof(text2);
  if (w <= width)
    return text2;
  const e = widthof(ellipsis);
  const [j] = cut(text2, w - width + e, widthof, -e);
  return j < 0 ? ellipsis : ellipsis + text2.slice(readCharacter(text2, j)).trimStart();
}
const reCombiner = /[\p{Combining_Mark}\p{Emoji_Modifier}]+/uy;
const rePictographic = /\p{Extended_Pictographic}/uy;
function readCharacter(text2, i) {
  i += isSurrogatePair(text2, i) ? 2 : 1;
  if (isCombiner(text2, i))
    i = reCombiner.lastIndex;
  if (isZeroWidthJoiner(text2, i))
    return readCharacter(text2, i + 1);
  return i;
}
function isAscii(text2, i) {
  return text2.charCodeAt(i) < 128;
}
function isSurrogatePair(text2, i) {
  const hi = text2.charCodeAt(i);
  if (hi >= 55296 && hi < 56320) {
    const lo = text2.charCodeAt(i + 1);
    return lo >= 56320 && lo < 57344;
  }
  return false;
}
function isZeroWidthJoiner(text2, i) {
  return text2.charCodeAt(i) === 8205;
}
function isCombiner(text2, i) {
  return isAscii(text2, i) ? false : (reCombiner.lastIndex = i, reCombiner.test(text2));
}
function isPictographic(text2, i) {
  return isAscii(text2, i) ? false : (rePictographic.lastIndex = i, rePictographic.test(text2));
}
const defaults$3 = {
  ariaLabel: "vector",
  fill: "none",
  stroke: "currentColor",
  strokeWidth: 1.5,
  strokeLinejoin: "round",
  strokeLinecap: "round"
};
const defaultRadius = 3.5;
const wingRatio = defaultRadius * 5;
const shapeArrow = {
  draw(context, l, r) {
    const wing = l * r / wingRatio;
    context.moveTo(0, 0);
    context.lineTo(0, -l);
    context.moveTo(-wing, wing - l);
    context.lineTo(0, -l);
    context.lineTo(wing, wing - l);
  }
};
const shapeSpike = {
  draw(context, l, r) {
    context.moveTo(-r, 0);
    context.lineTo(0, -l);
    context.lineTo(r, 0);
  }
};
const shapes = /* @__PURE__ */ new Map([
  ["arrow", shapeArrow],
  ["spike", shapeSpike]
]);
function isShapeObject(value) {
  return value && typeof value.draw === "function";
}
function Shape(shape) {
  if (isShapeObject(shape))
    return shape;
  const value = shapes.get(`${shape}`.toLowerCase());
  if (value)
    return value;
  throw new Error(`invalid shape: ${shape}`);
}
class Vector extends Mark {
  constructor(data, options = {}) {
    const { x, y, r = defaultRadius, length: length2, rotate, shape = shapeArrow, anchor = "middle", frameAnchor } = options;
    const [vl, cl] = maybeNumberChannel(length2, 12);
    const [vr, cr] = maybeNumberChannel(rotate, 0);
    super(data, {
      x: { value: x, scale: "x", optional: true },
      y: { value: y, scale: "y", optional: true },
      length: { value: vl, scale: "length", optional: true },
      rotate: { value: vr, optional: true }
    }, options, defaults$3);
    this.r = +r;
    this.length = cl;
    this.rotate = cr;
    this.shape = Shape(shape);
    this.anchor = keyword(anchor, "anchor", ["start", "middle", "end"]);
    this.frameAnchor = maybeFrameAnchor(frameAnchor);
  }
  render(index2, scales, channels, dimensions, context) {
    const { x, y } = scales;
    const { x: X, y: Y, length: L, rotate: A5 } = channels;
    const { length: length2, rotate, anchor, shape, r } = this;
    const [cx, cy] = applyFrameAnchor(this, dimensions);
    return create("svg:g", context).call(applyIndirectStyles, this, dimensions, context).call(applyTransform, this, { x: X && x, y: Y && y }).call((g) => g.selectAll().data(index2).enter().append("path").call(applyDirectStyles, this).attr("transform", template`translate(${X ? (i) => X[i] : cx},${Y ? (i) => Y[i] : cy})${A5 ? (i) => ` rotate(${A5[i]})` : rotate ? ` rotate(${rotate})` : ``}${anchor === "start" ? `` : anchor === "end" ? L ? (i) => ` translate(0,${L[i]})` : ` translate(0,${length2})` : L ? (i) => ` translate(0,${L[i] / 2})` : ` translate(0,${length2 / 2})`}`).attr("d", L ? (i) => {
      const p = pathRound();
      shape.draw(p, L[i], r);
      return p;
    } : (() => {
      const p = pathRound();
      shape.draw(p, length2, r);
      return p;
    })()).call(applyChannelStyles, this, channels)).node();
  }
}
function vectorX(data, options = {}) {
  const { x = identity$1, ...rest } = options;
  return new Vector(data, { ...rest, x });
}
function vectorY(data, options = {}) {
  const { y = identity$1, ...rest } = options;
  return new Vector(data, { ...rest, y });
}
function maybeData(data, options) {
  if (arguments.length < 2 && !isIterable(data))
    options = data, data = null;
  if (options === void 0)
    options = {};
  return [data, options];
}
function maybeAnchor({ anchor } = {}, anchors) {
  return anchor === void 0 ? anchors[0] : keyword(anchor, "anchor", anchors);
}
function anchorY(options) {
  return maybeAnchor(options, ["left", "right"]);
}
function anchorFy(options) {
  return maybeAnchor(options, ["right", "left"]);
}
function anchorX(options) {
  return maybeAnchor(options, ["bottom", "top"]);
}
function anchorFx(options) {
  return maybeAnchor(options, ["top", "bottom"]);
}
function axisY() {
  const [data, options] = maybeData(...arguments);
  return axisKy("y", anchorY(options), data, options);
}
function axisFy() {
  const [data, options] = maybeData(...arguments);
  return axisKy("fy", anchorFy(options), data, options);
}
function axisX() {
  const [data, options] = maybeData(...arguments);
  return axisKx("x", anchorX(options), data, options);
}
function axisFx() {
  const [data, options] = maybeData(...arguments);
  return axisKx("fx", anchorFx(options), data, options);
}
function axisKy(k2, anchor, data, { color: color2 = "currentColor", opacity: opacity2 = 1, stroke = color2, strokeOpacity = opacity2, strokeWidth = 1, fill = color2, fillOpacity = opacity2, textAnchor, textStroke, textStrokeOpacity, textStrokeWidth, tickSize = k2 === "y" ? 6 : 0, tickPadding, tickRotate, x, marginTop = 20, marginRight = anchor === "right" ? 40 : 0, marginBottom = 20, marginLeft = anchor === "left" ? 40 : 0, label, labelOffset, labelAnchor, ...options }) {
  tickSize = number(tickSize);
  tickPadding = number(tickPadding);
  tickRotate = number(tickRotate);
  if (labelAnchor !== void 0)
    labelAnchor = keyword(labelAnchor, "labelAnchor", ["center", "top", "bottom"]);
  return marks(tickSize && !isNoneish(stroke) ? axisTickKy(k2, anchor, data, {
    stroke,
    strokeOpacity,
    strokeWidth,
    tickSize,
    tickPadding,
    tickRotate,
    x,
    ...options
  }) : null, !isNoneish(fill) ? axisTextKy(k2, anchor, data, {
    fill,
    fillOpacity,
    stroke: textStroke,
    strokeOpacity: textStrokeOpacity,
    strokeWidth: textStrokeWidth,
    textAnchor,
    tickSize,
    tickPadding,
    tickRotate,
    x,
    marginTop,
    marginRight,
    marginBottom,
    marginLeft,
    ...options
  }) : null, !isNoneish(fill) && label !== null ? text([], {
    fill,
    fillOpacity,
    ...options,
    lineWidth: void 0,
    textOverflow: void 0,
    facet: "super",
    x: null,
    y: null,
    initializer: function(data2, facets, channels, scales, dimensions) {
      const scale = scales[k2];
      const { marginTop: marginTop2, marginRight: marginRight2, marginBottom: marginBottom2, marginLeft: marginLeft2 } = k2 === "y" && dimensions.inset || dimensions;
      const cla = labelAnchor ?? (scale.bandwidth ? "center" : "top");
      const clo = labelOffset ?? (anchor === "right" ? marginRight2 : marginLeft2) - 3;
      if (cla === "center") {
        this.textAnchor = void 0;
        this.lineAnchor = anchor === "right" ? "bottom" : "top";
        this.frameAnchor = anchor;
        this.rotate = -90;
      } else {
        this.textAnchor = anchor === "right" ? "end" : "start";
        this.lineAnchor = cla;
        this.frameAnchor = `${cla}-${anchor}`;
        this.rotate = 0;
      }
      this.dy = cla === "top" ? 3 - marginTop2 : cla === "bottom" ? marginBottom2 - 3 : 0;
      this.dx = anchor === "right" ? clo : -clo;
      this.ariaLabel = `${k2}-axis label`;
      return {
        facets: [[0]],
        channels: {
          text: {
            value: [label === void 0 ? inferAxisLabel(k2, scale, cla) : label]
          }
        }
      };
    }
  }) : null);
}
function axisKx(k2, anchor, data, { color: color2 = "currentColor", opacity: opacity2 = 1, stroke = color2, strokeOpacity = opacity2, strokeWidth = 1, fill = color2, fillOpacity = opacity2, textAnchor, textStroke, textStrokeOpacity, textStrokeWidth, tickSize = k2 === "x" ? 6 : 0, tickPadding, tickRotate, y, marginTop = anchor === "top" ? 30 : 0, marginRight = 20, marginBottom = anchor === "bottom" ? 30 : 0, marginLeft = 20, label, labelAnchor, labelOffset, ...options }) {
  tickSize = number(tickSize);
  tickPadding = number(tickPadding);
  tickRotate = number(tickRotate);
  if (labelAnchor !== void 0)
    labelAnchor = keyword(labelAnchor, "labelAnchor", ["center", "left", "right"]);
  return marks(tickSize && !isNoneish(stroke) ? axisTickKx(k2, anchor, data, {
    stroke,
    strokeOpacity,
    strokeWidth,
    tickSize,
    tickPadding,
    tickRotate,
    y,
    ...options
  }) : null, !isNoneish(fill) ? axisTextKx(k2, anchor, data, {
    fill,
    fillOpacity,
    stroke: textStroke,
    strokeOpacity: textStrokeOpacity,
    strokeWidth: textStrokeWidth,
    textAnchor,
    tickSize,
    tickPadding,
    tickRotate,
    y,
    marginTop,
    marginRight,
    marginBottom,
    marginLeft,
    ...options
  }) : null, !isNoneish(fill) && label !== null ? text([], {
    fill,
    fillOpacity,
    ...options,
    lineWidth: void 0,
    textOverflow: void 0,
    facet: "super",
    x: null,
    y: null,
    initializer: function(data2, facets, channels, scales, dimensions) {
      const scale = scales[k2];
      const { marginTop: marginTop2, marginRight: marginRight2, marginBottom: marginBottom2, marginLeft: marginLeft2 } = k2 === "x" && dimensions.inset || dimensions;
      const cla = labelAnchor ?? (scale.bandwidth ? "center" : "right");
      const clo = labelOffset ?? (anchor === "top" ? marginTop2 : marginBottom2) - 3;
      if (cla === "center") {
        this.frameAnchor = anchor;
        this.textAnchor = void 0;
      } else {
        this.frameAnchor = `${anchor}-${cla}`;
        this.textAnchor = cla === "right" ? "end" : "start";
      }
      this.lineAnchor = anchor;
      this.dy = anchor === "top" ? -clo : clo;
      this.dx = cla === "right" ? marginRight2 - 3 : cla === "left" ? 3 - marginLeft2 : 0;
      this.ariaLabel = `${k2}-axis label`;
      return {
        facets: [[0]],
        channels: {
          text: {
            value: [label === void 0 ? inferAxisLabel(k2, scale, cla) : label]
          }
        }
      };
    }
  }) : null);
}
function axisTickKy(k2, anchor, data, { strokeWidth = 1, strokeLinecap = null, strokeLinejoin = null, facetAnchor = anchor + (k2 === "y" ? "-empty" : ""), frameAnchor = anchor, tickSize, inset = 0, insetLeft = inset, insetRight = inset, dx = 0, y = k2 === "y" ? void 0 : null, ...options }) {
  return axisMark(vectorY, k2, `${k2}-axis tick`, data, {
    strokeWidth,
    strokeLinecap,
    strokeLinejoin,
    facetAnchor,
    frameAnchor,
    y,
    ...options,
    dx: anchor === "left" ? +dx - offset + +insetLeft : +dx + offset - insetRight,
    anchor: "start",
    length: tickSize,
    shape: anchor === "left" ? shapeTickLeft : shapeTickRight
  });
}
function axisTickKx(k2, anchor, data, { strokeWidth = 1, strokeLinecap = null, strokeLinejoin = null, facetAnchor = anchor + (k2 === "x" ? "-empty" : ""), frameAnchor = anchor, tickSize, inset = 0, insetTop = inset, insetBottom = inset, dy = 0, x = k2 === "x" ? void 0 : null, ...options }) {
  return axisMark(vectorX, k2, `${k2}-axis tick`, data, {
    strokeWidth,
    strokeLinejoin,
    strokeLinecap,
    facetAnchor,
    frameAnchor,
    x,
    ...options,
    dy: anchor === "bottom" ? +dy - offset - insetBottom : +dy + offset + +insetTop,
    anchor: "start",
    length: tickSize,
    shape: anchor === "bottom" ? shapeTickBottom : shapeTickTop
  });
}
function axisTextKy(k2, anchor, data, { facetAnchor = anchor + (k2 === "y" ? "-empty" : ""), frameAnchor = anchor, tickSize, tickRotate = 0, tickPadding = Math.max(3, 9 - tickSize) + (Math.abs(tickRotate) > 60 ? 4 * Math.cos(tickRotate * radians) : 0), tickFormat: tickFormat2, text: text2 = typeof tickFormat2 === "function" ? tickFormat2 : void 0, textAnchor = Math.abs(tickRotate) > 60 ? "middle" : anchor === "left" ? "end" : "start", lineAnchor = tickRotate > 60 ? "top" : tickRotate < -60 ? "bottom" : "middle", fontVariant, inset = 0, insetLeft = inset, insetRight = inset, dx = 0, y = k2 === "y" ? void 0 : null, ...options }) {
  return axisMark(textY, k2, `${k2}-axis tick label`, data, {
    facetAnchor,
    frameAnchor,
    text: text2 === void 0 ? null : text2,
    textAnchor,
    lineAnchor,
    fontVariant,
    rotate: tickRotate,
    y,
    ...options,
    dx: anchor === "left" ? +dx - tickSize - tickPadding + +insetLeft : +dx + +tickSize + +tickPadding - insetRight
  }, function(scale, ticks2, channels) {
    if (fontVariant === void 0)
      this.fontVariant = inferFontVariant(scale);
    if (text2 === void 0)
      channels.text = inferTextChannel(scale, ticks2, tickFormat2);
  });
}
function axisTextKx(k2, anchor, data, { facetAnchor = anchor + (k2 === "x" ? "-empty" : ""), frameAnchor = anchor, tickSize, tickRotate = 0, tickPadding = Math.max(3, 9 - tickSize) + (Math.abs(tickRotate) >= 10 ? 4 * Math.cos(tickRotate * radians) : 0), tickFormat: tickFormat2, text: text2 = typeof tickFormat2 === "function" ? tickFormat2 : void 0, textAnchor = Math.abs(tickRotate) >= 10 ? tickRotate < 0 ^ anchor === "bottom" ? "start" : "end" : "middle", lineAnchor = Math.abs(tickRotate) >= 10 ? "middle" : anchor === "bottom" ? "top" : "bottom", fontVariant, inset = 0, insetTop = inset, insetBottom = inset, dy = 0, x = k2 === "x" ? void 0 : null, ...options }) {
  return axisMark(textX, k2, `${k2}-axis tick label`, data, {
    facetAnchor,
    frameAnchor,
    text: text2 === void 0 ? null : text2,
    textAnchor,
    lineAnchor,
    fontVariant,
    rotate: tickRotate,
    x,
    ...options,
    dy: anchor === "bottom" ? +dy + +tickSize + +tickPadding - insetBottom : +dy - tickSize - tickPadding + +insetTop
  }, function(scale, ticks2, channels) {
    if (fontVariant === void 0)
      this.fontVariant = inferFontVariant(scale);
    if (text2 === void 0)
      channels.text = inferTextChannel(scale, ticks2, tickFormat2);
  });
}
function gridY() {
  const [data, options] = maybeData(...arguments);
  return gridKy("y", anchorY(options), data, options);
}
function gridFy() {
  const [data, options] = maybeData(...arguments);
  return gridKy("fy", anchorFy(options), data, options);
}
function gridX() {
  const [data, options] = maybeData(...arguments);
  return gridKx("x", anchorX(options), data, options);
}
function gridFx() {
  const [data, options] = maybeData(...arguments);
  return gridKx("fx", anchorFx(options), data, options);
}
function gridKy(k2, anchor, data, { y = k2 === "y" ? void 0 : null, x = null, x1: x12 = anchor === "left" ? x : null, x2 = anchor === "right" ? x : null, ...options }) {
  return axisMark(ruleY, k2, `${k2}-grid`, data, { y, x1: x12, x2, ...gridDefaults(options) });
}
function gridKx(k2, anchor, data, { x = k2 === "x" ? void 0 : null, y = null, y1: y12 = anchor === "top" ? y : null, y2 = anchor === "bottom" ? y : null, ...options }) {
  return axisMark(ruleX, k2, `${k2}-grid`, data, { x, y1: y12, y2, ...gridDefaults(options) });
}
function gridDefaults({ color: color2 = "currentColor", opacity: opacity2 = 0.1, stroke = color2, strokeOpacity = opacity2, strokeWidth = 1, ...options }) {
  return { stroke, strokeOpacity, strokeWidth, ...options };
}
function axisMark(mark, k2, ariaLabel, data, options, initialize) {
  let channels;
  const m = mark(data, initializer(options, function(data2, facets, _channels, scales) {
    const { [k2]: scale } = scales;
    if (!scale)
      throw new Error(`missing scale: ${k2}`);
    let { ticks: ticks2, tickSpacing, interval: interval2 } = options;
    if (isTemporalScale(scale) && typeof ticks2 === "string")
      interval2 = ticks2, ticks2 = void 0;
    if (data2 == null) {
      if (isIterable(ticks2)) {
        data2 = arrayify(ticks2);
      } else if (scale.ticks) {
        if (ticks2 !== void 0) {
          data2 = scale.ticks(ticks2);
        } else {
          interval2 = maybeInterval(interval2 === void 0 ? scale.interval : interval2, scale.type);
          if (interval2 !== void 0) {
            const [min2, max2] = extent$1(scale.domain());
            data2 = interval2.range(min2, interval2.offset(interval2.floor(max2)));
          } else {
            const [min2, max2] = extent$1(scale.range());
            ticks2 = (max2 - min2) / (tickSpacing === void 0 ? k2 === "x" ? 80 : 35 : tickSpacing);
            data2 = scale.ticks(ticks2);
          }
        }
      } else {
        data2 = scale.domain();
      }
      if (k2 === "y" || k2 === "x") {
        facets = [range(data2)];
      } else {
        channels[k2] = { scale: k2, value: identity$1 };
        facets = void 0;
      }
    }
    initialize == null ? void 0 : initialize.call(this, scale, ticks2, channels);
    return {
      data: data2,
      facets,
      channels: Object.fromEntries(Object.entries(channels).map(([name, channel]) => [name, { ...channel, value: valueof(data2, channel.value) }]))
    };
  }));
  if (data == null) {
    channels = m.channels;
    m.channels = {};
  } else {
    channels = {};
  }
  m.ariaLabel = ariaLabel;
  return m;
}
function inferTextChannel(scale, ticks2, tickFormat2) {
  return { value: inferTickFormat(scale, ticks2, tickFormat2) };
}
function inferTickFormat(scale, ticks2, tickFormat2) {
  return scale.tickFormat ? scale.tickFormat(isIterable(ticks2) ? null : ticks2, tickFormat2) : tickFormat2 === void 0 ? formatDefault : typeof tickFormat2 === "string" ? (isTemporal(scale.domain()) ? utcFormat : format$1)(tickFormat2) : constant(tickFormat2);
}
const shapeTickBottom = {
  draw(context, l) {
    context.moveTo(0, 0);
    context.lineTo(0, l);
  }
};
const shapeTickTop = {
  draw(context, l) {
    context.moveTo(0, 0);
    context.lineTo(0, -l);
  }
};
const shapeTickLeft = {
  draw(context, l) {
    context.moveTo(0, 0);
    context.lineTo(-l, 0);
  }
};
const shapeTickRight = {
  draw(context, l) {
    context.moveTo(0, 0);
    context.lineTo(l, 0);
  }
};
function inferFontVariant(scale) {
  return scale.bandwidth && scale.interval === void 0 ? void 0 : "tabular-nums";
}
function inferScaleOrder(scale) {
  return Math.sign(orderof(scale.domain())) * Math.sign(orderof(scale.range()));
}
function inferAxisLabel(key, scale, labelAnchor) {
  const label = scale.label;
  if (scale.bandwidth || !(label == null ? void 0 : label.inferred))
    return label;
  const order = inferScaleOrder(scale);
  return order ? key === "x" || labelAnchor === "center" ? key === "x" === order < 0 ? `← ${label}` : `${label} →` : `${order < 0 ? "↑ " : "↓ "}${label}` : label;
}
const defaults$2 = {
  ariaLabel: "frame",
  fill: "none",
  stroke: "currentColor"
};
const lineDefaults = {
  ariaLabel: "frame",
  fill: null,
  stroke: "currentColor",
  strokeLinecap: "square"
};
class Frame extends Mark {
  constructor(options = {}) {
    const { anchor = null, inset = 0, insetTop = inset, insetRight = inset, insetBottom = inset, insetLeft = inset, rx, ry } = options;
    super(void 0, void 0, options, anchor == null ? defaults$2 : lineDefaults);
    this.anchor = maybeKeyword(anchor, "anchor", ["top", "right", "bottom", "left"]);
    this.insetTop = number(insetTop);
    this.insetRight = number(insetRight);
    this.insetBottom = number(insetBottom);
    this.insetLeft = number(insetLeft);
    this.rx = number(rx);
    this.ry = number(ry);
  }
  render(index2, scales, channels, dimensions, context) {
    const { marginTop, marginRight, marginBottom, marginLeft, width, height } = dimensions;
    const { anchor, insetTop, insetRight, insetBottom, insetLeft, rx, ry } = this;
    const x12 = marginLeft + insetLeft;
    const x2 = width - marginRight - insetRight;
    const y12 = marginTop + insetTop;
    const y2 = height - marginBottom - insetBottom;
    return create(anchor ? "svg:line" : "svg:rect", context).call(applyIndirectStyles, this, dimensions, context).call(applyDirectStyles, this).call(applyTransform, this, {}).call(anchor === "left" ? (line) => line.attr("x1", x12).attr("x2", x12).attr("y1", y12).attr("y2", y2) : anchor === "right" ? (line) => line.attr("x1", x2).attr("x2", x2).attr("y1", y12).attr("y2", y2) : anchor === "top" ? (line) => line.attr("x1", x12).attr("x2", x2).attr("y1", y12).attr("y2", y12) : anchor === "bottom" ? (line) => line.attr("x1", x12).attr("x2", x2).attr("y1", y2).attr("y2", y2) : (rect) => rect.attr("x", x12).attr("y", y12).attr("width", x2 - x12).attr("height", y2 - y12).attr("rx", rx).attr("ry", ry)).node();
  }
}
function frame(options) {
  return new Frame(options);
}
function plot(options = {}) {
  const { facet, style, caption, ariaLabel, ariaDescription } = options;
  const className = maybeClassName(options.className);
  const marks2 = options.marks === void 0 ? [] : flatMarks(options.marks);
  const topFacetState = maybeTopFacet(facet, options);
  const facetStateByMark = /* @__PURE__ */ new Map();
  for (const mark of marks2) {
    const facetState = maybeMarkFacet(mark, topFacetState, options);
    if (facetState)
      facetStateByMark.set(mark, facetState);
  }
  const channelsByScale = /* @__PURE__ */ new Map();
  if (topFacetState)
    addScaleChannels(channelsByScale, [topFacetState]);
  addScaleChannels(channelsByScale, facetStateByMark);
  const axes = flatMarks(inferAxes(marks2, channelsByScale, options));
  for (const mark of axes) {
    const facetState = maybeMarkFacet(mark, topFacetState, options);
    if (facetState)
      facetStateByMark.set(mark, facetState);
  }
  marks2.unshift(...axes);
  const facets = Facets(channelsByScale, options);
  if (facets !== void 0) {
    const topFacetsIndex = topFacetState ? facetFilter(facets, topFacetState) : void 0;
    for (const mark of marks2) {
      if (mark.facet === null || mark.facet === "super")
        continue;
      const facetState = facetStateByMark.get(mark);
      if (facetState === void 0)
        continue;
      facetState.facetsIndex = mark.fx != null || mark.fy != null ? facetFilter(facets, facetState) : topFacetsIndex;
    }
    const nonEmpty = /* @__PURE__ */ new Set();
    for (const { facetsIndex } of facetStateByMark.values()) {
      facetsIndex == null ? void 0 : facetsIndex.forEach((index2, i) => {
        if ((index2 == null ? void 0 : index2.length) > 0) {
          nonEmpty.add(i);
        }
      });
    }
    facets.forEach(0 < nonEmpty.size && nonEmpty.size < facets.length ? (f, i) => f.empty = !nonEmpty.has(i) : (f) => f.empty = false);
    for (const mark of marks2) {
      if (mark.facet === "exclude") {
        const facetState = facetStateByMark.get(mark);
        if (facetState !== void 0)
          facetState.facetsIndex = facetExclude(facetState.facetsIndex);
      }
    }
  }
  for (const key of registry.keys()) {
    if (isScaleOptions(options[key]) && key !== "fx" && key !== "fy") {
      channelsByScale.set(key, []);
    }
  }
  const stateByMark = /* @__PURE__ */ new Map();
  for (const mark of marks2) {
    if (stateByMark.has(mark))
      throw new Error("duplicate mark; each mark must be unique");
    const { facetsIndex, channels: facetChannels } = facetStateByMark.get(mark) ?? {};
    const { data, facets: facets2, channels } = mark.initialize(facetsIndex, facetChannels);
    applyScaleTransforms(channels, options);
    stateByMark.set(mark, { data, facets: facets2, channels });
  }
  const scaleDescriptors = Scales(addScaleChannels(channelsByScale, stateByMark), options);
  const scales = ScaleFunctions(scaleDescriptors);
  const dimensions = Dimensions(scaleDescriptors, marks2, options);
  autoScaleRange(scaleDescriptors, dimensions);
  const { fx, fy } = scales;
  const subdimensions = fx || fy ? innerDimensions(scaleDescriptors, dimensions) : dimensions;
  const superdimensions = fx || fy ? actualDimensions(scales, dimensions) : dimensions;
  const context = Context(options, subdimensions);
  const newByScale = /* @__PURE__ */ new Set();
  for (const [mark, state] of stateByMark) {
    if (mark.initializer != null) {
      const dimensions2 = mark.facet === "super" ? superdimensions : subdimensions;
      const update = mark.initializer(state.data, state.facets, state.channels, scales, dimensions2, context);
      if (update.data !== void 0) {
        state.data = update.data;
      }
      if (update.facets !== void 0) {
        state.facets = update.facets;
      }
      if (update.channels !== void 0) {
        inferChannelScales(update.channels);
        Object.assign(state.channels, update.channels);
        for (const channel of Object.values(update.channels)) {
          const { scale } = channel;
          if (scale != null && registry.get(scale) !== position) {
            applyScaleTransform(channel, options);
            newByScale.add(scale);
          }
        }
        const { fx: fx2, fy: fy2 } = update.channels;
        if (fx2 != null || fy2 != null) {
          const facetState = facetStateByMark.get(mark) ?? { channels: {} };
          if (fx2 != null)
            facetState.channels.fx = fx2;
          if (fy2 != null)
            facetState.channels.fy = fy2;
          facetState.groups = facetGroups(state.data, facetState.channels);
          facetState.facetsIndex = state.facets = facetFilter(facets, facetState);
          facetStateByMark.set(mark, facetState);
        }
      }
    }
  }
  if (newByScale.size) {
    const newChannelsByScale = /* @__PURE__ */ new Map();
    addScaleChannels(newChannelsByScale, stateByMark, (key) => newByScale.has(key));
    addScaleChannels(channelsByScale, stateByMark, (key) => newByScale.has(key));
    const newScaleDescriptors = inheritScaleLabels(Scales(newChannelsByScale, options), scaleDescriptors);
    const newScales = ScaleFunctions(newScaleDescriptors);
    Object.assign(scaleDescriptors, newScaleDescriptors);
    Object.assign(scales, newScales);
  }
  for (const [mark, state] of stateByMark) {
    state.values = mark.scale(state.channels, scales, context);
  }
  const { width, height } = dimensions;
  const svg = create("svg", context).attr("class", className).attr("fill", "currentColor").attr("font-family", "system-ui, sans-serif").attr("font-size", 10).attr("text-anchor", "middle").attr("width", width).attr("height", height).attr("viewBox", `0 0 ${width} ${height}`).attr("aria-label", ariaLabel).attr("aria-description", ariaDescription).call((svg2) => svg2.append("style").text(`
        .${className} {
          display: block;
          background: white;
          height: auto;
          height: intrinsic;
          max-width: 100%;
        }
        .${className} text,
        .${className} tspan {
          white-space: pre;
        }
      `)).call(applyInlineStyles, style).node();
  if (facets !== void 0) {
    const facetDomains = { x: fx == null ? void 0 : fx.domain(), y: fy == null ? void 0 : fy.domain() };
    facets.sort(facetOrder(facetDomains));
    select(svg).selectAll().data(facets).enter().append("g").attr("aria-label", "facet").attr("transform", facetTranslate(fx, fy, dimensions)).each(function(f) {
      var _a;
      let empty2 = true;
      for (const mark of marks2) {
        if (mark.facet === "super")
          continue;
        const { channels, values: values2, facets: indexes } = stateByMark.get(mark);
        if (!(((_a = mark.facetAnchor) == null ? void 0 : _a.call(mark, facets, facetDomains, f)) ?? !f.empty))
          continue;
        let index2 = null;
        if (indexes) {
          index2 = indexes[facetStateByMark.has(mark) ? f.i : 0];
          index2 = mark.filter(index2, channels, values2);
          if (index2.length === 0)
            continue;
          index2.fi = f.i;
        }
        const node = mark.render(index2, scales, values2, subdimensions, context);
        if (node == null)
          continue;
        empty2 = false;
        this.appendChild(node);
      }
      if (empty2)
        this.remove();
    });
  }
  for (const mark of marks2) {
    if (facets !== void 0 && mark.facet !== "super")
      continue;
    const { channels, values: values2, facets: indexes } = stateByMark.get(mark);
    let index2 = null;
    if (indexes) {
      index2 = indexes[0];
      index2 = mark.filter(index2, channels, values2);
      if (index2.length === 0)
        continue;
    }
    const node = mark.render(index2, scales, values2, superdimensions, context);
    if (node != null)
      svg.appendChild(node);
  }
  let figure = svg;
  const legends = Legends(scaleDescriptors, context, options);
  if (caption != null || legends.length > 0) {
    const { document: document2 } = context;
    figure = document2.createElement("figure");
    figure.style.maxWidth = "initial";
    for (const legend of legends)
      figure.appendChild(legend);
    figure.appendChild(svg);
    if (caption != null) {
      const figcaption = document2.createElement("figcaption");
      figcaption.appendChild(caption instanceof Node ? caption : document2.createTextNode(caption));
      figure.appendChild(figcaption);
    }
  }
  figure.scale = exposeScales(scaleDescriptors);
  figure.legend = exposeLegends(scaleDescriptors, context, options);
  const w = consumeWarnings();
  if (w > 0) {
    select(svg).append("text").attr("x", width).attr("y", 20).attr("dy", "-1em").attr("text-anchor", "end").attr("font-family", "initial").text("⚠️").append("title").text(`${w.toLocaleString("en-US")} warning${w === 1 ? "" : "s"}. Please check the console.`);
  }
  return figure;
}
function plotThis({ marks: marks2 = [], ...options } = {}) {
  return plot({ ...options, marks: [...marks2, this] });
}
Mark.prototype.plot = plotThis;
function flatMarks(marks2) {
  return marks2.flat(Infinity).filter((mark) => mark != null).map(markify);
}
function markify(mark) {
  return typeof mark.render === "function" ? mark : new Render(mark);
}
class Render extends Mark {
  constructor(render) {
    if (typeof render !== "function")
      throw new TypeError("invalid mark; missing render function");
    super();
    this.render = render;
  }
  render() {
  }
}
function applyScaleTransforms(channels, options) {
  for (const name in channels)
    applyScaleTransform(channels[name], options);
  return channels;
}
function applyScaleTransform(channel, options) {
  var _a;
  const { scale } = channel;
  if (scale == null)
    return;
  const { type, percent, interval: interval2, transform = percent ? (x) => x * 100 : (_a = maybeInterval(interval2, type)) == null ? void 0 : _a.floor } = options[scale] ?? {};
  if (transform != null)
    channel.value = map(channel.value, transform);
}
function inferChannelScales(channels) {
  for (const name in channels) {
    inferChannelScale(name, channels[name]);
  }
}
function addScaleChannels(channelsByScale, stateByMark, filter2 = yes) {
  for (const { channels } of stateByMark.values()) {
    for (const name in channels) {
      const channel = channels[name];
      const { scale } = channel;
      if (scale != null && filter2(scale)) {
        const scaleChannels = channelsByScale.get(scale);
        if (scaleChannels !== void 0)
          scaleChannels.push(channel);
        else
          channelsByScale.set(scale, [channel]);
      }
    }
  }
  return channelsByScale;
}
function maybeTopFacet(facet, options) {
  if (facet == null)
    return;
  const { x, y } = facet;
  if (x == null && y == null)
    return;
  const data = arrayify(facet.data ?? x ?? y);
  if (data === void 0)
    throw new Error(`missing facet data`);
  const channels = {};
  if (x != null)
    channels.fx = Channel(data, { value: x, scale: "fx" });
  if (y != null)
    channels.fy = Channel(data, { value: y, scale: "fy" });
  applyScaleTransforms(channels, options);
  const groups2 = facetGroups(data, channels);
  return { channels, groups: groups2, data: facet.data };
}
function maybeMarkFacet(mark, topFacetState, options) {
  var _a;
  if (mark.facet === null || mark.facet === "super")
    return;
  const { fx, fy } = mark;
  if (fx != null || fy != null) {
    const data2 = arrayify(mark.data ?? fx ?? fy);
    if (data2 === void 0)
      throw new Error(`missing facet data in ${mark.ariaLabel}`);
    if (data2 === null)
      return;
    const channels2 = {};
    if (fx != null)
      channels2.fx = Channel(data2, { value: fx, scale: "fx" });
    if (fy != null)
      channels2.fy = Channel(data2, { value: fy, scale: "fy" });
    applyScaleTransforms(channels2, options);
    return { channels: channels2, groups: facetGroups(data2, channels2) };
  }
  if (topFacetState === void 0)
    return;
  const { channels, groups: groups2, data } = topFacetState;
  if (mark.facet !== "auto" || mark.data === data)
    return { channels, groups: groups2 };
  if ((groups2.size > 1 || groups2.size === 1 && channels.fx && channels.fy && [...groups2][0][1].size > 1) && ((_a = arrayify(mark.data)) == null ? void 0 : _a.length) === data.length) {
    warn(`Warning: the ${mark.ariaLabel} mark appears to use faceted data, but isn’t faceted. The mark data has the same length as the facet data and the mark facet option is "auto", but the mark data and facet data are distinct. If this mark should be faceted, set the mark facet option to true; otherwise, suppress this warning by setting the mark facet option to false.`);
  }
}
function inferAxes(marks2, channelsByScale, options) {
  let { projection: projection2, x = {}, y = {}, fx = {}, fy = {}, axis: axis2, grid, facet = {}, facet: { axis: facetAxis = axis2, grid: facetGrid } = facet, x: { axis: xAxis = axis2, grid: xGrid = xAxis === null ? null : grid } = x, y: { axis: yAxis = axis2, grid: yGrid = yAxis === null ? null : grid } = y, fx: { axis: fxAxis = facetAxis, grid: fxGrid = fxAxis === null ? null : facetGrid } = fx, fy: { axis: fyAxis = facetAxis, grid: fyGrid = fyAxis === null ? null : facetGrid } = fy } = options;
  if (projection2 || !isScaleOptions(x) && !hasScaleChannel("x", marks2))
    xAxis = xGrid = null;
  if (projection2 || !isScaleOptions(y) && !hasScaleChannel("y", marks2))
    yAxis = yGrid = null;
  if (!channelsByScale.has("fx"))
    fxAxis = fxGrid = null;
  if (!channelsByScale.has("fy"))
    fyAxis = fyGrid = null;
  if (xAxis === void 0)
    xAxis = !hasAxis(marks2, "x");
  if (yAxis === void 0)
    yAxis = !hasAxis(marks2, "y");
  if (fxAxis === void 0)
    fxAxis = !hasAxis(marks2, "fx");
  if (fyAxis === void 0)
    fyAxis = !hasAxis(marks2, "fy");
  if (xAxis === true)
    xAxis = "bottom";
  if (yAxis === true)
    yAxis = "left";
  if (fxAxis === true)
    fxAxis = xAxis === "top" || xAxis === null ? "bottom" : "top";
  if (fyAxis === true)
    fyAxis = yAxis === "right" || yAxis === null ? "left" : "right";
  const axes = [];
  maybeGrid(axes, fyGrid, gridFy, fy);
  maybeAxis(axes, fyAxis, axisFy, "right", "left", facet, fy);
  maybeGrid(axes, fxGrid, gridFx, fx);
  maybeAxis(axes, fxAxis, axisFx, "top", "bottom", facet, fx);
  maybeGrid(axes, yGrid, gridY, y);
  maybeAxis(axes, yAxis, axisY, "left", "right", options, y);
  maybeGrid(axes, xGrid, gridX, x);
  maybeAxis(axes, xAxis, axisX, "bottom", "top", options, x);
  return axes;
}
function maybeAxis(axes, axis2, axisType, primary, secondary, defaults2, options) {
  if (!axis2)
    return;
  const both = isBoth(axis2);
  options = axisOptions(both ? primary : axis2, defaults2, options);
  const { line } = options;
  if ((axisType === axisY || axisType === axisX) && line && !isNone(line))
    axes.push(frame(lineOptions(options)));
  axes.push(axisType(options));
  if (both)
    axes.push(axisType({ ...options, anchor: secondary, label: null }));
}
function maybeGrid(axes, grid, gridType, options) {
  if (!grid || isNone(grid))
    return;
  axes.push(gridType(gridOptions(grid, options)));
}
function isBoth(value) {
  return /^\s*both\s*$/i.test(value);
}
function axisOptions(anchor, defaults2, { line = defaults2.line, ticks: ticks2, tickSize, tickSpacing, tickPadding, tickFormat: tickFormat2, tickRotate, fontVariant, ariaLabel, ariaDescription, label = defaults2.label, labelAnchor, labelOffset }) {
  return {
    anchor,
    line,
    ticks: ticks2,
    tickSize,
    tickSpacing,
    tickPadding,
    tickFormat: tickFormat2,
    tickRotate,
    fontVariant,
    ariaLabel,
    ariaDescription,
    label,
    labelAnchor,
    labelOffset
  };
}
function lineOptions(options) {
  const { anchor, line } = options;
  return { anchor, facetAnchor: anchor + "-empty", stroke: line === true ? void 0 : line };
}
function gridOptions(grid, { stroke = isColor(grid) ? grid : void 0, ticks: ticks2 = isGridTicks(grid) ? grid : void 0, tickSpacing, ariaLabel, ariaDescription }) {
  return {
    stroke,
    ticks: ticks2,
    tickSpacing,
    ariaLabel,
    ariaDescription
  };
}
function isGridTicks(grid) {
  switch (typeof grid) {
    case "number":
      return true;
    case "string":
      return !isColor(grid);
  }
  return isIterable(grid) || typeof (grid == null ? void 0 : grid.range) === "function";
}
function hasAxis(marks2, k2) {
  const prefix = `${k2}-axis `;
  return marks2.some((m) => {
    var _a;
    return (_a = m.ariaLabel) == null ? void 0 : _a.startsWith(prefix);
  });
}
function hasScaleChannel(k2, marks2) {
  for (const mark of marks2) {
    for (const key in mark.channels) {
      if (mark.channels[key].scale === k2) {
        return true;
      }
    }
  }
  return false;
}
function inheritScaleLabels(newScales, scales) {
  for (const key in newScales) {
    const newScale = newScales[key];
    const scale = scales[key];
    if (newScale.label === void 0 && scale) {
      newScale.label = scale.label;
    }
  }
  return newScales;
}
function actualDimensions({ fx, fy }, dimensions) {
  const { marginTop, marginRight, marginBottom, marginLeft, width, height } = outerDimensions(dimensions);
  const fxr = fx && outerRange(fx);
  const fyr = fy && outerRange(fy);
  return {
    marginTop: fy ? fyr[0] : marginTop,
    marginRight: fx ? width - fxr[1] : marginRight,
    marginBottom: fy ? height - fyr[1] : marginBottom,
    marginLeft: fx ? fxr[0] : marginLeft,
    // Some marks, namely the x- and y-axis labels, want to know what the
    // desired (rather than actual) margins are for positioning.
    inset: {
      marginTop: dimensions.marginTop,
      marginRight: dimensions.marginRight,
      marginBottom: dimensions.marginBottom,
      marginLeft: dimensions.marginLeft
    },
    width,
    height
  };
}
function outerRange(scale) {
  const domain = scale.domain();
  let x12 = scale(domain[0]);
  let x2 = scale(domain[domain.length - 1]);
  if (x2 < x12)
    [x12, x2] = [x2, x12];
  return [x12, x2 + scale.bandwidth()];
}
function maybeIdentityY(options = {}) {
  const { y, y1: y12, y2 } = options;
  return y12 === void 0 && y2 === void 0 && y === void 0 ? { ...options, y: identity$1 } : options;
}
function stackY(stack2 = {}, options = {}) {
  if (arguments.length === 1)
    [stack2, options] = mergeOptions(stack2);
  const { x1: x12, x = x12, y, ...rest } = options;
  const [transform, X, y12, y2] = stackAlias(x, y, "y", stack2, rest);
  return { ...transform, x1: x12, x: X, y1: y12, y2, y: mid$1(y12, y2) };
}
function maybeStackY({ y, y1: y12, y2, ...options } = {}) {
  if (y12 === void 0 && y2 === void 0)
    return stackY({ y, ...options });
  [y12, y2] = maybeZero(y, y12, y2);
  return { ...options, y1: y12, y2 };
}
function mergeOptions(options) {
  const { offset: offset2, order, reverse: reverse2, ...rest } = options;
  return [{ offset: offset2, order, reverse: reverse2 }, rest];
}
function stack(x, y = one, ky2, { offset: offset2, order, reverse: reverse2 }, options) {
  const z = maybeZ(options);
  const [X, setX] = maybeColumn(x);
  const [Y12, setY1] = column(y);
  const [Y22, setY2] = column(y);
  offset2 = maybeOffset(offset2);
  order = maybeOrder(order, offset2, ky2);
  return [
    basic(options, (data, facets) => {
      const X3 = x == null ? void 0 : setX(valueof(data, x));
      const Y = valueof(data, y, Float64Array);
      const Z = valueof(data, z);
      const O = order && order(data, X3, Y, Z);
      const n = data.length;
      const Y13 = setY1(new Float64Array(n));
      const Y23 = setY2(new Float64Array(n));
      const facetstacks = [];
      for (const facet of facets) {
        const stacks = X3 ? Array.from(group(facet, (i) => X3[i]).values()) : [facet];
        if (O)
          applyOrder(stacks, O);
        for (const stack2 of stacks) {
          let yn = 0, yp = 0;
          if (reverse2)
            stack2.reverse();
          for (const i of stack2) {
            const y2 = Y[i];
            if (y2 < 0)
              yn = Y23[i] = (Y13[i] = yn) + y2;
            else if (y2 > 0)
              yp = Y23[i] = (Y13[i] = yp) + y2;
            else
              Y23[i] = Y13[i] = yp;
          }
        }
        facetstacks.push(stacks);
      }
      if (offset2)
        offset2(facetstacks, Y13, Y23, Z);
      return { data, facets };
    }),
    X,
    Y12,
    Y22
  ];
}
const stackAlias = stack;
function maybeOffset(offset2) {
  if (offset2 == null)
    return;
  if (typeof offset2 === "function")
    return offset2;
  switch (`${offset2}`.toLowerCase()) {
    case "expand":
    case "normalize":
      return offsetExpand;
    case "center":
    case "silhouette":
      return offsetCenter;
    case "wiggle":
      return offsetWiggle;
  }
  throw new Error(`unknown offset: ${offset2}`);
}
function extent(stack2, Y22) {
  let min2 = 0, max2 = 0;
  for (const i of stack2) {
    const y = Y22[i];
    if (y < min2)
      min2 = y;
    if (y > max2)
      max2 = y;
  }
  return [min2, max2];
}
function offsetExpand(facetstacks, Y12, Y22) {
  for (const stacks of facetstacks) {
    for (const stack2 of stacks) {
      const [yn, yp] = extent(stack2, Y22);
      for (const i of stack2) {
        const m = 1 / (yp - yn || 1);
        Y12[i] = m * (Y12[i] - yn);
        Y22[i] = m * (Y22[i] - yn);
      }
    }
  }
}
function offsetCenter(facetstacks, Y12, Y22) {
  for (const stacks of facetstacks) {
    for (const stack2 of stacks) {
      const [yn, yp] = extent(stack2, Y22);
      for (const i of stack2) {
        const m = (yp + yn) / 2;
        Y12[i] -= m;
        Y22[i] -= m;
      }
    }
    offsetZero(stacks, Y12, Y22);
  }
  offsetCenterFacets(facetstacks, Y12, Y22);
}
function offsetWiggle(facetstacks, Y12, Y22, Z) {
  for (const stacks of facetstacks) {
    const prev = new InternMap();
    let y = 0;
    for (const stack2 of stacks) {
      let j = -1;
      const Fi = stack2.map((i) => Math.abs(Y22[i] - Y12[i]));
      const Df = stack2.map((i) => {
        j = Z ? Z[i] : ++j;
        const value = Y22[i] - Y12[i];
        const diff = prev.has(j) ? value - prev.get(j) : 0;
        prev.set(j, value);
        return diff;
      });
      const Cf1 = [0, ...cumsum(Df)];
      for (const i of stack2) {
        Y12[i] += y;
        Y22[i] += y;
      }
      const s1 = sum(Fi);
      if (s1)
        y -= sum(Fi, (d, i) => (Df[i] / 2 + Cf1[i]) * d) / s1;
    }
    offsetZero(stacks, Y12, Y22);
  }
  offsetCenterFacets(facetstacks, Y12, Y22);
}
function offsetZero(stacks, Y12, Y22) {
  const m = min$1(stacks, (stack2) => min$1(stack2, (i) => Y12[i]));
  for (const stack2 of stacks) {
    for (const i of stack2) {
      Y12[i] -= m;
      Y22[i] -= m;
    }
  }
}
function offsetCenterFacets(facetstacks, Y12, Y22) {
  const n = facetstacks.length;
  if (n === 1)
    return;
  const facets = facetstacks.map((stacks) => stacks.flat());
  const m = facets.map((I) => (min$1(I, (i) => Y12[i]) + max(I, (i) => Y22[i])) / 2);
  const m0 = min$1(m);
  for (let j = 0; j < n; j++) {
    const p = m0 - m[j];
    for (const i of facets[j]) {
      Y12[i] += p;
      Y22[i] += p;
    }
  }
}
function maybeOrder(order, offset2, ky2) {
  if (order === void 0 && offset2 === offsetWiggle)
    return orderInsideOut;
  if (order == null)
    return;
  if (typeof order === "string") {
    switch (order.toLowerCase()) {
      case "value":
      case ky2:
        return orderY;
      case "z":
        return orderZ;
      case "sum":
        return orderSum;
      case "appearance":
        return orderAppearance;
      case "inside-out":
        return orderInsideOut;
    }
    return orderFunction(field(order));
  }
  if (typeof order === "function")
    return orderFunction(order);
  if (Array.isArray(order))
    return orderGiven(order);
  throw new Error(`invalid order: ${order}`);
}
function orderY(data, X, Y) {
  return Y;
}
function orderZ(order, X, Y, Z) {
  return Z;
}
function orderSum(data, X, Y, Z) {
  return orderZDomain(Z, groupSort(range(data), (I) => sum(I, (i) => Y[i]), (i) => Z[i]));
}
function orderAppearance(data, X, Y, Z) {
  return orderZDomain(Z, groupSort(range(data), (I) => X[greatest(I, (i) => Y[i])], (i) => Z[i]));
}
function orderInsideOut(data, X, Y, Z) {
  const I = range(data);
  const K2 = groupSort(I, (I2) => X[greatest(I2, (i) => Y[i])], (i) => Z[i]);
  const sums = rollup(I, (I2) => sum(I2, (i) => Y[i]), (i) => Z[i]);
  const Kp = [], Kn = [];
  let s2 = 0;
  for (const k2 of K2) {
    if (s2 < 0) {
      s2 += sums.get(k2);
      Kp.push(k2);
    } else {
      s2 -= sums.get(k2);
      Kn.push(k2);
    }
  }
  return orderZDomain(Z, Kn.reverse().concat(Kp));
}
function orderFunction(f) {
  return (data) => valueof(data, f);
}
function orderGiven(domain) {
  return (data, X, Y, Z) => orderZDomain(Z, domain);
}
function orderZDomain(Z, domain) {
  if (!Z)
    throw new Error("missing channel: z");
  domain = new InternMap(domain.map((d, i) => [d, i]));
  return Z.map((z) => domain.get(z));
}
function applyOrder(stacks, O) {
  for (const stack2 of stacks) {
    stack2.sort((i, j) => ascendingDefined(O[i], O[j]));
  }
}
const defaults$1 = {
  ariaLabel: "dot",
  fill: "none",
  stroke: "currentColor",
  strokeWidth: 1.5
};
function withDefaultSort(options) {
  return options.sort === void 0 && options.reverse === void 0 ? sort({ channel: "r", order: "descending" }, options) : options;
}
class Dot extends Mark {
  constructor(data, options = {}) {
    const { x, y, r, rotate, symbol: symbol2 = symbolCircle, frameAnchor } = options;
    const [vrotate, crotate] = maybeNumberChannel(rotate, 0);
    const [vsymbol, csymbol] = maybeSymbolChannel(symbol2);
    const [vr, cr] = maybeNumberChannel(r, vsymbol == null ? 3 : 4.5);
    super(data, {
      x: { value: x, scale: "x", optional: true },
      y: { value: y, scale: "y", optional: true },
      r: { value: vr, scale: "r", filter: positive, optional: true },
      rotate: { value: vrotate, optional: true },
      symbol: { value: vsymbol, scale: "auto", optional: true }
    }, withDefaultSort(options), defaults$1);
    this.r = cr;
    this.rotate = crotate;
    this.symbol = csymbol;
    this.frameAnchor = maybeFrameAnchor(frameAnchor);
    const { channels } = this;
    const { symbol: symbolChannel } = channels;
    if (symbolChannel) {
      const { fill: fillChannel, stroke: strokeChannel } = channels;
      symbolChannel.hint = {
        fill: fillChannel ? fillChannel.value === symbolChannel.value ? "color" : "currentColor" : this.fill,
        stroke: strokeChannel ? strokeChannel.value === symbolChannel.value ? "color" : "currentColor" : this.stroke
      };
    }
  }
  render(index2, scales, channels, dimensions, context) {
    const { x, y } = scales;
    const { x: X, y: Y, r: R, rotate: A5, symbol: S } = channels;
    const { r, rotate, symbol: symbol2 } = this;
    const [cx, cy] = applyFrameAnchor(this, dimensions);
    const circle = this.symbol === symbolCircle;
    const size = R ? void 0 : r * r * Math.PI;
    if (negative(r))
      index2 = [];
    return create("svg:g", context).call(applyIndirectStyles, this, dimensions, context).call(applyTransform, this, { x: X && x, y: Y && y }).call((g) => g.selectAll().data(index2).enter().append(circle ? "circle" : "path").call(applyDirectStyles, this).call(circle ? (selection2) => {
      selection2.attr("cx", X ? (i) => X[i] : cx).attr("cy", Y ? (i) => Y[i] : cy).attr("r", R ? (i) => R[i] : r);
    } : (selection2) => {
      selection2.attr("transform", template`translate(${X ? (i) => X[i] : cx},${Y ? (i) => Y[i] : cy})${A5 ? (i) => ` rotate(${A5[i]})` : rotate ? ` rotate(${rotate})` : ``}`).attr("d", R && S ? (i) => {
        const p = pathRound();
        S[i].draw(p, R[i] * R[i] * Math.PI);
        return p;
      } : R ? (i) => {
        const p = pathRound();
        symbol2.draw(p, R[i] * R[i] * Math.PI);
        return p;
      } : S ? (i) => {
        const p = pathRound();
        S[i].draw(p, size);
        return p;
      } : (() => {
        const p = pathRound();
        symbol2.draw(p, size);
        return p;
      })());
    }).call(applyChannelStyles, this, channels)).node();
  }
}
function dot(data, options = {}) {
  let { x, y, ...remainingOptions } = options;
  if (options.frameAnchor === void 0)
    [x, y] = maybeTuple(x, y);
  return new Dot(data, { ...remainingOptions, x, y });
}
class AbstractBar extends Mark {
  constructor(data, channels, options = {}, defaults2) {
    super(data, channels, options, defaults2);
    const { inset = 0, insetTop = inset, insetRight = inset, insetBottom = inset, insetLeft = inset, rx, ry } = options;
    this.insetTop = number(insetTop);
    this.insetRight = number(insetRight);
    this.insetBottom = number(insetBottom);
    this.insetLeft = number(insetLeft);
    this.rx = impliedString(rx, "auto");
    this.ry = impliedString(ry, "auto");
  }
  render(index2, scales, channels, dimensions, context) {
    const { rx, ry } = this;
    return create("svg:g", context).call(applyIndirectStyles, this, dimensions, context).call(this._transform, this, scales).call((g) => g.selectAll().data(index2).enter().append("rect").call(applyDirectStyles, this).attr("x", this._x(scales, channels, dimensions)).attr("width", this._width(scales, channels, dimensions)).attr("y", this._y(scales, channels, dimensions)).attr("height", this._height(scales, channels, dimensions)).call(applyAttr, "rx", rx).call(applyAttr, "ry", ry).call(applyChannelStyles, this, channels)).node();
  }
  _x(scales, { x: X }, { marginLeft }) {
    const { insetLeft } = this;
    return X ? (i) => X[i] + insetLeft : marginLeft + insetLeft;
  }
  _y(scales, { y: Y }, { marginTop }) {
    const { insetTop } = this;
    return Y ? (i) => Y[i] + insetTop : marginTop + insetTop;
  }
  _width({ x }, { x: X }, { marginRight, marginLeft, width }) {
    const { insetLeft, insetRight } = this;
    const bandwidth = X && x ? x.bandwidth() : width - marginRight - marginLeft;
    return Math.max(0, bandwidth - insetLeft - insetRight);
  }
  _height({ y }, { y: Y }, { marginTop, marginBottom, height }) {
    const { insetTop, insetBottom } = this;
    const bandwidth = Y && y ? y.bandwidth() : height - marginTop - marginBottom;
    return Math.max(0, bandwidth - insetTop - insetBottom);
  }
}
const defaults = {
  ariaLabel: "bar"
};
class BarY extends AbstractBar {
  constructor(data, options = {}) {
    const { x, y1: y12, y2 } = options;
    super(data, {
      y1: { value: y12, scale: "y" },
      y2: { value: y2, scale: "y" },
      x: { value: x, scale: "x", type: "band", optional: true }
    }, options, defaults);
  }
  _transform(selection2, mark, { y }) {
    selection2.call(applyTransform, mark, { y }, 0, 0);
  }
  _y({ y }, { y1: Y12, y2: Y22 }, { marginTop }) {
    const { insetTop } = this;
    return isCollapsed(y) ? marginTop + insetTop : (i) => Math.min(Y12[i], Y22[i]) + insetTop;
  }
  _height({ y }, { y1: Y12, y2: Y22 }, { marginTop, marginBottom, height }) {
    const { insetTop, insetBottom } = this;
    return isCollapsed(y) ? height - marginTop - marginBottom - insetTop - insetBottom : (i) => Math.max(0, Math.abs(Y22[i] - Y12[i]) - insetTop - insetBottom);
  }
}
function barY(data, options = { x: indexOf, y2: identity$1 }) {
  return new BarY(data, maybeStackY(maybeIntervalY(maybeIdentityY(options))));
}
const ROOT = document.getElementById("chart");
const DEFAULT_OPTIONS = {
  // This width is really a max-width: the plot adjusts using the available
  // width as well.
  width: 2e3,
  // The height is especially used for the ratio.
  height: 1e3
};
let preparedData;
function prepare() {
  const { airports, flights } = parseAirportsInformation();
  const flightsByAirport = groupFlightsByAirports(flights);
  const byAirport = index(airports, (d) => d.iata);
  const airportsGroupedByStateArray = groups(airports, (d) => d.state);
  const stateInformationSortedArray = airportsGroupedByStateArray.map(([state, airportsInState]) => {
    const totalFlightsInState = sum(airportsInState, ({ iata }) => {
      var _a;
      return (_a = flightsByAirport.get(iata)) == null ? void 0 : _a.total;
    });
    const sorted = sort$1(airportsInState, ({ iata }) => {
      var _a;
      return -((_a = flightsByAirport.get(iata)) == null ? void 0 : _a.total);
    });
    const mostUsedAirportsInState = sorted.slice(0, airportCountPerGroup());
    return {
      state,
      total: totalFlightsInState,
      mostUsedAirports: mostUsedAirportsInState
    };
  }).sort((stateA, stateB) => stateB.total - stateA.total);
  const stateSortedArray = stateInformationSortedArray.map(({ state }) => state);
  const plotData = stateInformationSortedArray.flatMap(({ mostUsedAirports, total, state }) => {
    const enrichedMostUsedAirports = mostUsedAirports.map(({ iata, name, city }, index2) => ({
      state,
      iata,
      name,
      city,
      index: index2,
      // This will be used to have consistent colors.
      ...flightsByAirport.get(iata)
    }));
    const otherTotal = total - sum(mostUsedAirports, ({ iata }) => {
      var _a;
      return (_a = flightsByAirport.get(iata)) == null ? void 0 : _a.total;
    });
    if (otherTotal > 0) {
      enrichedMostUsedAirports.push({
        state,
        iata: "Other",
        total: otherTotal,
        index: enrichedMostUsedAirports.length
      });
    }
    return enrichedMostUsedAirports;
  });
  preparedData = {
    airports,
    flights,
    flightsByAirport,
    byAirport,
    stateInformationSortedArray,
    stateSortedArray,
    plotData
  };
}
function parseAirportsInformation() {
  return {
    airports: csvParse(airportsString),
    flights: csvParse(flightsString)
  };
}
function groupFlightsByAirports(flights) {
  const flightsByAirport = /* @__PURE__ */ new Map();
  for (const { origin, destination, count } of flights) {
    const infoForOriginAirport = flightsByAirport.get(origin) ?? { origin: 0, destination: 0, total: 0 };
    const intCount = Number(count);
    if (Number.isNaN(intCount)) {
      console.error(`Couldn't convert ${count} to number.`);
      continue;
    }
    infoForOriginAirport.origin += intCount;
    infoForOriginAirport.total += intCount;
    flightsByAirport.set(origin, infoForOriginAirport);
    const infoForDestinationAirport = flightsByAirport.get(destination) ?? { origin: 0, destination: 0, total: 0 };
    infoForDestinationAirport.destination += intCount;
    infoForDestinationAirport.total += intCount;
    flightsByAirport.set(destination, infoForDestinationAirport);
  }
  return flightsByAirport;
}
function isReady() {
  return !!preparedData;
}
function addStackedBars() {
  if (!isReady())
    throw new Error("Please prepare the data first.");
  const options = {
    ...DEFAULT_OPTIONS,
    color: { type: "categorical" },
    x: {
      domain: preparedData.stateSortedArray
    },
    y: { grid: true, tickFormat: "~s" },
    marks: [
      barY(preparedData.plotData, {
        x: "state",
        y: "total",
        fill: "index",
        title: (d) => `${d.iata === "Other" ? "Other" : `${d.name}, ${d.city} (${d.iata})`}
${format$1(",")(d.total)} flights`
      }),
      text(preparedData.stateInformationSortedArray, { x: "state", y: "total", text: (d) => format$1(".2~s")(d.total), dy: -10 }),
      ruleY([0])
    ]
  };
  ROOT.append(plot(options));
}
function addDottedBars() {
  if (!isReady())
    throw new Error("Please prepare the data first.");
  const data = [...preparedData.flightsByAirport].flatMap(([iata, { origin, destination }]) => {
    const airportInformation = preparedData.byAirport.get(iata);
    return [
      { ...airportInformation, value: -origin },
      { ...airportInformation, value: destination }
    ];
  }).filter((d) => d.value);
  const options = {
    ...DEFAULT_OPTIONS,
    color: { type: "threshold", domain: [0] },
    x: {
      domain: preparedData.stateSortedArray
    },
    y: {
      grid: true,
      label: "← outward          Number of flights          inward →",
      labelAnchor: "center",
      tickFormat: (v) => format$1("~s")(Math.abs(v)),
      type: "pow",
      exponent: 0.2
    },
    marks: [
      dot(data, {
        x: "state",
        y: "value",
        r: 4,
        stroke: "value",
        strokeWidth: 3,
        title: (d) => `${d.iata === "Other" ? "Other" : `${d.name}, ${d.city} (${d.iata})`}
${format$1(",")(Math.abs(d.value))} ${d.value > 0 ? "inward" : "outward"} flights`
      }),
      ruleY([0])
    ]
  };
  ROOT.append(plot(options));
}
function reset() {
  ROOT.textContent = "";
}
async function runAllTheThings() {
  reset();
  [
    // Force prettier to use a multiline formatting
    "prepare",
    "add-stacked-chart-button",
    "add-dotted-chart-button"
  ].forEach((id2) => document.getElementById(id2).click());
}
function airportCountPerGroup() {
  return document.querySelector("#airport-group-size-input").value;
}
function onGroupSizeInputChange() {
  document.querySelector("#airport-group-size").textContent = airportCountPerGroup();
}
document.getElementById("prepare").addEventListener("click", prepare);
document.getElementById("add-stacked-chart-button").addEventListener("click", addStackedBars);
document.getElementById("add-dotted-chart-button").addEventListener("click", addDottedBars);
document.getElementById("reset").addEventListener("click", reset);
document.getElementById("run-all").addEventListener("click", runAllTheThings);
document.getElementById("airport-group-size-input").addEventListener("input", onGroupSizeInputChange);
onGroupSizeInputChange();
