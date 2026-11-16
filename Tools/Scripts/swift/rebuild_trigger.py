#!/usr/bin/env python3
"""Update a Swift module's rebuild trigger, the generated source whose mtime
makes ninja recompile the module when a header it imports changes.

`<module>.swift-deps.d` is both the depfile of the trigger's ninja edge and one
of that edge's inputs, because ninja only ingests a depfile when the edge
declaring it runs and swiftc-wrapper.py writes this one after the module
compiles. Touching the trigger on such a run would recompile the module for
nothing, so touch only when some other input has changed since the module last
compiled. The edge carries `restat = 1`, so an untouched trigger lets ninja mark
the module clean and settle.

"Since the module last compiled" is the stamp swiftc-wrapper.py writes, not the
trigger's own mtime: the trigger edge runs early in a build, ahead of hundreds
of generated headers that the module imports, so comparing against it would
call every one of those headers a change.
"""

import argparse
import os
import sys
from pathlib import Path

import depfile


def inputs_to_compare(args):
    yield from args.inputs
    if os.path.exists(args.depfile):
        for dep in depfile.parse(args.depfile):
            yield dep if os.path.isabs(dep) else os.path.join(args.root, dep)


def any_newer(stamp_mtime, paths):
    for path in paths:
        try:
            if os.stat(path).st_mtime_ns > stamp_mtime:
                return True
        except OSError:
            # Gone: rebuild, and let the compiler write a dependency list that
            # no longer mentions it.
            return True
    return False


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trigger", required=True,
                        help="the generated .swift file whose mtime is the signal")
    parser.add_argument("--depfile", required=True,
                        help="the module's merged depfile, written by swiftc-wrapper.py")
    parser.add_argument("--stamp", required=True,
                        help="the file swiftc-wrapper.py touches when the module compiles")
    parser.add_argument("--root", default=os.getcwd(),
                        help="directory that relative depfile entries are resolved against")
    parser.add_argument("inputs", nargs="*",
                        help="other inputs of the trigger edge, e.g. the platform args .resp")
    args = parser.parse_args(argv)

    # WebKitMacros.cmake seeds the trigger at configure time; touch() also
    # creates it, for the case where it went missing.
    trigger = Path(args.trigger)
    if trigger.exists() and os.path.exists(args.stamp):
        if not any_newer(os.stat(args.stamp).st_mtime_ns, inputs_to_compare(args)):
            return 0
    trigger.touch()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
