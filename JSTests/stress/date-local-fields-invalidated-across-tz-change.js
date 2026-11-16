//@ skip if $hostOS == "playstation"

// A Date caches the broken-down local-time form of its time value. That form is only valid for
// the time zone it was computed in, so a host time zone change has to drop it.

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected ${want}, got ${got}`);
}

function setZone(tz)
{
    if (!$vm.setHostTimeZone(tz))
        throw new Error(`Failed to set host time zone to ${tz}`);
}

// The JITs read the cached breakdown straight out of the Date, so the accessors have to be warm
// for the invalidation to be tested where it actually matters.
function read(date)
{
    return `${date.getHours()}/${date.getDate()}/${date.getTimezoneOffset()}/${date.getUTCHours()}`;
}
noInline(read);

function check(label, date, want)
{
    for (let i = 0; i < testLoopCount; ++i)
        expect(label, read(date), want);
}

const date = new Date(Date.UTC(2007, 0, 1, 12, 34, 56));

setZone("UTC");
// A zone change is only picked up on VM entry, so observe it from a fresh turn.
setTimeout(() => {
    check("UTC", date, "12/1/0/12");

    setZone("Asia/Tokyo");
    setTimeout(() => {
        // The trailing field is a UTC accessor: those are zone-independent and must not move.
        check("Asia/Tokyo", date, "21/1/-540/12");

        setZone("America/Los_Angeles");
        setTimeout(() => {
            check("America/Los_Angeles", date, "4/1/480/12");
        }, 0);
    }, 0);
}, 0);
