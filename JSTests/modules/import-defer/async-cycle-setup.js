globalThis.asyncCycleEvaluations = [];

export const blocker = Promise.withResolvers();
export const aStarted = Promise.withResolvers();
