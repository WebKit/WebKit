"use strict";

// All currently known WebGL draft extensions should be added here.

let currentDraftExtensions = {
    "webgl": [
    ],
    "webgl2" : [
        "EXT_provoking_vertex",
        "WEBGL_draw_instanced_base_vertex_base_instance",
        "WEBGL_multi_draw_instanced_base_vertex_base_instance",
    ]
};

function runTest() {
    for (const [contextType, draftExtensions] of Object.entries(currentDraftExtensions)) {
        if (!draftExtensions) {
            testPassed(`${contextType}: No current draft extensions.`);
            continue;
        }
        const canvas = document.createElement("canvas");
        const gl = canvas.getContext(contextType);
        if (!gl) {
            testPassed(`${contextType}: Not supported.`);
            continue;
        }
        const supportedExtensions = gl.getSupportedExtensions();
        for (const draftExtension of draftExtensions) {
            const s = supportedExtensions.includes(draftExtension);
            testPassed(`${contextType}:${draftExtension}: ${s ? "Supported" : "Not supported"}`);
            if (!s)
                continue;
            const ext = gl.getExtension(draftExtension);
            if (typeof ext !== "undefined")
                testPassed(`${contextType}:${draftExtension}: Has object.`);
            else
                testFailed(`${contextType}:${draftExtension}: No object.`);
        }
    }
}