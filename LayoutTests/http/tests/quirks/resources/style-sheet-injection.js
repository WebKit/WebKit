class StyleSheetQuirkHelpers {
    static sheetColoringTargetGreenAndDeclaringOneFontFace = "#target { color: rgb(0, 128, 0); } @font-face { font-family: quirk-injected; src: local(Helvetica); }";

    static enableSiteSpecificQuirksResolvedFromTestHost()
    {
        internals.setTopDocumentHostForQuirks("127.0.0.1");
    }

    static installQuirk(matches, queryContains)
    {
        const quirk = {
            matches: [matches],
            behaviors: [{ id: "NeedsStyleSheetInjectedQuirk", styleSheet: this.sheetColoringTargetGreenAndDeclaringOneFontFace }]
        };
        if (queryContains)
            quirk.queryContains = queryContains;

        internals.setQuirkTableForTesting({ quirks: [quirk] });
        this.enableSiteSpecificQuirksResolvedFromTestHost();
    }

    static state()
    {
        const injectedSheetCopyCount = document.fonts.size;
        return [
            "quirk active: " + (window.internals ? internals.activeQuirks().includes("NeedsStyleSheetInjectedQuirk") : "no internals"),
            "color: " + getComputedStyle(document.getElementById("target")).color,
            "font face count: " + injectedSheetCopyCount
        ].join("\n");
    }
}
