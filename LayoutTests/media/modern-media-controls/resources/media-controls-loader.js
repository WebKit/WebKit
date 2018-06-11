
(function() {
    if (window.internals)
        internals.settings.setUseAnonymousModeWhenFetchingMaskImages(false);

    const layoutTestsPath = window.location.href.substr(0, window.location.href.indexOf("/LayoutTests/"));
    const modulePath = layoutTestsPath ? layoutTestsPath + "/Source/WebCore/Modules/modern-media-controls" : "/modern-media-controls";

    ["activity-indicator", "airplay-button", "background-tint", "button", "buttons-container", "controls-bar", "inline-media-controls", "macos-fullscreen-media-controls", "macos-inline-media-controls", "media-controls", "media-document", "placard", "slider", "status-label", "text-tracks", "time-label", "tracks-panel"].forEach(cssFile => {
        document.write(`<link rel="stylesheet" type="text/css" href="${modulePath}/controls/${cssFile}.css">`);
    });

    const request = new XMLHttpRequest;
    request.open("GET", `${modulePath}/js-files`, false);
    request.send();

    request.responseText.trim().split("\n").forEach(jsFile => {
        document.write(`<script type="text/javascript" src="${modulePath}/${jsFile}"></script>`);
    });

    document.write(`<script type="text/javascript">iconService.directoryPath = "${modulePath}/images";</script>`);
})();
