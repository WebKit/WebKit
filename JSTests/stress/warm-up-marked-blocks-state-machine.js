//@ runDefault("--useDollarVM=1", "--warmUpMarkedBlockCount=8", "--warmUpMarkedBlockIdleTimeout=0.02")

// Drives the warm-up supply: it fills while blocks are being asked for, hands the memory back once
// nothing has asked for a while, survives an allocation failure, and comes back once allocation
// works again. Every assertion is "reaches this state eventually", since a libpas helper thread
// drives the transitions on its own.

const pollSeconds = 0.01;
const timeoutSeconds = 20;

const retained = [];

function allocateBlocks() {
    // Retaining these is the point: a heap that can recycle stops asking the allocator for blocks,
    // and sustained block demand is what keeps the supply refilling. The count is kept small so
    // that a run which ends up timing out still reports rather than exhausting memory first.
    for (let i = 0; i < 2000; ++i)
        retained.push({ a: i, b: i, c: i });
}

function waitFor(description, predicate, betweenAttempts = () => { }) {
    for (let attempt = 0; attempt < timeoutSeconds / pollSeconds; ++attempt) {
        if (predicate($vm.warmUpMarkedBlockCount()))
            return;
        betweenAttempts();
        sleepSeconds(pollSeconds);
    }
    throw new Error(`Timed out waiting for ${description}; blocks=${$vm.warmUpMarkedBlockCount()}`);
}

function driveTheSupply() {
    // The supply lives in libpas, so a build that does not use libpas has nothing to drive: it
    // reports itself disabled even though a depth was configured on the command line.
    if (!$vm.warmUpMarkedBlocksAreEnabled())
        return;

    // Demand makes the helper fill the supply.
    allocateBlocks();
    waitFor("the supply to fill", blocks => blocks > 0, allocateBlocks);

    // With no demand at all, the memory goes back rather than staying resident on the chance that
    // somebody will want it. This is what keeps the supply from surviving a quiet period.
    waitFor("the supply to be released when idle", blocks => !blocks);

    // Fresh demand fills it again.
    allocateBlocks();
    waitFor("the supply to refill", blocks => blocks > 0, allocateBlocks);

    // An empty supply cannot tell a filler that has stood down from one refilling as fast as the
    // demand here consumes, so draining is only the setup.
    $vm.setWarmUpMarkedBlockAllocationShouldFail(true);
    waitFor("the supply to drain while allocation fails", blocks => !blocks, allocateBlocks);

    // The real assertion, since a filler that treated the failure as fatal never refills.
    $vm.setWarmUpMarkedBlockAllocationShouldFail(false);
    waitFor("the supply to recover", blocks => blocks > 0, allocateBlocks);
}

driveTheSupply();
