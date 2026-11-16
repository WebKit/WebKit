//@ runDefault

// Each test262 agent runs in its own VM. Build IntlDateTimeFormat across the
// common construction shapes in N parallel agents and assert nothing throws.

const N = 4;

const workerCode = `
function check(label, cond) {
    if (!cond)
        throw new Error(label);
}

try {
    const a = new Intl.DateTimeFormat("en-US");
    const ra = a.resolvedOptions();
    check("worker a.locale", ra.locale === "en-US");
    check("worker a.timeZone is string", typeof ra.timeZone === "string" && ra.timeZone.length > 0);
    check("worker a.calendar", ra.calendar === "gregory");

    const b = new Intl.DateTimeFormat("en-US");
    check("worker a !== b", a !== b);
    check("worker a.format == b.format same input", a.format(0) === b.format(0));
    check("worker a.tz == b.tz",
        a.resolvedOptions().timeZone === b.resolvedOptions().timeZone);

    const c = new Intl.DateTimeFormat("ja");
    check("worker c.locale", c.resolvedOptions().locale === "ja");

    const d = new Intl.DateTimeFormat("en-US", { timeZone: "Asia/Kolkata" });
    check("worker d.timeZone explicit", d.resolvedOptions().timeZone === "Asia/Kolkata");
    check("worker d format works", typeof d.format(0) === "string");

    const e = new Intl.DateTimeFormat("en-US", { hour: "numeric" });
    check("worker e format works", typeof e.format(0) === "string");

    $.agent.report("ok");
} catch (e) {
    $.agent.report("fail: " + String(e));
}
$.agent.leaving();
`;

for (let i = 0; i < N; ++i)
    $.agent.start(workerCode);

let received = 0;
const reports = [];
while (received < N) {
    const r = waitForReport();
    if (r === null)
        throw new Error(`waitForReport returned null after ${received} of ${N} reports`);
    reports.push(r);
    ++received;
}

for (let i = 0; i < N; ++i) {
    if (reports[i] !== "ok")
        throw new Error(`worker ${i}: ${reports[i]}`);
}
