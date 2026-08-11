//@ runDefault("--useDollarVM=1")
//@ skip if $hostOS == "playstation"

// The two construction shapes (`new Intl.DateTimeFormat()` and
// `new Intl.DateTimeFormat(undefined, {})`) must always agree on the resolved time
// zone across host TZ changes. A host TZ change only takes effect at the next VM
// entry, so each change is observed across a setTimeout boundary: there the cached
// no-arg formatter must have moved to the new zone in step with the non-cacheable
// shape.

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected ${JSON.stringify(want)}, got ${JSON.stringify(got)}`);
}

function cacheableTZ() { return new Intl.DateTimeFormat().resolvedOptions().timeZone; }
function slowTZ() { return new Intl.DateTimeFormat(undefined, {}).resolvedOptions().timeZone; }
function cacheableFormat(ts) { return new Intl.DateTimeFormat("en-US").format(ts); }
function slowFormat(ts) { return new Intl.DateTimeFormat("en-US", {}).format(ts); }

const ts = 0; // 1970-01-01T00:00:00Z lands on different calendar days east vs west of UTC.
expect("baseline cacheable matches slow on TZ", cacheableTZ(), slowTZ());
expect("baseline cacheable matches slow on format", cacheableFormat(ts), slowFormat(ts));

const tzs = ["Asia/Tokyo", "America/New_York", "Europe/Paris", "UTC", "Australia/Sydney", "UTC"];

function runCycle(i)
{
    if (i >= tzs.length)
        return;
    const tz = tzs[i];
    if (!$vm.setHostTimeZone(tz))
        throw new Error(`Failed to set host time zone to ${tz}`);
    setTimeout(() => {
        expect(`after setTZ(${tz}): cacheable moved to new host`, cacheableTZ(), tz);
        expect(`after setTZ(${tz}): cacheable matches slow on TZ`, cacheableTZ(), slowTZ());
        expect(`after setTZ(${tz}): cacheable matches slow on format`, cacheableFormat(ts), slowFormat(ts));
        runCycle(i + 1);
    }, 0);
}

runCycle(0);
