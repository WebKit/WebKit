"use strict";

// All currently known WebGL draft extensions should be added here.

let currentDraftExtensions = {
    "webgl": [
        "EXT_blend_func_extended",
        "EXT_clip_control",
        "EXT_depth_clamp",
        "EXT_polygon_offset_clamp",
        "EXT_texture_mirror_clamp_to_edge",
        "WEBGL_polygon_mode",
    ],
    "webgl2" : [
        "EXT_blend_func_extended",
        "EXT_clip_control",
        "EXT_conservative_depth",
        "EXT_depth_clamp",
        "EXT_polygon_offset_clamp",
        "EXT_render_snorm",
        "EXT_texture_mirror_clamp_to_edge",
        "NV_shader_noperspective_interpolation",
        "OES_sample_variables",
        "OES_shader_multisample_interpolation",
        "WEBGL_draw_instanced_base_vertex_base_instance",
        "WEBGL_multi_draw_instanced_base_vertex_base_instance",
        "WEBGL_polygon_mode",
        // Not checked here because availability would be
        // different for Intel and Apple silicon Macs
        // "WEBGL_render_shared_exponent",
        "WEBGL_stencil_texturing",
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