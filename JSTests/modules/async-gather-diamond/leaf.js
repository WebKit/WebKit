// TLA leaf — both A and B (siblings in the diamond) await this module's
// completion, so when leaf fulfills GatherAvailableAncestors visits A and B,
// each of which has C as its single sync ancestor.
await Promise.resolve();
export const leafValue = 1;
