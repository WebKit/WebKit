#!/usr/bin/python -B

""" Usage: make-normalize-generateddata-input.py PATH_TO_MOZILLA_CENTRAL

    This script generates test input data for String.prototype.normalize
    from intl/icu/source/data/unidata/NormalizationTest.txt
    to js/src/tests/non262/String/normalize-generateddata-input.js
"""

import re
import sys

sep_pat = re.compile(" +")


def to_code_list(codes):
    return "[" + ", ".join("0x{0}".format(x) for x in re.split(sep_pat, codes)) + "]"


def convert(dir):
    ver_pat = re.compile(r"NormalizationTest-([0-9\.]+)\.txt")
    part_pat = re.compile("^@(Part([0-9]+) .+)$")
    test_pat = re.compile(
        "^([0-9A-Fa-f ]+);([0-9A-Fa-f ]+);([0-9A-Fa-f ]+);([0-9A-Fa-f ]+);([0-9A-Fa-f ]+);$"
    )
    ignore_pat = re.compile("^#|^$")
    js_path = "js/src/tests/non262/String/normalize-generateddata-input.js"
    txt_path = "intl/icu/source/data/unidata/NormalizationTest.txt"

    part_opened = False
    not_empty = False
    with open("{dir}/{path}".format(dir=dir, path=txt_path), "r") as f:
        with open("{dir}/{path}".format(dir=dir, path=js_path), "w") as outf:
            for line in f:
                m = test_pat.search(line)
                if m:
                    if not_empty:
                        outf.write(",")
                    outf.write("\n")
                    pat = "{{ source: {source}, NFC: {NFC}, NFD: {NFD}, NFKC: {NFKC}, NFKD: {NFKD} }}"  # NOQA: E501
                    outf.write(
                        pat.format(
                            source=to_code_list(m.group(1)),
                            NFC=to_code_list(m.group(2)),
                            NFD=to_code_list(m.group(3)),
                            NFKC=to_code_list(m.group(4)),
                            NFKD=to_code_list(m.group(5)),
                        )
                    )
                    not_empty = True
                    continue
                m = part_pat.search(line)
                if m:
                    desc = m.group(1)
                    part = m.group(2)
                    if part_opened:
                        outf.write("\n];\n")
                    outf.write("/* {desc} */\n".format(desc=desc))
                    outf.write("var tests_part{part} = [".format(part=part))
                    part_opened = True
                    not_empty = False
                    continue
                m = ver_pat.search(line)
                if m:
                    ver = m.group(1)
                    outf.write(
                        "/* created from NormalizationTest-{ver}.txt */\n".format(
                            ver=ver
                        )
                    )
                    continue
                m = ignore_pat.search(line)
                if m:
                    continue
                print("Unknown line: {0}".format(line), file=sys.stderr)
            if part_opened:
                outf.write("\n];\n")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(
            "Usage: make-normalize-generateddata-input.py PATH_TO_MOZILLA_CENTRAL",
            file=sys.stderr,
        )
        sys.exit(1)
    convert(sys.argv[1])
