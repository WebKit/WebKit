// Construction in a fresh realm must agree with construction in the original
// realm at every observation point. Skipped on engines without a realm hook.

const makeGlobal = (typeof newGlobal === "function")
    ? newGlobal
    : (typeof $vm !== "undefined" && typeof $vm.createGlobalObject === "function")
        ? () => $vm.createGlobalObject()
        : null;
if (!makeGlobal)
    quit();

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected ${JSON.stringify(want)}, got ${JSON.stringify(got)}`);
}

// (1) Same locale across realms must produce equal user-visible resolution.
const a = new Intl.DateTimeFormat("en-US");
const g = makeGlobal();
const bResolvedJSON = g.eval(`JSON.stringify(new Intl.DateTimeFormat("en-US").resolvedOptions())`);
const bFormatted = g.eval(`new Intl.DateTimeFormat("en-US").format(0)`);

const aResolvedJSON = JSON.stringify(a.resolvedOptions());
expect("cross-realm en-US resolvedOptions agree", bResolvedJSON, aResolvedJSON);
expect("cross-realm en-US format(0) agrees", bFormatted, a.format(0));

// (2) Reverse direction: populate from realm B first, then construct in main.
const g2 = makeGlobal();
const cResolvedJSON = g2.eval(`JSON.stringify(new Intl.DateTimeFormat("ja").resolvedOptions())`);
const dResolvedJSON = JSON.stringify(new Intl.DateTimeFormat("ja").resolvedOptions());
expect("cross-realm ja resolvedOptions agree (B first)", dResolvedJSON, cResolvedJSON);

// (3) Distinct locales must render distinctly even when both realms have used them.
const eFormat = new Intl.DateTimeFormat("en-US").format(0);
const fFormat = g.eval(`new Intl.DateTimeFormat("ja").format(0)`);
if (eFormat === fFormat)
    throw new Error(`distinct locales rendered identically across realms: en-US="${eFormat}" ja="${fFormat}"`);

// (4) Re-querying after cross-realm constructions of many distinct locales must
// still return realm-consistent resolutions.
for (const tag of ["fr", "de", "es", "it", "pt"])
    g.eval(`new Intl.DateTimeFormat(${JSON.stringify(tag)})`);
for (const tag of ["ko", "zh", "ar"])
    new Intl.DateTimeFormat(tag);

const reAEnUS = new Intl.DateTimeFormat("en-US");
expect("re-query en-US in main: locale", reAEnUS.resolvedOptions().locale, a.resolvedOptions().locale);
expect("re-query en-US in main: format", reAEnUS.format(0), a.format(0));
const reBJa = JSON.parse(g2.eval(`JSON.stringify(new Intl.DateTimeFormat("ja").resolvedOptions())`));
const refJa = JSON.parse(cResolvedJSON);
expect("re-query ja in g2: locale", reBJa.locale, refJa.locale);
