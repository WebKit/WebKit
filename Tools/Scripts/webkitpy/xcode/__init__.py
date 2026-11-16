# Required for Python to search this directory for module files

import hashlib
import struct


# The default location for Xcode's "DerivedData" (build output and intermediate
# files) is a unique directory in ~/Library/Developer/Xcode/DerivedData with a
# name incorporating a hash based on the full path of the project or workspace
# being built. The following function takes that path and returns the
# corresponding hash.
#
# The algorithm is adapted from the following article:
#
#   <https://pewpewthespells.com/blog/xcode_deriveddata_hashes.pdf>

def xcode_hash_for_path(path):
    def convert_to_string(n):
        s = ''
        for _ in range(0, 14):
            (n, r) = divmod(n, 26)
            s = chr(r + 97) + s
        return s

    (part1, part2) = struct.unpack(">QQ", hashlib.md5(path.encode()).digest())
    return convert_to_string(part1) + convert_to_string(part2)
