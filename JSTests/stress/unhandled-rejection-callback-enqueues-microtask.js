setUnhandledRejectionCallback(async function() {
    await undefined;
});

async function alwaysThrows() {
    throw new Error;
}

alwaysThrows();