delete Error.stackTraceLimit
// GetOwnPropertySlot does not materializeErrorInfoIfNeeded because stackString is null.
Object.hasOwn(Error(), "column")
Error.stackTraceLimit = 10
// Now it does
Object.hasOwn(Error(), "column")