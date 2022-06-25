/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ /* @returns an ExpressionBuilder that evaluates the prefix unary operation */
export function unary(op) {
  return value => `${op}${value}`;
}
