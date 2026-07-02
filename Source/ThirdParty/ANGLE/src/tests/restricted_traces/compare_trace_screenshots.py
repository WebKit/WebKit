#! /usr/bin/env python3
#
# Copyright 2023 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
'''
compare_trace_screenshots.py

This script will cycle through screenshots from traces and compare them in useful ways.

It can run in multiple ways.

* `versus_native`

  This mode expects to be run in a directory full of two sets of screenshots

    angle_trace_tests --run-to-key-frame --screenshot-dir /tmp/screenshots
    angle_trace_tests --run-to-key-frame --screenshot-dir /tmp/screenshots --use-gl=native
    python3 compare_trace_screenshots.py versus_native --screenshot-dir /tmp/screenshots --trace-list-path ~/angle/src/tests/restricted_traces/

* `versus_upgrade`

  This mode expects to be pointed to two directories of identical images (same names and pixel contents)

    python3 compare_trace_screenshots.py versus_upgrade --before /my/trace/before --after /my/trace/after --out /my/trace/compare

* `fuzz_ab`

  Similar to `versus_upgrade` mode but uses "AE" metric with different fuzz factors like `versus_native` does

    python3 compare_trace_screenshots.py fuzz_ab --a_dir /my/trace/a --b_dir /my/trace/b --out /my/trace/diff

Prerequisites
sudo apt-get install imagemagick
'''

import argparse
import json
import logging
import os
import subprocess
import sys

DEFAULT_LOG_LEVEL = 'info'

EXIT_SUCCESS = 0
EXIT_FAILURE = 1


def compare_with_fuzz_factors(outdir, discard_zero_diff_png, trace, a_image, b_image):
    # Compare each of the images with different fuzz factors so we can see how each is doing
    # `compare -metric AE -fuzz ${FUZZ} ${A_IMAGE} ${B_IMAGE} ${TRACE}_fuzz${FUZZ}_diff.png`
    results = []
    for fuzz in {0, 1, 2, 5, 10, 20}:
        diff_file = trace + "_fuzz" + str(fuzz) + "%_TEST_diff.png"
        diff_file = os.path.join(outdir, diff_file)
        command = "compare -metric AE -fuzz " + str(
            fuzz) + "% " + a_image + " " + b_image + " " + diff_file
        logging.debug("Running " + command)
        diff = subprocess.run(command, shell=True, capture_output=True)
        for line in diff.stderr.splitlines():
            if "unable to open image".encode('UTF-8') in line:
                results.append("NA".encode('UTF-8'))
            else:
                results.append(diff.stderr)
                if discard_zero_diff_png and line.split()[0] == b'0':
                    os.remove(diff_file)
        logging.debug(" for " + trace + " " + str(fuzz) + "%")
    return results


def get_common_files(relaxed_match, a_dir, b_dir):
    # Get a list of all the files in a_dir
    a_files = sorted(os.listdir(a_dir))

    # Get a list of all the files in b_dir
    b_files = sorted(os.listdir(b_dir))

    # Can return either list if they are equal
    if a_files == b_files:
        return a_files

    extra_a_files = sorted(set(a_files) - set(b_files))
    extra_b_files = sorted(set(b_files) - set(a_files))

    print("File lists don't match!")
    if extra_a_files:
        print("Extra '%s' files: %s" % (a_dir, extra_a_files))
    if extra_b_files:
        print("Extra '%s' files: %s" % (b_dir, extra_b_files))

    # If either list is missing files, this is a fail if relaxed_match is False
    if not relaxed_match:
        exit(1)

    common_files = sorted(set(a_files) & set(b_files))
    if not common_files:
        print("No matches between file lists while using relaxed match!")
        exit(2)

    return common_files


def versus_native(args):

    # Get a list of all PNG files in the directory
    png_files = os.listdir(args.screenshot_dir)

    # Build a set of unique trace names
    traces = set()

    def get_traces_from_images():
        # Iterate through the PNG files
        for png_file in sorted(png_files):
            if png_file.startswith("angle_native") or png_file.startswith("angle_vulkan"):
                # Strip the prefix and the PNG extension from the file name
                trace_name = png_file.replace("angle_vulkan_",
                                              "").replace("swiftshader_",
                                                          "").replace("angle_native_",
                                                                      "").replace(".png", "")
                traces.add(trace_name)

    def get_traces_from_file(restricted_traces_path):
        with open(os.path.join(restricted_traces_path, "restricted_traces.json")) as f:
            trace_data = json.load(f)

        # Have to split the 'trace version' thing up
        trace_and_version = trace_data['traces']
        for i in trace_and_version:
            traces.add(i.split(' ',)[0])

    def get_trace_key_frame(restricted_traces_path, trace):
        with open(os.path.join(restricted_traces_path, trace, trace + ".json")) as f:
            single_trace_data = json.load(f)

        metadata = single_trace_data['TraceMetadata']
        keyframe = ""
        if 'KeyFrames' in metadata:
            keyframe = metadata['KeyFrames'][0]
        return keyframe

    if args.trace_list_path != None:
        get_traces_from_file(args.trace_list_path)
    else:
        get_traces_from_images()

    for trace in sorted(traces):
        frame = ""
        if args.trace_list_path != None:
            keyframe = get_trace_key_frame(args.trace_list_path, trace)
            if keyframe != "":
                frame = "_frame" + str(keyframe)

        native_file = "angle_native_" + trace + frame + ".png"
        native_file = os.path.join(args.screenshot_dir, native_file)
        if not os.path.isfile(native_file):
            native_file = "MISSING_EXT.png"

        vulkan_file = "angle_vulkan_" + trace + frame + ".png"
        vulkan_file = os.path.join(args.screenshot_dir, vulkan_file)
        if not os.path.isfile(vulkan_file):
            vulkan_file = "angle_vulkan_swiftshader_" + trace + frame + ".png"
            vulkan_file = os.path.join(args.screenshot_dir, vulkan_file)
            if not os.path.isfile(vulkan_file):
                vulkan_file = "MISSING_EXT.png"

        results = compare_with_fuzz_factors(args.screenshot_dir, args.discard_zero_diff_png, trace,
                                            vulkan_file, native_file)
        print(trace, os.path.basename(vulkan_file), os.path.basename(native_file),
              results[0].decode('UTF-8'), results[1].decode('UTF-8'), results[2].decode('UTF-8'),
              results[3].decode('UTF-8'), results[4].decode('UTF-8'), results[5].decode('UTF-8'))


def versus_upgrade(args):

    # Get a list of all the files in before and after (lists MUST match!)
    files = get_common_files(False, args.before, args.after)

    # Walk through the list and compare files in two directories
    for image in files:

        # Compare each of the images using root mean squared, no fuzz factor
        # `compare -metric RMSE ${BEFORE} ${AFTER} ${TRACE}_RMSE_diff.png;`

        results = []
        diff_file = args.outdir + "/" + image + "_TEST_diff.png"
        command = "compare -metric RMSE " + os.path.join(args.before, image) + " " + os.path.join(
            args.after, image) + " " + diff_file
        diff = subprocess.run(command, shell=True, capture_output=True)
        for line in diff.stderr.splitlines():
            if "unable to open image".encode('UTF-8') in line:
                results.append("NA".encode('UTF-8'))
            else:
                # If the last element of the diff isn't zero, there was a pixel diff
                if line.split()[-1] != b'(0)':
                    print(image, diff.stderr.decode('UTF-8'))
                    print("Pixel diff detected!")
                    exit(1)
                else:
                    results.append(diff.stderr)
                    if args.discard_zero_diff_png:
                        os.remove(diff_file)

        print(image, results[0].decode('UTF-8'))

    print("Test completed successfully, no diffs detected")


def fuzz_ab(args):

    # Get a list of common files in a_dir and b_dir
    files = get_common_files(args.relaxed_file_list_match, args.a_dir, args.b_dir)

    # Walk through the list and compare files in two directories
    for image in files:
        results = compare_with_fuzz_factors(args.outdir, args.discard_zero_diff_png, image,
                                            os.path.join(args.a_dir, image),
                                            os.path.join(args.b_dir, image))
        print(image, results[0].decode('UTF-8'), results[1].decode('UTF-8'),
              results[2].decode('UTF-8'), results[3].decode('UTF-8'), results[4].decode('UTF-8'),
              results[5].decode('UTF-8'))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', '--log', help='Logging level.', default=DEFAULT_LOG_LEVEL)
    parser.add_argument(
        '-d',
        '--discard_zero_diff_png',
        help='Discard output PNGs with zero difference.',
        action='store_true',
        default=False)

    # Create commands for different modes of using this script
    subparsers = parser.add_subparsers(dest='command', required=True, help='Command to run.')

    # This mode will compare images of two runs, vulkan vs. native, and give you fuzzy comparison results
    versus_native_parser = subparsers.add_parser(
        'versus_native', help='Compares vulkan vs. native images.')
    versus_native_parser.add_argument(
        '--screenshot-dir', help='Directory containing two sets of screenshots', required=True)
    versus_native_parser.add_argument(
        '--trace-list-path', help='Path to dir containing restricted_traces.json')

    # This mode will compare before and after images when upgrading a trace
    versus_upgrade_parser = subparsers.add_parser(
        'versus_upgrade', help='Compare images before and after an upgrade')
    versus_upgrade_parser.add_argument(
        '--before', help='Full path to dir containing *before* screenshots', required=True)
    versus_upgrade_parser.add_argument(
        '--after', help='Full path to dir containing *after* screenshots', required=True)
    versus_upgrade_parser.add_argument('--outdir', help='Where to write output files', default='.')

    # This mode will compare images in two directories, and give you fuzzy comparison results
    fuzz_ab_parser = subparsers.add_parser(
        'fuzz_ab', help='Compare images in two directories, and give you fuzzy comparison results')
    fuzz_ab_parser.add_argument(
        '-r',
        '--relaxed_file_list_match',
        help='Allow comparing file lists if there is at least single match',
        action='store_true',
        default=False)
    fuzz_ab_parser.add_argument(
        '--a_dir', help='Full path to dir containing *A* screenshots', required=True)
    fuzz_ab_parser.add_argument(
        '--b_dir', help='Full path to dir containing *B* screenshots', required=True)
    fuzz_ab_parser.add_argument('--outdir', help='Where to write output files', default='.')

    args = parser.parse_args()

    logging.basicConfig(level=args.log.upper())

    try:
        if args.command == 'versus_native':
            return versus_native(args)
        elif args.command == 'versus_upgrade':
            return versus_upgrade(args)
        elif args.command == 'fuzz_ab':
            return fuzz_ab(args)
        else:
            logging.fatal('Unknown command: %s' % args.command)
            return EXIT_FAILURE
    except subprocess.CalledProcessError as e:
        logging.exception('There was an exception: %s', e.output.decode())
        return EXIT_FAILURE


if __name__ == '__main__':
    sys.exit(main())
