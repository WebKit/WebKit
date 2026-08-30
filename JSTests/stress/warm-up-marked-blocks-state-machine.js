//@ runDefault("--useDollarVM=1", "--warmUpMarkedBlockCount=8", "--warmUpMarkedBlockIdleTimeout=0.2")

// Drives the warm-up supply through its whole state machine: fill, release on idle, restart,
// stand-down on allocation failure, and recovery once allocation works again. Every assertion is
// "reaches this state eventually", since a helper thread drives the transitions.

const pollSeconds = 0.05;
const timeoutSeconds = 20;

const retained = [];

function allocateBlocks() {
    // Retaining these is the point: a heap that can recycle stops asking the allocator for blocks,
    // and sustained block demand is what the stand-down and restart paths are driven by. The count
    // is kept small so that a run which ends up timing out still reports rather than exhausting
    // memory first.
    for (let i = 0; i < 2000; ++i)
        retained.push({ a: i, b: i, c: i });
}

function waitFor(description, predicate, betweenAttempts = () => { }) {
    for (let attempt = 0; attempt < timeoutSeconds / pollSeconds; ++attempt) {
        if (predicate($vm.warmUpMarkedBlockState()))
            return;
        betweenAttempts();
        sleepSeconds(pollSeconds);
    }
    const state = $vm.warmUpMarkedBlockState();
    throw new Error(`Timed out waiting for ${description}; blocks=${state.blocks} phase=${state.phase}`);
}

const isFilled = state => state.phase === "armed" && state.blocks > 0;

// Demand starts the helper, which arms to the configured depth and fills.
allocateBlocks();
waitFor("the supply to fill", isFilled, allocateBlocks);

// With no demand at all, the helper hands everything back and shuts down.
waitFor("the supply to be released when idle", state => state.phase === "stopped");

// Fresh demand brings it back.
allocateBlocks();
waitFor("the helper to restart", isFilled, allocateBlocks);

// An allocation failure makes it stand down rather than spin against an exhausted heap.
$vm.setWarmUpMarkedBlockAllocationShouldFail(true);
waitFor("the helper to stand down", state => state.phase === "standingDown", allocateBlocks);

// Standing down must not be permanent: the idle timeout still fires while demand continues, and
// lifts it once allocation works again.
$vm.setWarmUpMarkedBlockAllocationShouldFail(false);
waitFor("the stand-down to lift", isFilled, allocateBlocks);
