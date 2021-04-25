#
# Copyright (c) 2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

import plistlib
import sys


def main(argv):
    if len(argv) != 4:
        print('Usage: ', argv[0], '<feature_flags_plist> <internal_feature_flags_plist> <output_plist>')
        return 1

    feature_flags_plist_path = argv[1]
    internal_feature_flags_plist_path = argv[2]
    output_plist_path = argv[3]

    try:
        with open(feature_flags_plist_path, 'rb') as feature_flags_file, open(internal_feature_flags_plist_path, 'rb') as internal_feature_flags_file:
            feature_flags = plistlib.load(feature_flags_file)
            internal_feature_flags = plistlib.load(internal_feature_flags_file)
            for key in internal_feature_flags:
                if key not in feature_flags:
                    print('Error!', key, 'does not exist in' + feature_flags_plist_path)
                    return 1

                if 'Attributes' in feature_flags[key] and 'Attributes' in internal_feature_flags[key]:
                    feature_flags[key]['Attributes'].update(internal_feature_flags[key]['Attributes'])
                else:
                    feature_flags[key].update(internal_feature_flags[key])

            with open(output_plist_path, 'wb') as output_plist_file:
                plistlib.dump(feature_flags, output_plist_file)
    except IOError:
        print('File read/write error! Please make sure file names', feature_flags_plist_path, 'and', internal_feature_flags_plist_path, 'are correct.')

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
