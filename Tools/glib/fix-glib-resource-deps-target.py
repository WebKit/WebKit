#!/usr/bin/env python3

import os
import re
import sys
import traceback


# Workaround glib-compile-resources dependency file issue https://gitlab.gnome.org/GNOME/glib/-/issues/2829
def main():

    if len(sys.argv) != 4:
        print("Usage: python3 script.py GResource.deps GResourceFile.xml Parent")
        sys.exit(1)

    deps_file = sys.argv[1]
    xml_file = sys.argv[2]
    parent_path = sys.argv[3]
    c_file = os.path.splitext(xml_file)[0] + ".c"

    with open(deps_file, "r+", encoding="utf-8") as file:
        content = file.read()
        # Ignore likely fixed files
        if not content.startswith(xml_file):
            return

        # Also workaround ninja expecting relative paths as targets in deps files
        relative_c_file = re.sub(f"{re.escape(parent_path)}/?", "", c_file)
        updated_content = re.sub(
            f"({re.escape(xml_file)}):", f"{relative_c_file}: {xml_file}", content
        )
        file.seek(0)
        file.write(updated_content)
        file.truncate()


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"Failed to process dependency file {sys.argv[1]}:", file=sys.stderr)
        traceback.print_exception(type(e), e, e.__traceback__, file=sys.stderr)
