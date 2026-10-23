#!/usr/bin/env python3
"""Static correctness gates for the Wii U in-process WebKit loading path.

These checks complement (not replace) the cross-compile and runtime browser tests.
They deliberately fail on semantically dangerous regressions that can still compile.
"""
from pathlib import Path
import re
import subprocess
import sys

# Resolve the checkout root from the workflow working directory first, then the
# script location. GitHub Actions can invoke this file from a sparse checkout;
# using a fixed parent index is brittle if the script is symlinked or relocated.
def find_repository_root():
    import os

    # Prefer Git's own checkout root. In container jobs with sparse-checkout,
    # GITHUB_WORKSPACE and the script's resolved path can differ from the actual
    # checkout root (and a partially materialized .github directory can mislead
    # file-based discovery).
    for start in (Path.cwd(), Path(__file__).resolve().parent):
        try:
            result = subprocess.run(
                ["git", "-c", f"safe.directory={ROOT}", "rev-parse", "--show-toplevel"],
                cwd=start,
                check=True,
                capture_output=True,
                text=True,
                encoding="utf-8",
            )
            root = Path(result.stdout.strip()).resolve()
            if (root / "Source/WebCore").is_dir():
                return root
        except (OSError, subprocess.CalledProcessError):
            pass

    candidates = []
    workspace = os.environ.get("GITHUB_WORKSPACE")
    if workspace:
        candidates.append(Path(workspace).resolve())
    candidates.append(Path.cwd().resolve())
    candidates.extend(Path(__file__).resolve().parents)
    for candidate in candidates:
        if (candidate / "Source/WebCore").is_dir():
            return candidate
    return Path(__file__).resolve().parents[2]

ROOT = find_repository_root()
VIEW_CPP = ROOT / "Source/WebKit/WKC/webkit/WKCWebView.cpp"
VIEW_H = ROOT / "Source/WebKit/WKC/webkit/WKCWebView.h"
RESOURCE = ROOT / "Source/WebCore/platform/WKC/ResourceHandleWKC.cpp"
RESPONSE_CONVERSION = ROOT / "Source/WebCore/platform/network/curl/ResourceResponseCurl.cpp"
FRAME_LOADER = ROOT / "Source/WebKit/WKC/WebCoreSupport/FrameLoaderClientWKC.cpp"
EVENT_LOOP = ROOT / "Source/WebCore/platform/WKC/EventLoopWKC.cpp"
VIEW_HEADER = ROOT / "Source/WebKit/WKC/webkit/WKCWebView.h"
CMAKE = ROOT / "Source/WebKit/WKC/CMakeLists.txt"

errors = []

def require(path, label, needles):
    source = path.read_text(encoding="utf-8")
    for needle in needles:
        if needle not in source:
            errors.append(f"{label}: required contract missing: {needle}")
    return source

view = require(VIEW_CPP, "WKCWebView.cpp", [
    "bool WKCWebView::loadURLOrSearch(",
    'target = "https://www.google.com/search?q="',
    "WTF::URL parsedTarget",
    "parsedTarget.isValid()",
    "parsedTarget.protocolIsInHTTPFamily()",
    "loadURI(normalizedTarget.data(), referer)",
])
header = require(VIEW_H, "WKCWebView.h", [
    "bool loadURLOrSearch(const char* input, const char* referrer = 0);",
])

# The public URI entry point is used by both startup navigation and callers that
# bypass address-bar normalization. It must tolerate view/frame construction and
# teardown windows without dereferencing a missing main frame.
load_uri = re.search(r"void WKCWebView::loadURI\(const char\* uri, const char\* referer\)\s*\{([^}]*)\}", view)
if not load_uri or "!m_private->m_mainFrame" not in load_uri.group(1):
    errors.append("WKCWebView.cpp: loadURI must guard a missing main frame")
resource = require(RESOURCE, "ResourceHandleWKC.cpp", [
    '#include "WKCCACertBundle.h"',
    "#elif defined(WKC_REQUIRE_EMBEDDED_CA_BUNDLE)",
    'setCACertData(WTF::move(caBundle))',
    "bool ResourceHandle::start()",
    "void ResourceHandle::cancel()",
    "void ResourceHandle::platformLoadResourceSynchronously(",
    "curlDidReceiveResponse(",
    "curlDidReceiveData(",
    "curlDidComplete(",
    "curlDidFailWithError(",
    "maximumRedirects = 20",
    "CURLOPT_POSTFIELDSIZE_LARGE",
    "retaining normal TLS peer and hostname verification",
    "configureCertificateAuthorityBundle(",
    "WKCCACertificates::bundle",
    "sslHandle.setCACertData(WTF::move(caBundle))",
    "handle->didReceiveResponse(WTF::move(response)",
    "protectedRequest->completeDidReceiveResponse()",
    "client->didReceiveData(handle.get(), buffer.get(), static_cast<int>(length))",
    "handle->client()->didFinishLoading(handle.get(), metrics)",
    "handle->client()->didFail(handle.get(), error)",
    "applyCookiesToRequest(",
    "removeActiveTransfer(",
])

# A network response must be translated into normal WebCore metadata and the
# asynchronous response-completion handshake must release the paused cURL body.
response = require(RESPONSE_CONVERSION, "ResourceResponseCurl.cpp", [
    "ResourceResponse::ResourceResponse(CurlResponse& response)",
    "setURL(URL { response.url })",
    "setHTTPStatusCode(response.statusCode ? response.statusCode : response.httpConnectCode)",
    "setMimeType(extractMIMETypeFromMediaType(",
    "setTextEncodingName(extractCharsetFromMediaType(",
    "setSource(ResourceResponse::Source::Network)",
])
frame_loader = require(FRAME_LOADER, "FrameLoaderClientWKC.cpp", [
    "FrameLoaderClientWKC::committedLoad(",
    "writer.setEncoding(loader->response().textEncodingName()",
    "loader->commitData(data)",
    "FrameLoaderClientWKC::finishedLoading(",
    "m_appClient->finishedLoading(&ldr.wkc())",
])

# Worker-thread cURL completions are dispatched through WTF::RunLoop. The WKC
# embedder owns the frame loop, so the engine must expose an explicit pump API.
event_loop = require(EVENT_LOOP, "EventLoopWKC.cpp", [
    "void wkcRunLoopCycle()",
    "WTF::RunLoop::cycle()",
])
view_header = require(VIEW_HEADER, "WKCWebView.h", [
    "WKCWebKitProcessEvents()",
])
if "WKCWebKitProcessEvents() { WebCore::wkcRunLoopCycle(); }" not in view:
    errors.append("WKCWebView.cpp: public event pump does not drain WebCore's main RunLoop")

cmake = require(CMAKE, "CMakeLists.txt", [
    "WKC_ENABLE_FULL_WEBCORE",
    "WKC_REQUIRE_EMBEDDED_CA_BUNDLE=1",
    'target_include_directories(WebKitWKC PRIVATE',
    "/tmp/ssl-headers",
    "ResourceHandleWKC.cpp",
    "list(FIND WEBCORE_PLATFORM_SOURCES",
    "message(FATAL_ERROR",
])

# Catch accidental TLS bypasses even when all APIs still compile.
for pattern in (
    r"CURLOPT_SSL_VERIFYPEER\s*,\s*0\b",
    r"CURLOPT_SSL_VERIFYHOST\s*,\s*0\b",
):
    if re.search(pattern, resource):
        errors.append(f"ResourceHandleWKC.cpp: insecure TLS verification override matches {pattern}")

# The full engine must not silently route networking through the modern
# out-of-process default instead of the in-process Wii U backend.
if "set(_WKC_RESOURCE_HANDLE_SOURCE" not in cmake:
    errors.append("CMakeLists.txt: full-WebCore in-process network source guard missing")
if 'list(REMOVE_ITEM WEBCORE_CORE_SOURCES' not in cmake or 'platform/network/ResourceHandle.cpp' not in cmake:
    errors.append("CMakeLists.txt: upstream ResourceHandle.cpp is not explicitly excluded from the full-engine source list")
if "if (WKC_ENABLE_FULL_WEBCORE)" not in cmake:
    errors.append("CMakeLists.txt: full-WebCore guard is not conditional on the full engine")

# Compilation alone is insufficient: reject known compatibility shims that make
# WebCore appear initialized while silently disabling its live network/cookie/TLS
# management path. These are semantic failures even if every translation unit builds.
view_without_comments = re.sub(r'/\*.*?\*/|//[^\n]*', '', view, flags=re.S)
for pattern, description in (
    (r'static\s+ResourceHandleManager\*\s+sharedInstance\s*\(\s*\)\s*\{\s*return\s+nullptr\s*;',
     'ResourceHandleManager::sharedInstance always returns null'),
    (r'static\s+bool\s+createSharedInstance\s*\(\s*\)\s*\{\s*return\s+true\s*;',
     'ResourceHandleManager::createSharedInstance reports success without creating a manager'),
    (r'void\s+clearCookies\s*\(\s*\)\s*\{\s*\}',
     'cookie clearing is implemented as a no-op'),
    (r'void\s+CookieDeserialize\s*\([^)]*\)\s*\{\s*\}',
     'cookie restoration is implemented as a no-op'),
):
    if re.search(pattern, view_without_comments):
        errors.append(f'WKCWebView.cpp: semantic stub detected: {description}')

# Verify the CI path actually configures and builds the full engine, not only the
# bootstrap target. A successful bootstrap archive is not evidence of page loading.
workflow_path = ROOT / '.github/workflows/build.yml'
try:
    # Validate the committed workflow, not a possibly partial working-tree copy
    # produced by sparse checkout. This keeps the contract check tied to the
    # exact workflow revision that triggered the build.
    workflow = subprocess.run(
        ['git', '-c', f'safe.directory={ROOT}', 'show', 'HEAD:.github/workflows/build.yml'],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
        encoding='utf-8',
    ).stdout
except (OSError, subprocess.CalledProcessError) as error:
    if workflow_path.is_file():
        workflow = workflow_path.read_text(encoding='utf-8')
    else:
        errors.append(f'build.yml: cannot inspect tracked workflow for page-engine contracts: {error}')
        workflow = ''
if '-DWKC_ENABLE_FULL_WEBCORE=ON' not in workflow:
    errors.append('build.yml: CI does not configure the full WebCore page engine')
if 'python3 Tools/wiiu/validate_page_loading.py' not in workflow:
    errors.append('build.yml: semantic page-loading validation is not a required workflow step')

# Generator failures must stop CI instead of leaving stale or empty outputs.
if '|| echo "FAIL' in workflow:
    errors.append('build.yml: a source generator failure is swallowed instead of failing CI')
if 'MISSING — LowLevelInterpreter.cpp will fail, build continues' in workflow:
    errors.append('build.yml: LLInt generation is allowed to fail while the build continues')
for required_output in ('EventInterfaces.h', 'EventTargetInterfaces.h', 'EventNames.h', 'Lexer.lut.h', 'KeywordLookup.h', 'RegExpJitTables.h', 'LexerUnicodePropertyTables.h', 'UnicodePatternTables.h', 'yarr/YarrCanonicalizeUnicode.cpp', 'LLIntAssembly.h'):
    if required_output not in workflow:
        errors.append(f'build.yml: required generated output is not validated: {required_output}')

if errors:
    print("WKC page-loading correctness checks FAILED:", file=sys.stderr)
    for error in errors:
        print(f" - {error}", file=sys.stderr)
    sys.exit(1)

print("WKC page-loading correctness checks passed.")
print("Checked navigation API parity, embedded CA fallback, TLS verification,")
print("response metadata, async response/body callbacks, redirects, cookies,")
print("and the WebCore-to-DocumentWriter rendering handoff.")
