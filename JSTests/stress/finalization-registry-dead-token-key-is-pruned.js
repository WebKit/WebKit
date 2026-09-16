//@ runDefault

// m_deadRegistrations is keyed by unregister token, and those keys are weakly held. A bucket
// populated while its token was alive used to keep the token's address as a key after the token was
// collected, so a freshly allocated cell landing at that address would be treated as the token:
// unregister() reported a match for an unrelated object and dropped the bucket's pending holdings,
// whose cleanup callbacks were then never delivered.

function assert(condition, message)
{
    if (!condition)
        throw new Error(message);
}

const registrationCount = 8;
const registry = new FinalizationRegistry(() => { });

(function () {
    let token = {};
    (function () {
        for (let i = 0; i < registrationCount; ++i)
            registry.register({}, "holdings-" + i, token);
    })();
    // The targets die while the token is still live, so the registrations move into the bucket keyed
    // by the token. The cleanup callback is only scheduled, so they stay pending.
    gc();
    assert(finalizationRegistryDeadCount(registry) === registrationCount, "targets should be dead but their holdings still pending");
})();

// The token is unreachable now, so its bucket has to be pruned.
gc();
assert(finalizationRegistryDeadCount(registry) === registrationCount, "holdings should outlive their unregister token");

for (let i = 0; i < 10000; ++i)
    assert(!registry.unregister({}), "unregister() matched an object that was never used as a token");

assert(finalizationRegistryDeadCount(registry) === registrationCount, "pending holdings should not have been dropped");
