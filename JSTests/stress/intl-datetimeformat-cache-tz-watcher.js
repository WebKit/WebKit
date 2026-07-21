//@ runDefault("--useDollarVM=1")
//@ skip if $hostOS == "playstation"

// This is an end-to-end test for the time zone change notification. It verifies that
// we respond to the notification and that the new tz takes effect when the spec says it should.

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected "${want}", got "${got}"`);
}

function cacheableTZ()
{
    return new Intl.DateTimeFormat().resolvedOptions().timeZone;
}

function slowTZ()
{
    return new Intl.DateTimeFormat(undefined, {}).resolvedOptions().timeZone;
}

const beforeCacheable = cacheableTZ();
const beforeSlow = slowTZ();
expect("baseline: matches", beforeCacheable, beforeSlow);

if (!$vm.setHostTimeZone("Pacific/Kiritimati"))
    throw new Error("Failed to set host time zone to Pacific/Kiritimati");

// Get outside the VMEntryScope
setTimeout(() => {
    const afterInvalidate = cacheableTZ();
    const afterInvalidateSlow = slowTZ();
    expect("after notification fires: cacheable matches slow", afterInvalidate, afterInvalidateSlow);
    expect("after notification fires: new TZ in effect", afterInvalidateSlow, "Pacific/Kiritimati");
}, 100);
