// Indexed accessors on Object.prototype force every array into a mode where
// [[Set]] on an index consults the prototype. The spec builds Intl locale lists
// and formatToParts results with CreateDataPropertyOrThrow, which defines an own
// data property and must never invoke an inherited setter or read an inherited
// getter. Matches V8.
//
// The parts arrays are appended to at increasing indices, and [[Set]] only
// consults a prototype accessor at the exact index being written. We poison a
// whole range so a stray [[Set]] at any element position is observed.

const poisonLimit = 256;
let getFired = 0, setFired = 0;
for (let i = 0; i < poisonLimit; ++i) {
    Object.defineProperty(Object.prototype, i, {
        get() { ++getFired; return "POISON"; },
        set() { ++setFired; },
        configurable: true,
    });
}

function run(label, fn)
{
    getFired = setFired = 0;
    const result = fn();
    if (getFired || setFired)
        throw new Error(`${label}: fired a user callback (get=${getFired} set=${setFired}); a CreateDataPropertyOrThrow step used [[Set]]`);
    return result;
}

function checkParts(label, parts)
{
    if (parts.length > poisonLimit)
        throw new Error(`${label}: ${parts.length} parts exceeds the poisoned range ${poisonLimit}`);
    // Every element must be a real own part, not a poisoned inherited accessor.
    for (let i = 0; i < parts.length; ++i) {
        if (!Object.prototype.hasOwnProperty.call(parts, String(i)))
            throw new Error(`${label}: element ${i} leaked to Object.prototype[${i}] instead of being an own property`);
        const part = parts[i];
        if (typeof part !== "object" || typeof part.type !== "string" || typeof part.value !== "string")
            throw new Error(`${label}: element ${i} is not a well-formed part: ${JSON.stringify(part)}`);
    }
    return parts;
}

// (1) CanonicalizeLocaleList: every Intl constructor with a string locale.
run("DateTimeFormat ctor", () => new Intl.DateTimeFormat("en-US"));
run("NumberFormat ctor", () => new Intl.NumberFormat("en-US"));
run("Collator ctor", () => new Intl.Collator("en-US"));
run("PluralRules ctor", () => new Intl.PluralRules("en-US"));
run("RelativeTimeFormat ctor", () => new Intl.RelativeTimeFormat("en-US"));
run("DisplayNames ctor", () => new Intl.DisplayNames("en-US", { type: "language" }));
run("ListFormat ctor", () => new Intl.ListFormat("en-US"));
run("Segmenter ctor", () => new Intl.Segmenter("en-US"));
run("Locale ctor", () => new Intl.Locale("en-US"));
run("getCanonicalLocales", () => Intl.getCanonicalLocales("en-US"));
run("supportedLocalesOf", () => Intl.DateTimeFormat.supportedLocalesOf("en-US"));
if (typeof Intl.DurationFormat === "function")
    run("DurationFormat ctor", () => new Intl.DurationFormat("en-US"));

// The locale still resolves to the argument, not the poisoned prototype value.
if (new Intl.DateTimeFormat("en-US").resolvedOptions().locale !== "en-US")
    throw new Error("DateTimeFormat resolved locale is not en-US under a bad time");

// (2) formatToParts result arrays.
checkParts("NumberFormat.formatToParts",
    run("NumberFormat.formatToParts", () => new Intl.NumberFormat("en-US").formatToParts(1234.5)));
checkParts("DateTimeFormat.formatToParts",
    run("DateTimeFormat.formatToParts", () => new Intl.DateTimeFormat("en-US").formatToParts(0)));
checkParts("RelativeTimeFormat.formatToParts",
    run("RelativeTimeFormat.formatToParts", () => new Intl.RelativeTimeFormat("en-US").formatToParts(1, "day")));
// numeric:"auto" with a value that has a dedicated word ("today") produces the
// whole result as one literal part, exercising the no-numeric-field path.
checkParts("RelativeTimeFormat.formatToParts auto",
    run("RelativeTimeFormat.formatToParts auto", () => new Intl.RelativeTimeFormat("en-US", { numeric: "auto" }).formatToParts(0, "day")));
checkParts("ListFormat.formatToParts",
    run("ListFormat.formatToParts", () => new Intl.ListFormat("en-US").formatToParts(["a", "b"])));
// A Mongolian disjunction appends a suffix after the last element, exercising the
// trailing-literal flush branch that "and"/"unit" lists never reach.
checkParts("ListFormat.formatToParts disjunction",
    run("ListFormat.formatToParts disjunction", () => new Intl.ListFormat("mn", { type: "disjunction" }).formatToParts(["a", "b", "c"])));
if (typeof Intl.DurationFormat === "function") {
    checkParts("DurationFormat.formatToParts",
        run("DurationFormat.formatToParts", () => new Intl.DurationFormat("en-US").formatToParts({ hours: 1, minutes: 2 })));
    // A digital duration inserts ":" between fields as literal elements.
    checkParts("DurationFormat.formatToParts digital",
        run("DurationFormat.formatToParts digital", () => new Intl.DurationFormat("en-US", { hours: "2-digit", minutes: "2-digit", seconds: "2-digit" }).formatToParts({ hours: 1, minutes: 2, seconds: 3 })));
}

// (3) Range parts variants build the same result arrays.
{
    const dtf = new Intl.DateTimeFormat("en-US");
    if (typeof dtf.formatRangeToParts === "function") {
        checkParts("DateTimeFormat.formatRangeToParts",
            run("DateTimeFormat.formatRangeToParts", () => dtf.formatRangeToParts(0, 86400000)));
        // A full-style Japanese range ends with a literal, exercising the interval
        // trailing-literal flush branch.
        checkParts("DateTimeFormat.formatRangeToParts full",
            run("DateTimeFormat.formatRangeToParts full", () => new Intl.DateTimeFormat("ja-JP", { dateStyle: "full" }).formatRangeToParts(0, 86400000)));
    }
    const nf = new Intl.NumberFormat("en-US");
    if (typeof nf.formatRangeToParts === "function")
        checkParts("NumberFormat.formatRangeToParts",
            run("NumberFormat.formatRangeToParts", () => nf.formatRangeToParts(1, 100)));
}
