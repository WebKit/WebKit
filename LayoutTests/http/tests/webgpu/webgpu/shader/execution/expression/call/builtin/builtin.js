/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ /* @returns an ExpressionBuilder that calls the builtin with the given name */
export function builtin(name) {
  return values => `${name}(${values.join(', ')})`;
}
