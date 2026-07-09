// Unanchored RegExp.test with a counted-quantifier character class (e.g. /\b\d{3}-\d{2}-\d{4}\b/)
// over mostly non-matching subjects. Stresses the Boyer-Moore fast-skip path: the {n} quantifier
// must not prevent BoyerMooreInfo collection, otherwise every subject position re-runs the whole
// body alternative.

function test(re, string)
{
    return re.test(string);
}
noInline(test);

var base = [
    "Unhandled rejection in checkout worker 12 while reconciling the cart totals for session 8842171: the upstream pricing service returned an unexpected payload shape after 3 retries and the fallback path recomputed the discount tier from the cached promotion table before retrying the idempotent write to the orders store",
    "Slow query took 1842 ms on the reporting replica while aggregating weekly engagement metrics for dashboard 91: the planner chose a sequential scan over the composite index because the statistics were stale after the nightly bulk load of 250000 rows finished later than usual and autovacuum had not caught up",
    "Retrying webhook delivery to partner endpoint 44 after receiving status 504 in 30000 ms: the request will be retried with exponential backoff and jitter until the maximum attempt count of 10 is reached, at which point event 77120 will be parked in the dead letter queue for inspection by the integrations team",
    "Customer support note for account 55023: user reported that the ssn on file 078-05-1120 was entered incorrectly during onboarding and asked us to verify the masked value shown in the profile settings page before resubmitting the identity verification form to the compliance vendor for another review pass",
];

var strings = [];
for (var i = 0; i < 16; ++i)
    strings.push(base[i & 3] + " rid " + i);

var re = /\b\d{3}-\d{2}-\d{4}\b/;
var count = 0;
for (var i = 0; i < 3e5; ++i)
    count += test(re, strings[i & 15]) ? 1 : 0;

if (count !== 3e5 / 4)
    throw new Error("bad count: " + count);
