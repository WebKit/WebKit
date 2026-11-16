//@ skip if $hostOS == "playstation"

// Verifies that an Intl.DateTimeFormat host time zone change is observed
//
// ECMA-262 leaves the *timing* of when a host zone change becomes observable
// implementation-defined, and ECMA-402 only requires that observations stay
// consistent.
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
    // Passing an options object (even an empty one) bypasses the fast cache
    // path, so this resolves the zone through the slow path.
    return new Intl.DateTimeFormat(undefined, {}).resolvedOptions().timeZone;
}

function testTZ(timezones, index)
{
    if (index >= timezones.length)
        return;

    const tz = timezones[index];
    const before = cacheableTZ();
    expect("baseline: cacheable matches slow", before, slowTZ());

    if (!$vm.setHostTimeZone(tz))
        throw new Error("Failed to set host time zone to " + tz);

    // Get outside the VMEntryScope so clearForTimeZoneChange() runs.
    setTimeout(() => {
        const afterInvalidate = cacheableTZ();
        const afterInvalidateSlow = slowTZ();
        expect("after notification fires: cacheable matches slow", afterInvalidate, afterInvalidateSlow);
        expect("after notification fires: new TZ in effect", afterInvalidate, tz);
        testTZ(timezones, index + 1);
    }, 0);
}

testTZ(["UTC", "Pacific/Kiritimati", "America/Los_Angeles"], 0);
