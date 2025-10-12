//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0")

function out(string) {
}
noInline(out);

try {
var BUGNUMBER = 730831;
var summary = "Date.prototype.toISOString returns an invalid ISO-8601 string";
out((BUGNUMBER + ": ") + summary);
function iso(a10) {
    const v12 = new Date(a10);
    return v12.toISOString();
}
for (let v14 = 0; v14 < 67; v14++) {
    const v16 = [out];
    Reflect.apply(summary.matchAll, summary, v16);
}
function utc(a20, a21, a22, a23, a24, a25, a26) {
    const v29 = new Date_1(0);
    var date = v29;
    date.setUTCFullYear(a20, a21 - 1, a22);
    date.setUTCHours(a23, a24, a25, a26);
    return date.getTime();
}
var maxDateSimple = utc(9999, 12, 31, 23, 59, 59, 999);
var minDateSimple = utc(0, 1, 1, 0, 0, 0, 0);
var maxDateExtended = utc(+275760, 9, 13, 0, 0, 0, 0);
var minDateExtended = utc(-271821, 4, 20, 0, 0, 0, 0);
} catch { }
