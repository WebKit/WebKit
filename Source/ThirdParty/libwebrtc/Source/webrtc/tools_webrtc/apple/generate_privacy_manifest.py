#!/usr/bin/env vpython3

# Copyright (c) 2024 The WebRTC Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

__doc__ = """Generate privacy manifest of WebRTC iOS framework."""

import argparse
import plistlib
import sys


def generate_privacy_manifest(out_file):
    privacy_manifest = {
        "NSPrivacyTracking":
        False,
        "NSPrivacyCollectedDataTypes": [],
        "NSPrivacyTrackingDomains": [],
        "NSPrivacyAccessedAPITypes": [
            # For mach_absolute_time usage in rtc_base/system_time.cc
            {
                "NSPrivacyAccessedAPIType":
                "NSPrivacyAccessedAPICategorySystemBootTime",
                "NSPrivacyAccessedAPITypeReasons": [
                    # Declare this reason to access the system boot time
                    # in order to measure the amount of time that has elapsed
                    # between events that occurred within the app or to perform
                    # calculations to enable timers.
                    "35F9.1",
                    # Declare this reason to access the system boot time to
                    # calculate absolute timestamps for events that occurred
                    # within your app, such as events related to the UIKit or
                    # AVFAudio frameworks.
                    "8FFB.1",
                ]
            },
            # For stat usage in rtc_base/file_rotating_stream.cc
            # TODO: bugs.webrtc.org/337909152 - Make this optional since this
            # is only used for RTCFileLogger, which is not used by default and
            # not considered as a core feature.
            {
                "NSPrivacyAccessedAPIType":
                "NSPrivacyAccessedAPICategoryFileTimestamp",
                "NSPrivacyAccessedAPITypeReasons": [
                    # Declare this reason to access the timestamps, size, or
                    # other metadata of files inside the app container, app
                    # group container, or the app’s CloudKit container.
                    "C617.1"
                ]
            }
        ]
    }

    with open(out_file, 'wb') as file:
        plistlib.dump(privacy_manifest, file, fmt=plistlib.FMT_XML)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-o", "--output", type=str, help="Output file.")
    # TODO: bugs.webrtc.org/337909152 - Add an option to not to emit privacy
    # manifest entries for NSPrivacyAccessedAPICategoryFileTimestamp

    args = parser.parse_args()

    if not args.output:
        print("Output file is required")
        return 1

    generate_privacy_manifest(args.output)

    return 0


if __name__ == '__main__':
    sys.exit(main())
