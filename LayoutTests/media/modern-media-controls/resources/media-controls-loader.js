
(function() {
    const layoutTestsPath = window.location.href.substr(0, window.location.href.indexOf("/LayoutTests/"));
    const modulePath = layoutTestsPath ? layoutTestsPath + "/Source/WebCore/Modules/modern-media-controls" : "/modern-media-controls";

    ["media-controls", "background-tint", "volume-slider", "slider", "button", "start-button", "icon-button", "airplay-button", "time-label", "status-label", "controls-bar", "macos-media-controls", "macos-inline-media-controls", "macos-compact-inline-media-controls", "macos-fullscreen-media-controls", "ios-inline-media-controls", "buttons-container", "placard", "tracks-panel"].forEach(cssFile => {
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
