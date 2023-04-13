/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { dataCache } from '../../../../common/framework/data_cache.js';
import { deserializeComparator } from '../../../util/compare.js';
import { Scalar, Vector, serializeValue, deserializeValue } from '../../../util/conversion.js';
import {
  deserializeF32Interval,
  F32Interval,
  serializeF32Interval,
} from '../../../util/f32_interval.js';

/** serializeExpectation() converts an Expectation to a SerializedExpectation */
export function serializeExpectation(e) {
  if (e instanceof Scalar || e instanceof Vector) {
    return { kind: 'value', value: serializeValue(e) };
  }
  if (e instanceof F32Interval) {
    return { kind: 'interval', value: serializeF32Interval(e) };
  }
  if (e instanceof Array) {
    return { kind: 'intervals', value: e.map(i => serializeF32Interval(i)) };
  }
  if (e instanceof Function) {
    const comp = e;
    if (comp !== undefined) {
      // if blocks used to refine the type of comp.kind, otherwise it is
      // actually the union of the string values
      if (comp.kind === 'anyOf') {
        return { kind: 'comparator', value: { kind: comp.kind, data: comp.data } };
      }
      if (comp.kind === 'skipUndefined') {
        return { kind: 'comparator', value: { kind: comp.kind, data: comp.data } };
      }
    }
    throw 'cannot serialize comparator';
  }
  throw 'cannot serialize expectation';
}

/** deserializeExpectation() converts a SerializedExpectation to a Expectation */
export function deserializeExpectation(data) {
  switch (data.kind) {
    case 'value':
      return deserializeValue(data.value);
    case 'interval':
      return deserializeF32Interval(data.value);
    case 'intervals':
      return data.value.map(i => deserializeF32Interval(i));
    case 'comparator':
      return deserializeComparator(data.value);
  }
}

/**
 * SerializedCase holds the serialized form of a Case.
 * This form can be safely encoded to JSON.
 */

/** serializeCase() converts an Case to a SerializedCase */
export function serializeCase(c) {
  return {
    input: c.input instanceof Array ? c.input.map(v => serializeValue(v)) : serializeValue(c.input),
    expected: serializeExpectation(c.expected),
  };
}

/** serializeCase() converts an SerializedCase to a Case */
export function deserializeCase(data) {
  return {
    input:
      data.input instanceof Array
        ? data.input.map(v => deserializeValue(v))
        : deserializeValue(data.input),
    expected: deserializeExpectation(data.expected),
  };
}

/** CaseListBuilder is a function that builds a CaseList */

/**
 * CaseCache is a cache of CaseList.
 * CaseCache implements the Cacheable interface, so the cases can be pre-built
 * and stored in the data cache, reducing computation costs at CTS runtime.
 */
export class CaseCache {
  /**
   * Constructor
   * @param name the name of the cache. This must be globally unique.
   * @param builders a Record of case-list name to case-list builder.
   */
  constructor(name, builders) {
    this.path = `webgpu/shader/execution/case-cache/${name}.json`;
    this.builders = builders;
  }

  /** get() returns the list of cases with the given name */
  async get(name) {
    const data = await dataCache.fetch(this);
    return data[name];
  }

  /**
   * build() implements the Cacheable.build interface.
   * @returns the data.
   */
  build() {
    const built = {};
    for (const name in this.builders) {
      const cases = this.builders[name]();
      built[name] = cases;
    }
    return Promise.resolve(built);
  }

  /**
   * serialize() implements the Cacheable.serialize interface.
   * @returns the serialized data.
   */
  serialize(data) {
    const serialized = {};
    for (const name in data) {
      serialized[name] = data[name].map(c => serializeCase(c));
    }
    return JSON.stringify(serialized);
  }

  /**
   * deserialize() implements the Cacheable.deserialize interface.
   * @returns the deserialize data.
   */
  deserialize(serialized) {
    const data = JSON.parse(serialized);
    const casesByName = {};
    for (const name in data) {
      const cases = data[name].map(caseData => deserializeCase(caseData));
      casesByName[name] = cases;
    }
    return casesByName;
  }
}

export function makeCaseCache(name, builders) {
  return new CaseCache(name, builders);
}
