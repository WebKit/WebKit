#!/usr/bin/env python3
"""Generates the .html files of the WebGL benchmark suites in PerformanceTests/WebGL.

    python3 PerformanceTests/WebGL/resources/generate-tests.py

Each suite is a directory of small .html files that each call one function of a script in
resources/ with one permutation, for example a format and whether it is multisampled. This script
writes every .html file of every suite, deletes the .html files of the suite directories that it no
longer generates, and prints the number of files of each suite. It also writes the run-*-sweep.sh
scripts, which run sweeps of the benchmarks, see QUICK_SWEEP. Running it again on an unchanged
tree changes nothing.

The properties of the formats are in one place, the WebGLFormats table in webgl-formats.js, which
the benchmarks look formats up in by name. This script reads the same table, and uses it only to
decide which permutations exist. The lists of permutations that are choices rather than format
properties come first in this file. Extensions are checked when a benchmark runs, not here.
"""

import json
import os
import re
import sys

sys.dont_write_bytecode = True

# ------------------------------------------------------------------------------------------------
# Tables
# ------------------------------------------------------------------------------------------------

# ContextCreate: every combination of these WebGL context attributes, which are false by default
# (WebGL 1.0 specification, section 5.2, WebGLContextAttributes). Each attribute is (name, name in
# file names). A file name lists the attributes that are true, in this order, or is "none".
CONTEXT_ATTRIBUTES = [
    ("alpha", "alpha"),
    ("antialias", "msaa"),
    ("stencil", "stencil"),
    ("depth", "depth"),
    ("preserveDrawingBuffer", "preserve"),
]

# RenderbufferClear and FramebufferDraw: the color-renderable sized formats of ES 3.0 table 3.13
# and EXT_color_buffer_float.
RENDERBUFFER_COLOR_FORMATS = [
    "R8", "RG8", "RGB8", "RGB565", "RGBA8", "SRGB8_ALPHA8", "RGBA4", "RGB5_A1", "RGB10_A2",
    "R16F", "RG16F", "RGBA16F", "R32F", "RG32F", "RGBA32F", "R11F_G11F_B10F",
    "R8I", "R8UI", "R16I", "R16UI", "R32I", "R32UI", "RG8I", "RG8UI", "RG16I", "RG16UI", "RG32I", "RG32UI",
    "RGBA8I", "RGBA8UI", "RGB10_A2UI", "RGBA16I", "RGBA16UI", "RGBA32I", "RGBA32UI",
]

# The depth-renderable and stencil-renderable formats of ES 3.0 table 3.13. Each is a benchmark by
# itself.
RENDERBUFFER_DEPTH_STENCIL_FORMATS = [
    "DEPTH_COMPONENT16", "DEPTH_COMPONENT24", "DEPTH_COMPONENT32F", "DEPTH24_STENCIL8", "DEPTH32F_STENCIL8", "STENCIL_INDEX8",
]

# The depth and stencil renderbuffers attached with each color format: none, depth, stencil, and
# both.
RENDERBUFFER_ATTACHED_DEPTH_STENCIL_FORMATS = [None, "DEPTH_COMPONENT24", "STENCIL_INDEX8", "DEPTH24_STENCIL8"]

# Each permutation is single-sampled and, except for integer color formats, which can't be
# multisampled (ES 3.0 section 4.4.2.1), multisampled with 4 samples.

# FramebufferDraw: what the framebuffer draws to, renderbuffers ("rb") or textures ("tex"), each
# with the same permutations as RenderbufferClear. A file name starts with the target. "tex" leaves
# out the formats that are not texture formats in WebGL 2: STENCIL_INDEX8.
FRAMEBUFFER_DRAW_TARGETS = ["rb", "tex"]

# HiddenSurfaceRemoval: the fragment shaders, each named for what it uses that may stop the GPU's
# hidden surface removal: textureLod() at level 0 ("lod"), texture() at the implicit level, fwidth(),
# a discard that is never taken, and no texture at all. See resources/hidden-surface-removal.js.
# Each draws to a single-sampled and to a 4x multisampled default framebuffer. A file name is the
# shader, then "-msaa" when multisampled.
HIDDEN_SURFACE_REMOVAL_SHADERS = ["lod", "texture", "fwidth", "discard", "notexture"]
HIDDEN_SURFACE_REMOVAL_MULTISAMPLE = [False, True]

# FramebufferInvalidate: a frame that draws to the canvas, without and with binding a framebuffer
# whose only attachment is an unwritten texture and invalidating it first, and with a depth prepass
# to that framebuffer, with color writes off, before the invalidation. See
# resources/framebuffer-invalidate.js.
FRAMEBUFFER_INVALIDATE_TESTS = {
    "canvas-draw": {"invalidateUnwrittenTexture": False},
    "canvas-draw-invalidate-unwritten-texture": {"invalidateUnwrittenTexture": True},
    "canvas-draw-depth-prepass-invalidate-unwritten-texture": {"invalidateUnwrittenTexture": True, "drawDepthFirst": True},
}

# TextureInit: how the level of an RGBA8 texture becomes uninitialized, what the framebuffer is
# attached to, and the operation that then needs the level initialized, each a permutation. See
# resources/texture-init.js. A file name is the storage, the target and the operation.
TEXTURE_INIT_STORAGES = ["new", "invalidate"]
TEXTURE_INIT_TARGETS = ["2d", "2d-array"]
TEXTURE_INIT_OPERATIONS = ["draw", "sample", "sub-upload", "copy-sub", "blit-sub", "read"]

# TextureAtlasUpload: 64 small uploads into one 1024x1024 texture per run, for each format, tile
# size and state, see resources/texture-atlas-upload.js. A file name is the format and the tile size,
# then the state, except for "idle".
TEXTURE_ATLAS_UPLOAD_FORMATS = ["r8", "rgba8"]
TEXTURE_ATLAS_UPLOAD_TILE_SIZES = [16, 64]
TEXTURE_ATLAS_UPLOAD_STATES = ["idle", "used", "busy"]

# TextureClear: each sized texture format that is renderable, by itself or with an extension.
# TextureUpload and TextureSubUpload: each combination that texImage2D() accepts data for, and each
# compressed format. TextureStorageUpload: the same, but only sized formats, which texStorage2D()
# requires. TextureSubUpload and TextureStorageUpload leave out the compressed formats that
# compressedTexSubImage2D() can't replace the level of.

# TextureUpload, TextureSubUpload and TextureStorageUpload: the width and height of the textures, in
# ascending area, each a permutation. Some are not square, wide or tall, and most are not powers of
# two. All are multiples of 4, the block size of most compressed formats. PVRTC requires square
# textures with sides that are powers of two, so PVRTC formats have only those sizes. A file name
# has WIDTHxHEIGHT after the format.
TEXTURE_UPLOAD_SIZES = [(64, 64), (96, 160), (256, 200), (300, 500), (720, 480), (1280, 720), (1920, 1080), (2048, 2048)]

# The states of the GPU work around each upload, each a permutation, see texture-upload.js. A file
# name ends with the state, except for "idle". TextureStorageUpload creates a new texture in each
# run, which no draw can have used, so it has only the states that don't use the texture before the
# upload.
TEXTURE_UPLOAD_STATES = ["idle", "pass", "used", "used-flushed"]
TEXTURE_STORAGE_UPLOAD_STATES = ["idle", "pass"]

# The sweeps, each a run-SWEEP-sweep.sh script in PerformanceTests/WebGL that runs some benchmarks of
# some suites with Tools/Scripts/run-webgl-benchmarks. Each of quick, medium and full runs the benchmarks of
# the one before it, and more:
#
#  - quick: a few benchmarks of each suite, the common cases, to check a change in minutes.
#  - medium: also every size and state of the upload suites for TEXTURE_UPLOAD_SWEEP_FORMATS.
#  - full: every benchmark.
#  - upload: every benchmark of the upload suites, UPLOAD_SUITES, and none of the others: every
#    format at every size and in every state, for choosing how an implementation uploads textures.
#  - upload-quick: the same question in about 10 minutes for two builds: the formats, sizes and
#    states of TEXTURE_UPLOAD_QUICK_SWEEP.
#
# QUICK_SWEEP lists the benchmarks of quick by suite.
QUICK_SWEEP = {
    "ContextCreate": ["none", "alpha-msaa-stencil-depth-preserve"],
    "RenderbufferClear": ["rgba8", "rgba8-depth24_stencil8-msaa", "depth24_stencil8"],
    "FramebufferDraw": ["rb-rgba8", "tex-rgba8", "rb-rgba8-depth24_stencil8-msaa"],
    "HiddenSurfaceRemoval": ["lod", "texture", "texture-msaa"],
    "FramebufferInvalidate": list(FRAMEBUFFER_INVALIDATE_TESTS),
    "TextureClear": ["rgba8", "depth24_stencil8"],
    "TextureAtlasUpload": ["rgba8-16x16", "rgba8-16x16-used"],
    "TextureInit": ["new-2d-draw", "new-2d-sample", "invalidate-2d-draw", "invalidate-2d-array-draw"],
    "TextureUpload": ["rgba8-unsigned_byte-64x64", "rgba8-unsigned_byte-2048x2048", "rgba8-unsigned_byte-64x64-used"],
    "TextureSubUpload": ["rgba8-unsigned_byte-64x64", "rgba8-unsigned_byte-2048x2048", "rgba8-unsigned_byte-64x64-used"],
    "TextureStorageUpload": ["rgba8-unsigned_byte-64x64", "rgba8-unsigned_byte-2048x2048", "rgba8-unsigned_byte-64x64-pass"],
}

# The formats that medium runs at every size and state of the upload suites, as (internalFormat,
# type): the common case, and the format whose small uploads slowed down the most when ANGLE's Metal
# backend uploaded through staging buffers.
TEXTURE_UPLOAD_SWEEP_FORMATS = [("RGBA8", "UNSIGNED_BYTE"), ("RG32UI", "UNSIGNED_INT")]
UPLOAD_SUITES = ["TextureUpload", "TextureSubUpload", "TextureStorageUpload"]

# upload-quick: each format at each size and in each state that the suite has. The formats are
# file name prefixes: one of each size of texel, 1, 2, 3, 4, 8 and 16 bytes, an integer format, a
# depth-stencil format, which uploads through a staging buffer either way, and a compressed format.
# The sizes span the sizes where the two ways of uploading of ANGLE's Metal backend cross over. In
# the measurements, "pass" cost the same as "idle", and "used-flushed" the same as "used", so the
# sweep has only "idle" and "used". A benchmark takes about 1.7 s, so with two builds the sweep takes
# about 10 minutes.
TEXTURE_UPLOAD_QUICK_SWEEP = {
    "formats": [
        "r8-unsigned_byte", "rg8-unsigned_byte", "rgb8-unsigned_byte", "rgba8-unsigned_byte", "rgba16f-half_float",
        "rg32ui-unsigned_int", "rgba32f-float", "depth24_stencil8-unsigned_int_24_8", "compressed_rgba_astc_4x4_khr",
    ],
    "sizes": [(64, 64), (256, 200), (720, 480), (2048, 2048)],
    "states": ["idle", "used"],
}

# ------------------------------------------------------------------------------------------------
# Generation
# ------------------------------------------------------------------------------------------------

WEBGL_DIRECTORY = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

FORMATS_PATH = os.path.join(WEBGL_DIRECTORY, "resources", "webgl-formats.js")

TEMPLATE = """<!DOCTYPE html>
<html>
<body>
{scripts}
<script>
{call}
</script>
</body>
</html>
"""


def load_formats():
    """Reads the WebGLFormats table of webgl-formats.js, which is strict JSON after the assignment."""
    with open(FORMATS_PATH) as file:
        text = file.read()
    match = re.search(r"^const WebGLFormats = ", text, re.M)
    body = text[match.end():].rstrip()
    if not body.endswith(";"):
        sys.exit(f"{FORMATS_PATH}: the WebGLFormats table must end with a semicolon")
    return json.loads(body[:-1])


FORMATS = load_formats()


def rows(columns, groups):
    """The rows of a table of WebGLFormats as dictionaries, with "group" set to their group."""
    if isinstance(groups, list):
        groups = {None: groups}
    return [dict(zip(columns, values), group=group) for group, group_rows in groups.items() for values in group_rows]


TEXTURE_FORMATS = rows(FORMATS["textureFormatColumns"], FORMATS["textureFormats"])
COMPRESSED_TEXTURE_FORMATS = rows(FORMATS["compressedTextureFormatColumns"], FORMATS["compressedTextureFormats"])
RENDERBUFFER_ONLY_FORMATS = rows(FORMATS["renderbufferOnlyFormatColumns"], FORMATS["renderbufferOnlyFormats"])


def js(value):
    """Formats a Python value as a JavaScript literal."""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, str)):
        return json.dumps(value)
    if isinstance(value, (list, tuple)):
        return "[" + ", ".join(js(item) for item in value) + "]"
    if isinstance(value, dict):
        return "{ " + ", ".join(f"{key}: {js(item)}" for key, item in value.items()) + " }"
    raise TypeError(f"Can't format {value!r}")


def call(function, options):
    return f"{function}({js(options)});"


def channels_of(internal_format):
    return next(row for row in TEXTURE_FORMATS + RENDERBUFFER_ONLY_FORMATS if row["internalFormat"] == internal_format)["channels"]


def is_integer(internal_format):
    return channels_of(internal_format)[0].startswith(("uint", "int"))


def is_sized(row):
    return row["internalFormat"] != row["format"]


def is_texture_format(internal_format):
    return any(row["internalFormat"] == internal_format for row in TEXTURE_FORMATS)


def context_create_tests():
    tests = {}
    count = len(CONTEXT_ATTRIBUTES)
    for bits in range(2 ** count):
        values = [bool(bits & (1 << (count - 1 - index))) for index in range(count)]
        name = "-".join(short for (attribute, short), value in zip(CONTEXT_ATTRIBUTES, values) if value) or "none"
        options = {attribute: value for (attribute, short), value in zip(CONTEXT_ATTRIBUTES, values)}
        tests[name] = call("runContextCreate", options)
    return tests


def renderbuffer_tests(function, target=None):
    tests = {}

    def add(color_format, depth_stencil, multisample):
        if target == "tex" and not all(is_texture_format(format) for format in (color_format, depth_stencil) if format):
            return
        options = {"target": target} if target else {}
        if color_format:
            options["colorFormat"] = color_format
        if depth_stencil:
            options["depthStencilFormat"] = depth_stencil
        options["multisample"] = multisample
        name = ([target] if target else []) + [format for format in (color_format, depth_stencil) if format] + (["msaa"] if multisample else [])
        tests["-".join(name).lower()] = call(function, options)

    for color_format in RENDERBUFFER_COLOR_FORMATS:
        for depth_stencil in RENDERBUFFER_ATTACHED_DEPTH_STENCIL_FORMATS:
            for multisample in (False, True):
                if not (multisample and is_integer(color_format)):
                    add(color_format, depth_stencil, multisample)
    for depth_stencil in RENDERBUFFER_DEPTH_STENCIL_FORMATS:
        for multisample in (False, True):
            add(None, depth_stencil, multisample)
    return tests


def texture_clear_tests():
    tests = {}
    for row in TEXTURE_FORMATS:
        if is_sized(row) and row["renderable"]:
            tests[row["internalFormat"].lower()] = call("runTextureClear", {"format": row["internalFormat"]})
    return tests


def framebuffer_draw_tests():
    tests = {}
    for target in FRAMEBUFFER_DRAW_TARGETS:
        tests.update(renderbuffer_tests("runFramebufferDraw", target))
    return tests


def hidden_surface_removal_tests():
    tests = {}
    for shader in HIDDEN_SURFACE_REMOVAL_SHADERS:
        for multisample in HIDDEN_SURFACE_REMOVAL_MULTISAMPLE:
            name = shader + ("-msaa" if multisample else "")
            tests[name] = call("runHiddenSurfaceRemoval", {"shader": shader, "multisample": multisample})
    return tests


def framebuffer_invalidate_tests():
    return {name: call("runFramebufferInvalidate", options) for name, options in FRAMEBUFFER_INVALIDATE_TESTS.items()}


def texture_atlas_upload_tests():
    tests = {}
    for format_name in TEXTURE_ATLAS_UPLOAD_FORMATS:
        for tile_size in TEXTURE_ATLAS_UPLOAD_TILE_SIZES:
            for state in TEXTURE_ATLAS_UPLOAD_STATES:
                name = f"{format_name}-{tile_size}x{tile_size}" + ("" if state == "idle" else f"-{state}")
                tests[name] = call("runTextureAtlasUpload", {"format": format_name, "tileSize": tile_size, "state": state})
    return tests


def texture_init_tests():
    tests = {}
    for storage in TEXTURE_INIT_STORAGES:
        for target in TEXTURE_INIT_TARGETS:
            for operation in TEXTURE_INIT_OPERATIONS:
                tests[f"{storage}-{target}-{operation}"] = call("runTextureInit", {"storage": storage, "target": target, "operation": operation})
    return tests


def texture_upload_name(format_name, width, height, state):
    name = f"{format_name}-{width}x{height}"
    if state != "idle":
        name += f"-{state}"
    return name.lower()


def texture_upload_options(options, width, height, state):
    options = dict(options, width=width, height=height)
    if state != "idle":
        options["state"] = state
    return options


def texture_upload_tests(function, subimage, immutable):
    tests = {}
    states = TEXTURE_STORAGE_UPLOAD_STATES if immutable else TEXTURE_UPLOAD_STATES
    for width, height in TEXTURE_UPLOAD_SIZES:
        is_power_of_two_square = width == height and width & (width - 1) == 0
        for state in states:
            for row in TEXTURE_FORMATS:
                if row["uploadable"] and (is_sized(row) or not immutable):
                    name = texture_upload_name(f"{row['internalFormat']}-{row['type']}", width, height, state)
                    tests[name] = call(function, texture_upload_options({"internalFormat": row["internalFormat"], "type": row["type"]}, width, height, state))
            for row in COMPRESSED_TEXTURE_FORMATS:
                if (row["subimage"] or not subimage) and (row["compression"] != "pvrtc" or is_power_of_two_square):
                    name = texture_upload_name(row["internalFormat"], width, height, state)
                    tests[name] = call(function, texture_upload_options({"internalFormat": row["internalFormat"]}, width, height, state))
    return tests


# Each suite: its directory, the scripts that its .html files load after runner.js and
# webgl-bench.js, and the function that returns the call of each of its benchmarks by file name.
SUITES = [
    ("ContextCreate", ["context-create.js"], context_create_tests),
    ("RenderbufferClear", ["webgl-formats.js", "renderbuffer-clear.js"], lambda: renderbuffer_tests("runRenderbufferClear")),
    ("FramebufferDraw", ["webgl-formats.js", "framebuffer-draw.js"], framebuffer_draw_tests),
    ("HiddenSurfaceRemoval", ["hidden-surface-removal.js"], hidden_surface_removal_tests),
    ("FramebufferInvalidate", ["framebuffer-invalidate.js"], framebuffer_invalidate_tests),
    ("TextureClear", ["webgl-formats.js", "texture-clear.js"], texture_clear_tests),
    ("TextureInit", ["texture-init.js"], texture_init_tests),
    ("TextureAtlasUpload", ["texture-atlas-upload.js"], texture_atlas_upload_tests),
    ("TextureUpload", ["webgl-formats.js", "texture-upload.js"], lambda: texture_upload_tests("runTextureUpload", subimage=False, immutable=False)),
    ("TextureSubUpload", ["webgl-formats.js", "texture-upload.js"], lambda: texture_upload_tests("runTextureSubUpload", subimage=True, immutable=False)),
    ("TextureStorageUpload", ["webgl-formats.js", "texture-upload.js"], lambda: texture_upload_tests("runTextureStorageUpload", subimage=True, immutable=True)),
]


def sweep_tests(sweep, suite, tests):
    """The names of the benchmarks of suite that sweep runs, or None for all of them."""
    if sweep == "full":
        return None
    if sweep == "upload":
        return None if suite in UPLOAD_SUITES else []
    if sweep == "upload-quick":
        if suite not in UPLOAD_SUITES:
            return []
        states = TEXTURE_STORAGE_UPLOAD_STATES if suite == "TextureStorageUpload" else TEXTURE_UPLOAD_STATES
        names = [
            texture_upload_name(format_name, width, height, state)
            for format_name in TEXTURE_UPLOAD_QUICK_SWEEP["formats"]
            for width, height in TEXTURE_UPLOAD_QUICK_SWEEP["sizes"]
            for state in TEXTURE_UPLOAD_QUICK_SWEEP["states"] if state in states
        ]
        missing = [name for name in names if name not in tests]
        if missing:
            sys.exit(f"TEXTURE_UPLOAD_QUICK_SWEEP names benchmarks that {suite} doesn't have: {', '.join(missing)}")
        return names
    names = list(QUICK_SWEEP[suite])
    missing = [name for name in names if name not in tests]
    if missing:
        sys.exit(f"QUICK_SWEEP lists benchmarks that {suite} doesn't have: {', '.join(missing)}")
    if sweep == "medium" and suite in UPLOAD_SUITES:
        prefixes = tuple(f"{internal_format}-{type}-".lower() for internal_format, type in TEXTURE_UPLOAD_SWEEP_FORMATS)
        names += [name for name in tests if name.startswith(prefixes) and name not in names]
    return names


SWEEP_DESCRIPTIONS = {
    "quick": "a few benchmarks of each suite, the common cases",
    "medium": "the quick sweep, and every size and state of the texture upload suites for " + " and ".join(f"{internal_format}/{type}" for internal_format, type in TEXTURE_UPLOAD_SWEEP_FORMATS),
    "full": "every benchmark of every suite",
    "upload": "every benchmark of the texture upload suites, " + ", ".join(UPLOAD_SUITES) + ", for choosing how an implementation uploads textures",
    "upload-quick": "a few formats, sizes and states of the texture upload suites, for choosing how an implementation uploads textures in about 10 minutes",
}

SWEEP_TEMPLATE = """#!/bin/sh
# Generated by resources/generate-tests.py. Do not edit.
#
# Runs the {sweep} sweep of the WebGL benchmarks: {description}.
# {count} benchmarks.
#
#     run-{sweep}-sweep.sh BUILD_A [BUILD_B]
#
# BUILD_A and BUILD_B are build directories that contain WebKitTestRunner, for example
# WebKitBuild/cmake-mac/Release. With BUILD_B, every benchmark runs with BUILD_A first, then with
# BUILD_B, and the tables compare them. Each suite runs with Tools/Scripts/run-webgl-benchmarks,
# which writes SUITE.csv and the WebKitTestRunner output of each benchmark to the results directory,
# $WEBGL_BENCH_RESULTS, by default /tmp/webgl-benchmarks/{sweep}.

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "Usage: $0 BUILD_A [BUILD_B]" >&2
    exit 2
fi
# A sleep of the machine during a benchmark loses the WebGL context, so keep it awake.
if [ -z "${{WEBGL_BENCH_AWAKE:-}}" ] && command -v caffeinate > /dev/null; then
    WEBGL_BENCH_AWAKE=1 exec caffeinate -dims "$0" "$@"
fi
build_a=$1
build_b=${{2:-}}
directory=$(cd "$(dirname "$0")" && pwd)
runner=$directory/../../Tools/Scripts/run-webgl-benchmarks
results=${{WEBGL_BENCH_RESULTS:-/tmp/webgl-benchmarks/{sweep}}}
mkdir -p "$results"
status=0

# run_suite SUITE [BENCHMARK...]: runs the benchmarks of SUITE, or all of them if none are given.
run_suite() {{
    suite=$1
    shift
    if [ $# -gt 0 ]; then
        set -- --only "$@"
    fi
    echo "== $suite" >&2
    if ! "$runner" "$build_a" ${{build_b:+"$build_b"}} --tests-directory "$directory/$suite" --results-directory "$results/$suite" "$@" > "$results/$suite.csv"; then
        echo "run-webgl-benchmarks failed for $suite" >&2
        status=1
    fi
}}

{runs}
exit $status
"""


def write_if_changed(path, content, executable=False):
    if os.path.exists(path):
        with open(path) as file:
            if file.read() == content:
                return False
    with open(path, "w") as file:
        file.write(content)
    if executable:
        os.chmod(path, 0o755)
    return True


def write_sweeps(suite_tests):
    for sweep in SWEEP_DESCRIPTIONS:
        runs = []
        count = 0
        for directory, tests in suite_tests.items():
            names = sweep_tests(sweep, directory, tests)
            count += len(tests) if names is None else len(names)
            if names == []:
                continue
            if names is None:
                runs.append(f"run_suite {directory}")
            else:
                runs.append(f"run_suite {directory} \\\n" + " \\\n".join(f"    {name}" for name in names))
        content = SWEEP_TEMPLATE.format(sweep=sweep, description=SWEEP_DESCRIPTIONS[sweep], count=count, runs="\n".join(runs))
        written = write_if_changed(os.path.join(WEBGL_DIRECTORY, f"run-{sweep}-sweep.sh"), content, executable=True)
        print(f"run-{sweep}-sweep.sh: {count} benchmarks{' (written)' if written else ''}")


def main():
    total = 0
    suite_tests = {}
    for directory, scripts, generate in SUITES:
        path = os.path.join(WEBGL_DIRECTORY, directory)
        os.makedirs(path, exist_ok=True)
        tests = generate()
        suite_tests[directory] = tests
        written = 0
        for name, test_call in tests.items():
            script_tags = ["../../resources/runner.js", "../resources/webgl-bench.js"] + ["../resources/" + script for script in scripts]
            content = TEMPLATE.format(scripts="\n".join(f'<script src="{src}"></script>' for src in script_tags), call=test_call)
            if write_if_changed(os.path.join(path, name + ".html"), content):
                written += 1
        stale = sorted(name for name in os.listdir(path) if name.endswith(".html") and name[:-len(".html")] not in tests)
        for name in stale:
            os.remove(os.path.join(path, name))
        total += len(tests)
        print(f"{directory}: {len(tests)} files ({written} written, {len(stale)} deleted)")
    print(f"Total: {total} files")
    write_sweeps(suite_tests)


if __name__ == "__main__":
    sys.exit(main())
