#!/usr/bin/env python3
"""Static correctness gates for the Wii U in-process WebKit loading path.

These checks complement (not replace) the cross-compile and runtime browser tests.
They deliberately fail on semantically dangerous regressions that can still compile.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
VIEW_CPP = ROOT / "Source/WebKit/WKC/webkit/WKCWebView.cpp"
VIEW_H = ROOT / "Source/WebKit/WKC/webkit/WKCWebView.h"
RESOURCE = ROOT / "Source/WebCore/platform/WKC/ResourceHandleWKC.cpp"
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
resource = require(RESOURCE, "ResourceHandleWKC.cpp", [
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
    "applyCookiesToRequest(",
    "removeActiveTransfer(",
])
cmake = require(CMAKE, "CMakeLists.txt", [
    "WKC_ENABLE_FULL_WEBCORE",
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
if "if (WKC_ENABLE_FULL_WEBCORE)" not in cmake:
    errors.append("CMakeLists.txt: full-WebCore guard is not conditional on the full engine")

if errors:
    print("WKC page-loading correctness checks FAILED:", file=sys.stderr)
    for error in errors:
        print(f" - {error}", file=sys.stderr)
    sys.exit(1)

print("WKC page-loading correctness checks passed.")
print("Checked navigation API parity, URL validation, in-process resource lifecycle,")
print("async/sync transfer callbacks, redirects, cookies, TLS verification, and CMake inclusion.")
