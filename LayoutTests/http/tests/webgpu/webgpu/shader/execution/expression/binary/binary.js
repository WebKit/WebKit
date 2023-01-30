/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ /* @returns an ExpressionBuilder that evaluates a binary operation */
export function binary(op) {
  return values => {
    const values_str = values.map(v => `(${v})`);
    return `(${values_str.join(op)})`;
  };
}
