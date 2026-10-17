#!/usr/bin/env python3
"""Generate WebCore's derived sources for the standalone WKC build.

The normal WebKit CMake build owns this graph through WebCoreMacros.cmake.
WKC has a separate cross-build, so reproduce the required generator invocations
here without changing upstream WebCore/JSC files or invoking the host WebKit
build system.
"""
from __future__ import annotations

import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
WEBCORE = ROOT / "Source" / "WebCore"
JSC = ROOT / "Source" / "JavaScriptCore"
WTF = ROOT / "Source" / "WTF"
GEN = ROOT / ".generated-cache"
WKC_CPP_DIR = WEBCORE / "WebCore"
WEBCORE_CMAKE = WEBCORE / "CMakeLists.txt"
WEBCORE_MACROS = WEBCORE / "WebCoreMacros.cmake"

# These switches control generated DOM/JSC bindings, so they must match the
# *full-engine* CMake profile in .github/workflows/build.yml. The bootstrap
# archive is built separately and must not dictate the generated interface set.
# Keep notifications and MediaStream bindings because the full profile enables
# their WebCore APIs; Gamepad, WebGL, Web Audio, WebRTC and WebAssembly are
# deliberately disabled until their Wii U backends are complete.
FEATURE_DEFINES = (
    "LANGUAGE_JAVASCRIPT ENABLE_WKC ENABLE_CSS_ANIMATIONS "
    "ENABLE_INDEXED_DATABASE ENABLE_MEDIA_STREAM ENABLE_NOTIFICATIONS ENABLE_SVG"
)


def run(args: list[str], *, cwd: Path | None = None) -> None:
    printable = shlex.join([str(arg) for arg in args])
    print(f"+ {printable}", flush=True)
    subprocess.run([str(arg) for arg in args], cwd=cwd, check=True)


def parse_cmake_list(source: str, variable: str) -> list[str]:
    """Read literal IDL paths from the variable's set() and list(APPEND) blocks.

    WebCore's CMake file builds these lists in several stages. Reading only the
    initial set() silently drops later platform-independent interface entries;
    that can leave supplemental IDLs in the list whose base interface was
    declared by one of those appends (for example, GPUDevice.idl).
    """
    block_pattern = re.compile(
        rf"(?ms)^(?:set\(\s*{re.escape(variable)}\s+|"
        rf"list\(\s*APPEND\s+{re.escape(variable)}\s+)(.*?)^\)"
    )
    matches = list(block_pattern.finditer(source))
    if not matches:
        raise RuntimeError(f"Could not find upstream CMake list {variable}")

    values: list[str] = []
    for match in matches:
        for line in match.group(1).splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            # Accept literal relative IDL paths. Platform-injected paths are
            # supplied by the native CMake build and are not part of this port.
            if re.fullmatch(r"[A-Za-z0-9_+./-]+\.idl", line):
                values.append(line)
    return list(dict.fromkeys(values))


def parse_literal_paths(source: str, variable: str, suffix: str) -> list[Path]:
    match = re.search(
        rf"(?ms)^set\(\s*{re.escape(variable)}\s+(.*?)^\)",
        source,
    )
    if not match:
        raise RuntimeError(f"Could not find upstream CMake list {variable}")
    found: list[Path] = []
    for rel in re.findall(rf"\$\{{WEBCORE_DIR\}}/([^\s\")]+{re.escape(suffix)})", match.group(1)):
        path = WEBCORE / rel
        if path.is_file() and path not in found:
            found.append(path)
    return found


def write_list(path: Path, values: list[Path]) -> None:
    path.write_text("".join(f"{value.resolve()}\n" for value in values), encoding="utf-8")


def copy_generated_headers_and_sources() -> int:
    """Copy only generator outputs; never follow a forwarding-header symlink."""
    WKC_CPP_DIR.mkdir(parents=True, exist_ok=True)
    copied = 0
    for source in sorted((*GEN.glob("*.h"), *GEN.glob("*.cpp"))):
        destination = WKC_CPP_DIR / source.name
        if destination.is_symlink():
            destination.unlink()
        shutil.copy2(source, destination)
        copied += 1
    return copied


def main() -> int:
    cmake_source = WEBCORE_CMAKE.read_text(encoding="utf-8")
    if not WEBCORE_MACROS.is_file():
        raise RuntimeError(f"Missing {WEBCORE_MACROS}")
    GEN.mkdir(parents=True, exist_ok=True)

    # CSS property/value/selector tables are generated in the derived-sources
    # directory by the regular WebCore CMake graph. WKC doesn't use that graph.
    shutil.copyfile(WEBCORE / "css" / "CSSProperties.json", GEN / "CSSProperties.json")
    run([
        sys.executable,
        WEBCORE / "css" / "scripts" / "process-css-properties.py",
        "--defines", "",
        "--gperf-executable", "gperf",
    ], cwd=GEN)

    with (GEN / "CSSValueKeywords.in").open("wb") as out:
        for source in (
            WEBCORE / "css" / "CSSValueKeywords.in",
            WEBCORE / "css" / "SVGCSSValueKeywords.in",
        ):
            out.write(source.read_bytes())
    run([
        sys.executable,
        WEBCORE / "css" / "scripts" / "process-css-values.py",
        "--defines", FEATURE_DEFINES,
        "--gperf-executable", "gperf",
    ], cwd=GEN)

    shutil.copyfile(
        WEBCORE / "css" / "CSSPseudoSelectors.json",
        GEN / "CSSPseudoSelectors.json",
    )
    run([
        sys.executable,
        WEBCORE / "css" / "scripts" / "process-css-pseudo-selectors.py",
        "--defines", FEATURE_DEFINES,
        "--gperf-executable", "gperf",
    ], cwd=GEN)

    # Process-sync data is generated C++/headers, not checked-in WebCore files.
    for name in ("Document", "FrameTree"):
        run([
            sys.executable,
            WEBCORE / "Scripts" / "generate-process-sync-data.py",
            name,
            WEBCORE / "page" / f"{name}SyncData.in",
            GEN,
        ])

    # HTML/SVG/MathML/XML names, factories, and enums use upstream's own tool.
    make_names = WEBCORE / "dom" / "make_names.pl"
    run([
        "perl", make_names, "--outputDir", GEN,
        "--attrs", WEBCORE / "html" / "HTMLAttributeNames.in",
        "--elements", WEBCORE / "html" / "HTMLTagNames.in",
        "--factory", "--wrapperFactory",
    ])
    run([
        "perl", make_names, "--outputDir", GEN,
        "--attrs", WEBCORE / "svg" / "svgattrs.in",
        "--elements", WEBCORE / "svg" / "svgtags.in",
        "--factory", "--wrapperFactory",
    ])
    for namespace, attrs, elements in (
        ("MathML", "mathml/mathattrs.in", "mathml/mathtags.in"),
        ("XLink", "svg/xlinkattrs.in", None),
        ("XMLNS", "xml/xmlnsattrs.in", None),
        ("XML", "xml/xmlattrs.in", None),
    ):
        args = ["perl", make_names, "--outputDir", GEN, "--attrs", WEBCORE / attrs]
        if elements:
            args += ["--elements", WEBCORE / elements]
        if namespace == "MathML":
            args += ["--factory", "--wrapperFactory"]
        run(args)

    enum_inputs = [
        ("--elements", WEBCORE / "html" / "HTMLTagNames.in"),
        ("--elements", WEBCORE / "svg" / "svgtags.in"),
        ("--elements", WEBCORE / "mathml" / "mathtags.in"),
        ("--attrs", WEBCORE / "html" / "HTMLAttributeNames.in"),
        ("--attrs", WEBCORE / "mathml" / "mathattrs.in"),
        ("--attrs", WEBCORE / "svg" / "svgattrs.in"),
        ("--attrs", WEBCORE / "svg" / "xlinkattrs.in"),
        ("--attrs", WEBCORE / "xml" / "xmlattrs.in"),
        ("--attrs", WEBCORE / "xml" / "xmlnsattrs.in"),
    ]
    for enum_name in ("TagName", "NodeName", "Namespace"):
        args = ["perl", make_names, "--outputDir", GEN, "--enum", enum_name]
        for option, path in enum_inputs:
            args += [option, path]
        run(args)

    run([
        "perl", WEBCORE / "dom" / "make_event_factory.pl",
        "--input", WEBCORE / "dom" / "EventInterfaces.in",
        "--outputDir", GEN,
    ])
    run([
        "perl", WEBCORE / "dom" / "make_event_factory.pl",
        "--input", WEBCORE / "dom" / "EventTargetFactory.in",
        "--outputDir", GEN,
    ])
    run([
        sys.executable,
        WEBCORE / "dom" / "make-event-names.py",
        "--event-names", WEBCORE / "dom" / "EventNames.json",
    ], cwd=GEN)
    run([
        sys.executable,
        WEBCORE / "html" / "parser" / "create-html-entity-table",
        "-o", GEN / "HTMLEntityTable.cpp",
        WEBCORE / "html" / "parser" / "HTMLEntityNames.in",
    ])
    run([
        "perl", WEBCORE / "dom" / "make_names.pl",
        "--outputDir", GEN,
        "--fonts", WEBCORE / "css" / "WebKitFontFamilyNames.in",
    ])

    # Generate all of the JS builtins listed by upstream's CMake graph. The
    # per-file pass must run before --wrappers-only, just as upstream does.
    builtin_match = re.search(
        r"(?ms)^set\(\s*WebCore_BUILTINS_SOURCES\s+(.*?)^\)",
        cmake_source,
    )
    if not builtin_match:
        raise RuntimeError("Could not find WebCore_BUILTINS_SOURCES in CMakeLists.txt")
    builtin_paths = []
    for rel in re.findall(r"\$\{WEBCORE_DIR\}/([^\s\")]+\.js)", builtin_match.group(1)):
        path = WEBCORE / rel
        if path.is_file() and path not in builtin_paths:
            builtin_paths.append(path)
    if not builtin_paths:
        raise RuntimeError("No WebCore JS builtin sources found in upstream CMakeLists.txt")
    builtin_generator = JSC / "Scripts" / "generate-js-builtins.py"
    for source in builtin_paths:
        run([
            sys.executable, builtin_generator,
            "--framework", "WebCore",
            "--output-directory", GEN,
            source,
        ])
    run([
        sys.executable, builtin_generator,
        "--wrappers-only", "--framework", "WebCore",
        "--output-directory", GEN,
        *builtin_paths,
    ])

    # Recreate WebCore's generated default user-agent stylesheet tables.
    stylesheet_paths = parse_literal_paths(
        cmake_source, "WebCore_USER_AGENT_STYLE_SHEETS", ".css"
    )
    if stylesheet_paths:
        run([
            "perl", WEBCORE / "css" / "make-css-file-arrays.pl",
            "--defines", FEATURE_DEFINES,
            GEN / "UserAgentStyleSheets.h",
            GEN / "UserAgentStyleSheetsData.cpp",
            *stylesheet_paths,
        ])

    # Generate settings, which WebCore normally produces from YAML + ERB.
    run([
        "ruby", WEBCORE / "Scripts" / "GenerateSettings.rb",
        "--outputDir", GEN,
        "--template", WEBCORE / "Scripts" / "SettingsTemplates" / "Settings.h.erb",
        "--template", WEBCORE / "Scripts" / "SettingsTemplates" / "Settings.cpp.erb",
        WTF / "Scripts" / "Preferences" / "UnifiedWebPreferences.yaml",
        WEBCORE / "page" / "Settings.yaml",
    ])

    # The large DOM/HTML/SVG API is described in WebCore's IDL lists. Use those
    # lists instead of blindly wrapping every .idl on disk (which would treat
    # partial/mixin definitions as standalone interfaces).
    main_idl_relatives = (
        parse_cmake_list(cmake_source, "WebCore_NON_SVG_IDL_FILES")
        + parse_cmake_list(cmake_source, "WebCore_SVG_IDL_FILES")
        + parse_cmake_list(cmake_source, "WebCore_IDL_FILES")
    )
    main_idls = [WEBCORE / item for item in dict.fromkeys(main_idl_relatives)]
    missing = [path for path in main_idls if not path.is_file()]
    if missing:
        raise RuntimeError(
            "Upstream WebCore IDL list contains missing input files: "
            + ", ".join(str(path.relative_to(ROOT)) for path in missing[:20])
        )

    supplemental_relatives = parse_cmake_list(
        cmake_source, "WebCore_SUPPLEMENTAL_IDL_FILES"
    )
    supplemental_idls = [WEBCORE / item for item in dict.fromkeys(supplemental_relatives)]
    supplemental_idls.append(GEN / "CSSStyleProperties+PropertyNames.idl")
    supplemental_idls = [path for path in supplemental_idls if path.is_file()]
    if not (GEN / "CSSStyleProperties+PropertyNames.idl").is_file():
        raise RuntimeError(
            "CSSStyleProperties+PropertyNames.idl was not generated by process-css-properties.py"
        )

    idl_list = GEN / "wkc-idl-files.txt"
    pp_idl_list = GEN / "wkc-pp-idl-files.txt"
    idl_names_list = GEN / "wkc-idl-file-names.txt"
    write_list(idl_list, main_idls)
    all_names = list(dict.fromkeys(main_idls + supplemental_idls))
    write_list(pp_idl_list, all_names)
    write_list(idl_names_list, all_names)

    bindings_script_dir = WEBCORE / "bindings" / "scripts"
    bindings_generator = bindings_script_dir / "generate-bindings-all.pl"
    idl_attributes = bindings_script_dir / "IDLAttributes.json"
    binding_dependencies = [
        bindings_script_dir / item for item in (
            "generate-bindings.pl", "CodeGenerator.pm", "CodeGeneratorJS.pm",
            "Hasher.pm", "IDLParser.pm", "InFilesParser.pm",
            "StaticString.pm", "preprocessor.pm",
        )
    ]
    extra_outputs = [
        GEN / "DOMIsoSubspaces.h",
        GEN / "DOMClientIsoSubspaces.h",
        GEN / "DOMConstructors.h",
        GEN / "DOMWindowConstructors.idl",
        GEN / "WorkerGlobalScopeConstructors.idl",
        GEN / "ShadowRealmGlobalScopeConstructors.idl",
        GEN / "DedicatedWorkerGlobalScopeConstructors.idl",
        GEN / "ServiceWorkerGlobalScopeConstructors.idl",
        GEN / "SharedWorkerGlobalScopeConstructors.idl",
        GEN / "WorkletGlobalScopeConstructors.idl",
        GEN / "PaintWorkletGlobalScopeConstructors.idl",
        GEN / "AudioWorkletGlobalScopeConstructors.idl",
    ]
    extra_args = [
        ("--isoSubspacesHeaderFile", extra_outputs[0]),
        ("--clientISOSubspacesHeaderFile", extra_outputs[1]),
        ("--constructorsHeaderFile", extra_outputs[2]),
        ("--windowConstructorsFile", extra_outputs[3]),
        ("--workerGlobalScopeConstructorsFile", extra_outputs[4]),
        ("--shadowRealmGlobalScopeConstructorsFile", extra_outputs[5]),
        ("--dedicatedWorkerGlobalScopeConstructorsFile", extra_outputs[6]),
        ("--serviceWorkerGlobalScopeConstructorsFile", extra_outputs[7]),
        ("--sharedWorkerGlobalScopeConstructorsFile", extra_outputs[8]),
        ("--workletGlobalScopeConstructorsFile", extra_outputs[9]),
        ("--paintWorkletGlobalScopeConstructorsFile", extra_outputs[10]),
        ("--audioWorkletGlobalScopeConstructorsFile", extra_outputs[11]),
    ]
    binding_command = [
        "perl", bindings_generator,
        "--defines", FEATURE_DEFINES,
        "--generator", "JS",
        "--outputDir", GEN,
        "--idlFilesList", idl_list,
        "--idlFileNamesList", idl_names_list,
        "--ppIDLFilesList", pp_idl_list,
        "--idlAttributesFile", idl_attributes,
        "--supplementalDependencyFile", GEN / "wkc-supplemental-dependencies.txt",
        "--ignoreStandaloneConstructorAttributes",
        "--numOfJobs", str(max(1, os.cpu_count() or 1)),
    ]
    for dependency in binding_dependencies:
        binding_command.append(f"--generatorDependency={dependency}")
    for output in extra_outputs:
        binding_command.append(f"--ppExtraOutput={output}")
    for option, value in extra_args:
        binding_command.append(f"--ppExtraArgs={option}")
        binding_command.append(f"--ppExtraArgs={value}")
    run(binding_command, cwd=WEBCORE)

    required = (
        "CSSSelectorEnums.h",
        "CSSSelectorInlines.h",
        "NodeName.h",
        "TagName.h",
        "Namespace.h",
        "DocumentSyncData.h",
        "FrameTreeSyncData.h",
        "DOMConstructors.h",
        "WebCoreJSBuiltins.h",
        "WebCoreJSBuiltinInternals.h",
        "JSNode.h",
        "JSEventTarget.h",
    )
    absent = [name for name in required if not (GEN / name).is_file()]
    if absent:
        raise RuntimeError(
            "WebCore generation did not produce required headers: " + ", ".join(absent)
        )

    copied = copy_generated_headers_and_sources()
    generated_cpp = len(list(GEN.glob("*.cpp")))
    generated_h = len(list(GEN.glob("*.h")))
    print(
        f"Generated WebCore for WKC: {len(main_idls)} main IDLs, "
        f"{len(supplemental_idls)} supplemental IDLs, "
        f"{generated_cpp} .cpp files, {generated_h} headers; "
        f"copied {copied} generated files into Source/WebCore/WebCore/",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"ERROR: WKC WebCore code generation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
