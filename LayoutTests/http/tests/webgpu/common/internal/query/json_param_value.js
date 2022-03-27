/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import { assert, sortObjectByKey } from '../../util/util.js';
// JSON can't represent various values and by default stores them as `null`.
// Instead, storing them as a magic string values in JSON.
const jsUndefinedMagicValue = '_undef_';
const jsNaNMagicValue = '_nan_';
const jsPositiveInfinityMagicValue = '_posinfinity_';
const jsNegativeInfinityMagicValue = '_neginfinity_';

// -0 needs to be handled separately, because -0 === +0 returns true. Not
// special casing +0/0, since it behaves intuitively. Assuming that if -0 is
// being used, the differentiation from +0 is desired.
const jsNegativeZeroMagicValue = '_negzero_';

const toStringMagicValue = new Map([
[undefined, jsUndefinedMagicValue],
[NaN, jsNaNMagicValue],
[Number.POSITIVE_INFINITY, jsPositiveInfinityMagicValue],
[Number.NEGATIVE_INFINITY, jsNegativeInfinityMagicValue]
// No -0 handling because it is special cased.
]);

const fromStringMagicValue = new Map([
[jsUndefinedMagicValue, undefined],
[jsNaNMagicValue, NaN],
[jsPositiveInfinityMagicValue, Number.POSITIVE_INFINITY],
[jsNegativeInfinityMagicValue, Number.NEGATIVE_INFINITY],
// -0 is handled in this direction because there is no comparison issue.
[jsNegativeZeroMagicValue, -0]]);


function stringifyFilter(k, v) {
  // Make sure no one actually uses a magic value as a parameter.
  if (typeof v === 'string') {
    assert(
    !fromStringMagicValue.has(v),
    `${v} is a magic value for stringification, so cannot be used`);


    assert(
    v !== jsNegativeZeroMagicValue,
    `${v} is a magic value for stringification, so cannot be used`);

  }

  if (Object.is(v, -0)) {
    return jsNegativeZeroMagicValue;
  }

  return toStringMagicValue.has(v) ? toStringMagicValue.get(v) : v;
}

export function stringifyParamValue(value) {
  return JSON.stringify(value, stringifyFilter);
}

/**
 * Like stringifyParamValue but sorts dictionaries by key, for hashing.
 */
export function stringifyParamValueUniquely(value) {
  return JSON.stringify(value, (k, v) => {
    if (typeof v === 'object' && v !== null) {
      return sortObjectByKey(v);
    }

    return stringifyFilter(k, v);
  });
}

// 'any' is part of the JSON.parse reviver interface, so cannot be avoided.

function parseParamValueReviver(k, v) {
  if (fromStringMagicValue.has(v)) {
    return fromStringMagicValue.get(v);
  }

  return v;
}

export function parseParamValue(s) {
  return JSON.parse(s, parseParamValueReviver);
}
//# sourceMappingURL=json_param_value.js.map