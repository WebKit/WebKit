function executor(resolve, reject) {
}
noInline(executor); // Ensure `resolve` and `reject`'s materialization.

for (var i = 0; i < 1e6; ++i)
    new Promise(executor);
