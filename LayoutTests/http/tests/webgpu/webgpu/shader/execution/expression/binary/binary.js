/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ /* @returns an ExpressionBuilder that evaluates the binary operation */
export function binary(op) {
  return values => `(${values.join(op)})`;
}
