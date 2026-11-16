// Append-only evaluation log shared by A, B, C.
// If C is mistakenly appended twice to the gather execList, its body would
// run twice and C would appear twice here.
export const log = [];
