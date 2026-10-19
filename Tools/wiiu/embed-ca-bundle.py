#!/usr/bin/env python3
"""Generate a C++ header embedding a PEM CA bundle for the Wii U cURL backend."""

import argparse
from pathlib import Path

DELIMITER = "WKC_CA_BUNDLE"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="Source PEM CA bundle")
    parser.add_argument("--output", required=True, type=Path, help="Generated C++ header")
    args = parser.parse_args()

    try:
        raw = args.input.read_bytes()
    except OSError as error:
        parser.error(f"cannot read CA bundle {args.input}: {error}")

    if b"-----BEGIN CERTIFICATE-----" not in raw:
        parser.error(f"{args.input} does not contain a PEM certificate")

    try:
        pem = raw.decode("ascii").strip()
    except UnicodeDecodeError:
        parser.error(f"{args.input} is not an ASCII PEM bundle")

    if f"){DELIMITER}\"".encode("ascii") in raw:
        parser.error("CA bundle contains the C++ raw-string terminator")

    header = (
        "#pragma once\n"
        "#include <cstddef>\n\n"
        "namespace WebCore::WKCCACertificates {\n"
        f'inline constexpr char bundle[] = R"{DELIMITER}(\n'
        f"{pem}\n"
        f'){DELIMITER}";\n'
        "inline constexpr size_t bundleSize = sizeof(bundle) - 1;\n"
        "} // namespace WebCore::WKCCACertificates\n"
    )

    try:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(header, encoding="ascii")
    except OSError as error:
        parser.error(f"cannot write generated header {args.output}: {error}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
