#! /usr/bin/env python3
#
# Copyright 2026 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
'''
Script that detects interleaved attributes in existing traces, gathers
data, and, if requested, updates the client array attribute indices,
offsets and the helper API (UpdateClientArrayPointer()) to update the
data for those attributes prior to draw, allowing the retracing script
to preserve the interleaved aspect of the said attributes.
'''

import argparse
import glob
import json
import os
import pathlib
import shutil
import stat
import struct
import sys
import time
import zlib

from collections import OrderedDict
from pathlib import Path

SCRIPT_DIR = str(pathlib.Path(__file__).resolve().parent)
COMPRESSION_WBITS = 31
DEFAULT_MERGE_FOLDER_NAME = 'merged_traces'
DEFAULT_BLOCK_SIZE = 256 * 1024 * 1024
FILE_BLOCK_INFO_SIZE = 24  # Size of FileBlockInfo
TIMEOUT_FOR_ANALYSIS = 600  # In seconds


def _is_match_with_offset(bytearr1, bytearr2, offset):
    """Compares two byte arrays, with one of them truncated
    from the beginning from a certain offset.

    * If the offset is positive, Array 1 is truncated.
    * If it is negative, Array 2 is truncated.

    Then it returns whether the remaining data matches.
    """
    comp_range_1 = bytearr1 if offset <= 0 else bytearr1[offset:]
    comp_range_2 = bytearr2 if offset >= 0 else bytearr2[-offset:]

    min_len = min(len(comp_range_1), len(comp_range_2))

    for i in range(min_len):
        if comp_range_1[i] != comp_range_2[i]:
            return False

    return True


def find_offset(bytearr1, bytearr2):
    """Determines whether two byte arrays have an offset to one another
    in terms of bytes.
    * If Array 2 is ahead of Array 1, the returned offset is positive.
        * Example:       #1 ZZABCDEF
                         #2 ABCDEFZZ
    * If Array 2 is behind Array 1, the returned offset is negative.
        * Example:       #1 ABCDEFZZ
                         #2 ZZABCDEF
    * If the two arrays do not match in any offset, None is returned.
    """
    if _is_match_with_offset(bytearr1, bytearr2, 0):
        return 0

    # To prevent false positives (e.g., they intertwine only at the last element or two),
    # the range will be limited to half the length of the arrays.
    min_len = int(min(len(bytearr1) / 2, len(bytearr2) / 2))

    for i in range(min_len):
        if _is_match_with_offset(bytearr1, bytearr2, i):
            return i
        if _is_match_with_offset(bytearr1, bytearr2, -i):
            return -i

    return None


def decompress_trace_binary(trace_dir, is_verbose=False):
    """Decompress the trace binary file (*.angledata.gz).
    * If there is a binary metadata entry in the JSON file in the trace folder,
      it is used to determine the block offsets and sizes for the decompression
      of each block.

    * If there is no binary metadata entry, the binary is decompressed with the
      values given below.

    In the end, a single decompressed byte array is returned.
    """
    # Default values if there is no trace binary metadata
    index_offset = 0
    block_count = 1
    block_size = DEFAULT_BLOCK_SIZE

    trace_name = os.path.basename(trace_dir)
    compressed_filename = os.path.join(trace_dir, trace_name + '.angledata.gz')
    json_filename = os.path.join(trace_dir, trace_name + '.json')

    gzdata = bytearray()
    should_extract_block_info = False

    with open(json_filename, 'r') as fjson:
        json_data = json.loads(fjson.read())
        if 'BinaryMetadata' not in json_data:
            if is_verbose:
                print('Binary metadata does not exist; will load the entire binary file')
        else:
            if is_verbose:
                print('Binary metadata found ({})'.format(', '.join([
                    '{}: {}'.format(name, metadata)
                    for name, metadata in json_data['BinaryMetadata'].items()
                ])))
            index_offset = int(json_data['BinaryMetadata']['IndexOffset'])
            block_count = int(json_data['BinaryMetadata']['BlockCount'])
            block_size = int(json_data['BinaryMetadata']['BlockSize'])
            should_extract_block_info = True

    with open(compressed_filename, 'rb') as fcompressed:
        if should_extract_block_info:
            fcompressed.seek(index_offset)

            block_info = fcompressed.read(block_count * FILE_BLOCK_INFO_SIZE)

            for index in range(block_count):
                file_offset, data_offset, data_size = struct.unpack(
                    '<QQQ',
                    block_info[index * FILE_BLOCK_INFO_SIZE:(index + 1) * FILE_BLOCK_INFO_SIZE])

                fcompressed.seek(file_offset)
                try:
                    decomp_block = zlib.decompress(fcompressed.read(), COMPRESSION_WBITS)
                    gzdata.extend(decomp_block[0:data_size])
                    if is_verbose:
                        print(
                            'Successfully decompressed block {}/{} (Size: {}, Total uncompressed size: {})'
                            .format(index + 1, block_count, len(decomp_block), len(gzdata)))

                except Exception as e:
                    if is_verbose:
                        print('Decompression failed: {}'.format(e))
                    return bytearray()

        else:
            fcompressed.seek(0)
            try:
                decomp_block = zlib.decompress(fcompressed.read(), COMPRESSION_WBITS)
                gzdata.extend(decomp_block)
                if is_verbose:
                    print(
                        'Successfully decompressed data (Size: {}, Uncompressed size: {})'.format(
                            len(decomp_block), len(gzdata)))
                block_size = len(gzdata)
            except Exception as e:
                if is_verbose:
                    print('Decompression failed: {}'.format(e))
                return bytearray()

    return gzdata


def extract_update_client_array_pointer_args(ucap_func_call_line):
    """It returns the args in the helper API UpdateClientArrayPointer(),
    which is used to copy the data to the client attributes prior to a
    draw call if needed.

    UpdateClientArrayPointer() takes three args:
    * Attribute index
    * Start offset
    * Size
    """
    # 'UpdateClientArrayPointerWithOffset' is generated as an output of the interleaving analysis in this script,
    # and is not expected in the input trace.
    assert not 'UpdateClientArrayPointerWithOffset' in ucap_func_call_line, 'This type of trace cannot be analyzed.'

    # The API, the parentheses and the semicolon are removed and split to get the arg strings.
    func_args_list = ucap_func_call_line.strip()[len('UpdateClientArrayPointer') +
                                                 1:-2].split(', ')
    assert len(func_args_list) == 3  # Attribute index, start offset, size

    attrib_index = int(func_args_list[0])
    start_offset_str = func_args_list[1]
    size = int(func_args_list[2])

    # The trace may be using either gBinaryData[] or GetBinaryData() with the start offset to load the trace data.
    start_offset = None
    if 'gBinaryData' in start_offset_str:
        findstart = start_offset_str.find('gBinaryData') + len('gBinaryData')
        assert start_offset_str[findstart] == '['
        findend = start_offset_str[findstart:].find(']')
        start_offset = int(start_offset_str[findstart + 1:findstart + findend])

    if 'GetBinaryData' in start_offset_str:
        findstart = start_offset_str.find('GetBinaryData') + len('GetBinaryData')
        assert start_offset_str[findstart] == '('
        findend = start_offset_str[findstart:].find(')')
        start_offset = int(start_offset_str[findstart + 1:findstart + findend])

    assert start_offset is not None

    return attrib_index, start_offset, size


def find_merged_indices_and_offsets(gzdata, ucap_info):
    """Based on the extracted UpdateClientArrayPointer(), extract the corresponding data
    from the trace binary data and compare them to determine whether they are interleaved.
    Then return the merge indices and the offsets for each attribute index.
    """
    merge_indices = dict()
    merged_offsets = dict()

    # Initialize all merge attributes as unmerged (same as attribute index, offset 0)
    for attr_index, _ in ucap_info.items():
        merge_indices[attr_index] = attr_index
        merged_offsets[attr_index] = 0

    for attr_index_1, attr_data_1 in ucap_info.items():
        if merge_indices[attr_index_1] != attr_index_1:
            # This means that this attribute is already found to be part of another merge group. So this can be skipped.
            continue
        for attr_index_2, attr_data_2 in ucap_info.items():
            if attr_index_1 >= attr_index_2:
                # To avoid duplicate comparisons or self-comparisons
                continue

            if merge_indices[attr_index_2] != attr_index_2:
                # This means that this attribute is already found to be part of another merge group. So this can be skipped.
                continue

            # We should make a merge index list and an offset list, so we know what the range of the merged memory space would be.
            start_offset_1, size_1 = attr_data_1['start_offset'], attr_data_1['size']
            start_offset_2, size_2 = attr_data_2['start_offset'], attr_data_2['size']
            data_range_1 = gzdata[start_offset_1:start_offset_1 + size_1]
            data_range_2 = gzdata[start_offset_2:start_offset_2 + size_2]
            offset = find_offset(data_range_1, data_range_2)
            if offset is not None:
                merge_indices[attr_index_2] = merge_indices[attr_index_1]
                merged_offsets[attr_index_2] = offset + merged_offsets[attr_index_1]

    return merge_indices, merged_offsets


def collect_merged_attribs_per_merged_index(merge_indices):
    """Per merge index, it shows which attributes have been
    merged into it.
    """
    merged_lists = dict()

    for attr_index, merged_index in merge_indices.items():
        if merged_index not in merged_lists:
            merged_lists[merged_index] = set()
        merged_lists[merged_index].update({attr_index})

    return merged_lists


def normalize_offsets_and_update_merged_indices_if_needed(merged_lists, merge_indices,
                                                          merged_offsets):
    """The offsets should be non-negative and at least one of them (the leader) should be 0 per merge group.
    If that was not the case, we should switch the merge groups in a way that the merged index used
    is the one with the 0 offset and the other indices merged into it have non-negative offsets.
    * Example: Offsets [0, -6, 2] (attributes merged to index 0) -> [6, 0, 8] (merged to index 1)
    """
    # The minimum offset is tracked per merge group.
    merged_global_offsets = dict()

    # In case the offsets were updated, the merged index should become the one with the offset 0
    merged_index_to_switch = list()

    for merged_index, merged_list in merged_lists.items():
        # Find minimum offset per merge group, and level them all to 0 and above.
        min_offset = min([merged_offsets[i] for i in merged_list])
        merged_global_offsets[merged_index] = min_offset

        if merged_global_offsets[merged_index] != 0:
            new_merged_index = merged_index
            for i in merged_list:
                merged_offsets[i] -= merged_global_offsets[merged_index]
                if merged_offsets[i] == 0 and new_merged_index == merged_index:
                    new_merged_index = i
            if new_merged_index != merged_index:
                merged_index_to_switch.append((merged_index, new_merged_index))

    for merged_index, new_merged_index in merged_index_to_switch:
        merged_lists[new_merged_index] = merged_lists[merged_index]
        del merged_lists[merged_index]
        for i in merged_list:
            merge_indices[i] = new_merged_index


def get_merged_attribute_data(gzdata, ucap_info):
    """Returns the following data:
    * Merged index: Maps each client attribute to the index it is merged to
    * Merged offset: Maps each client attribute to the offset it has in the merged attribute
    * Merged lists: Maps a merged index to the list of client attributes that merged to it.
    """
    merge_indices, merged_offsets = find_merged_indices_and_offsets(gzdata, ucap_info)
    merged_lists = collect_merged_attribs_per_merged_index(merge_indices)
    normalize_offsets_and_update_merged_indices_if_needed(merged_lists, merge_indices,
                                                          merged_offsets)

    return merge_indices, merged_offsets, merged_lists


def get_client_array_index_and_offset(func_call_line):
    """Get the client array index (the index of gClientArrays[]) and the offset
    in front of it (if any).
    """
    findstart = func_call_line.find('gClientArrays') + len('gClientArrays')
    assert func_call_line[findstart] == '['
    findend = func_call_line[findstart:].find(']')
    client_array_index = int(func_call_line[findstart + 1:findstart + findend])

    # Look for offset (expected to be positive if any)
    client_array_offset = 0
    find_start_offset = findstart + findend + 1
    offset_search_window = func_call_line[find_start_offset + 1:]
    assert '-' not in offset_search_window
    if '+' in offset_search_window:
        find_start_offset = offset_search_window.find('+')
        find_end_offset = offset_search_window.find(')')
        client_array_offset = int(offset_search_window[find_start_offset + 1:find_start_offset +
                                                       find_end_offset].strip())

    return client_array_index, client_array_offset


def apply_change_list_to_trace_files(trace_dir, change_list, is_verbose=False):
    """Apply the change list to the trace code. The list includes the new lines that
    should be written to specific lines of each file based on the interleaving analysis.
    """
    if len(change_list) == 0:
        # There is no change to be applied.
        return

    trace_files = glob.glob(os.path.join(trace_dir, '*'), recursive=False)
    for trace_file_num, trace_file in enumerate(trace_files):
        if '.cpp' not in trace_file and '.h' not in trace_file:
            continue

        trace_file_basename = os.path.basename(trace_file)
        if is_verbose:
            progress_log = 'Updating file {} ({}/{})'.format(trace_file_basename,
                                                             trace_file_num + 1, len(trace_files))
            # Do not print anything if the string is longer than the terminal column size to avoid spamming.
            if len(progress_log) < shutil.get_terminal_size().columns:
                print(progress_log.ljust(shutil.get_terminal_size().columns), end='\r')

        with open(trace_file, 'r', encoding='utf-8', errors='ignore') as f:
            filedata = f.readlines()

        for change in change_list[trace_file]:
            assert change['filename'] == trace_file
            filedata[change['line_num']] = change['new_line']

        with open(trace_file, 'w', encoding='utf-8', errors='ignore') as fnew:
            fnew.writelines(filedata)

    if is_verbose:
        print('\nChanges applied to trace files')


def update_max_client_attrib_size_change(size_change, new_size):
    """This function receives a pending change regarding the maximum client
    array size, which is used to initialize gClientArrays[]. It will then
    update the size to reflect the new required size for the attributes.
    """
    line = size_change['new_line']
    assert 'InitializeReplay' in line

    # After splitting by comma, element 0 would include the API and binaryDataFileName, which
    # will stay the same. However, element 1 would be maxClientArraySize, which will be updated.
    temp_args = line.split(', ')
    temp_args[1] = str(new_size)
    size_change['new_line'] = ', '.join(temp_args)


def analyze_trace_for_interleaved_attributes(trace_dir,
                                             apply_changes=False,
                                             is_verbose=False,
                                             disable_timeout=False):
    """This function analyzes the code in order to determine if the current client attribute
    data intersect and were meant to be interleaved.

    * If apply_changes is disabled, the test will check the ranges from the compressed
      data (from UpdateClientArrayPointer()) without changing the code.

        * However, it would still return the changes that would have been made if it was
          enabled. This allows the user to apply the changes later.

    * If apply_changes is enabled, the test will also apply the changes in this function.
    """
    # Output stats are initialized.
    output_stats = dict()
    output_stats['TotalGroupCount'] = 0
    output_stats['FullyMergedGroupCount'] = 0
    output_stats['PartiallyMergedGroupCount'] = 0
    output_stats['UnmergedGroupCount'] = 0
    output_stats['OneMemberGroupCount'] = 0
    output_stats['TotalUCAPCalls'] = 0
    output_stats['MaxClientArraySize'] = 0
    output_stats['ExistingNonZeroOffsetForClientArray'] = 0

    # List of changes to be applied when the processing ends
    change_list = dict()

    # Last client attribute pointer calls per attrib index
    last_client_attr_calls = dict()

    # Max client attrib size
    max_client_attrib_size = 0
    client_attrib_size_change = dict()

    # Decompress trace compressed binary for the analysis
    gzdata = decompress_trace_binary(trace_dir, is_verbose)
    if len(gzdata) == 0:
        if is_verbose:
            print('No decompressed data found')
        return output_stats, change_list

    # It is important to sort the trace files to keep track of the last client attribute calls per index.
    trace_files = sorted(glob.glob(os.path.join(trace_dir, '*'), recursive=False))

    if is_verbose:
        print('Interleaving analysis begins')

    start_time = time.time()
    for trace_file_num, trace_file in enumerate(trace_files):
        if '.cpp' not in trace_file and '.h' not in trace_file:
            continue

        current_time = time.time()
        if not disable_timeout:
            current_time = time.time()
            if current_time - start_time > TIMEOUT_FOR_ANALYSIS:
                # In case of timeout, return without applying any change to the code.
                if is_verbose:
                    print(
                        '\nTimeout occurred during interleaving analysis ({:.3f} seconds)'.format(
                            current_time - start_time))
                return output_stats, dict()

        with open(trace_file, 'r', encoding='utf-8', errors='ignore') as f:
            filedata = f.readlines()
        change_list[trace_file] = list()

        # UpdateClientArrayPointer() calls before a draw call are grouped and processed together.
        ucap_group = list()
        trace_file_basename = os.path.basename(trace_file)

        for line_num, line in enumerate(filedata):
            if is_verbose:
                progress_log = 'Processing file {} ({}/{}) | Line {}/{}'.format(
                    trace_file_basename, trace_file_num + 1, len(trace_files), line_num + 1,
                    len(filedata))
                # Do not print anything if the string is longer than the terminal column size to avoid spamming.
                if len(progress_log) < shutil.get_terminal_size().columns:
                    print(progress_log.ljust(shutil.get_terminal_size().columns), end='\r')

            if 'UpdateClientArrayPointer' in line:
                # 'UpdateClientArrayPointerWithOffset' is generated as an output of the interleaving analysis in this script,
                # and is not expected in the input trace.
                assert not 'UpdateClientArrayPointerWithOffset' in line, 'This type of trace cannot be analyzed.'
                ucap_group.append((line_num, line))
                output_stats['TotalUCAPCalls'] += 1

            elif 'gClientArrays' in line:
                # Expected in vertex attribute pointer calls or GLES1 equivalents
                client_array_index, client_array_offset = get_client_array_index_and_offset(line)
                last_client_attr_calls[client_array_index] = {
                    'file_num': trace_file_num,
                    'line_num': line_num,
                    'line': line,
                    'offset': client_array_offset
                }
                if client_array_offset != 0:
                    # In case of non-zero offset in front of gClientArrays[], it is likely that the trace has already
                    # been processed in terms of interleaved attributes.
                    output_stats['ExistingNonZeroOffsetForClientArray'] += 1

            elif 'glDraw' in line:
                if len(ucap_group) == 0:
                    # There is nothing to look at.
                    continue

                # OrderedDict was used for this to keep the order in which the client array data is updated via UpdateClientArrayPointer().
                ucap_info = OrderedDict()
                for ucap_line_num, ucap in ucap_group:
                    # The API args are extracted, which will be used to compare the memory spaces to determine if the attributes are interleaved.
                    attr_index, start_offset, size = extract_update_client_array_pointer_args(ucap)
                    ucap_info[attr_index] = {
                        'start_offset': start_offset,
                        'size': size,
                        'line_num': ucap_line_num,
                        'line': ucap
                    }

                # Based on the data from the compressed file and the UpdateClientArrayPointer() args, the merge indices and offsets are determined.
                merge_indices, merged_offsets, merged_lists = get_merged_attribute_data(
                    gzdata, ucap_info)

                # In the following loop, the line changes to the trace code are inserted into a list based on the file name.
                for attr_index, attr_data in ucap_info.items():
                    # Change the line with UpdateClientArrayPointer(). It will be replaced with UpdateClientArrayPointerWithOffset(),
                    # which has an extra starting offset in the end as a fourth arg to store on the corresponding gClientArrays[].
                    new_line = attr_data['line'].replace(
                        'UpdateClientArrayPointer({}'.format(attr_index),
                        'UpdateClientArrayPointerWithOffset({}'.format(
                            merge_indices[attr_index])).replace(
                                ');', ', {});'.format(merged_offsets[attr_index]))

                    change_list[trace_file].append({
                        'filename': trace_file,
                        'file_num': trace_file_num,
                        'line_num': attr_data['line_num'],
                        'new_line': new_line
                    })

                    # The maximum client attrib size is updated. It will be applied to the code in the end if needed.
                    max_client_attrib_size = max(max_client_attrib_size,
                                                 attr_data['size'] + merged_offsets[attr_index])

                    # Change the line about the corresponding client array call. Then remove it from the dict.
                    if attr_index in last_client_attr_calls:
                        new_line = last_client_attr_calls[attr_index]['line'].replace(
                            'gClientArrays[{}]'.format(attr_index), 'gClientArrays[{}]{}'.format(
                                merge_indices[attr_index],
                                ' + {}'.format(merged_offsets[attr_index])
                                if merged_offsets[attr_index] != 0 else ''))
                        change_list[trace_file].append({
                            'filename': trace_file,
                            'file_num': last_client_attr_calls[attr_index]['file_num'],
                            'line_num': last_client_attr_calls[attr_index]['line_num'],
                            'new_line': new_line
                        })
                        del last_client_attr_calls[attr_index]

                # Collect statistics regarding count, merging/not merging, cases with one attribute, etc.
                output_stats['TotalGroupCount'] += 1
                if len(merged_lists) == 1 and len(ucap_info) > 1:
                    output_stats['FullyMergedGroupCount'] += 1
                if len(merged_lists) > 1 and len(merged_lists) < len(ucap_info) and len(
                        ucap_info) > 1:
                    output_stats['PartiallyMergedGroupCount'] += 1
                if len(merged_lists) == 1 and len(ucap_info) == 1:
                    output_stats['OneMemberGroupCount'] += 1
                if len(merged_lists) > 1 and len(merged_lists) == len(ucap_info):
                    output_stats['UnmergedGroupCount'] += 1

                # End of attribute processing at the draw call. The processed group is cleared.
                ucap_group = []

            elif 'InitializeReplay' in line:
                # This line is used to initialize the maximum client array size. The size will be updated after the analysis.
                client_attrib_size_change = {
                    'filename': trace_file,
                    'file_num': trace_file_num,
                    'line_num': line_num,
                    'new_line': line
                }

    # Compressed data is no longer needed at this point.
    del gzdata

    # Finalize the change related to the max client attrib size.
    update_max_client_attrib_size_change(client_attrib_size_change, max_client_attrib_size)
    change_list[client_attrib_size_change['filename']].append(client_attrib_size_change)
    output_stats['MaxClientArraySize'] = max_client_attrib_size

    if is_verbose:
        end_time = time.time()
        print('\nInterleaving analysis complete ({:.3f} seconds)'.format(end_time - start_time))

    # Apply the code changes here if requested.
    if apply_changes:
        apply_change_list_to_trace_files(trace_dir, change_list, is_verbose)

    return output_stats, change_list


def get_trace_list_from_json(json_file_dir):
    """"Expected JSON format is under 'traces' in which the trace names are
    in the beginning (such as restricted_traces.json).
    """
    with open(json_file_dir, 'r') as ftracejson:
        trace_list = json.loads(ftracejson.read())
    json_trace_names = [entry.split()[0] for entry in trace_list['traces']]
    return json_trace_names


def main():
    """This script intends to take a list of trace names and analyze
    how interleaved its client array data is (if any).
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('--trace', help='Trace name to look into for attribute merge', default='')
    parser.add_argument(
        '--tracelist',
        help='List of trace names (JSON file) to look into for attribute merge '
        '(Expected format: Under `traces`; with each entry starting with the trace name)',
        default='')
    parser.add_argument(
        '--apply-changes',
        '-a',
        help='Apply changes to the code to merge interleaved client attributes',
        action='store_true')
    parser.add_argument(
        '--output-dir',
        '-o',
        help='Output directory for adding the updated traces (under `{}/`)'.format(
            DEFAULT_MERGE_FOLDER_NAME),
        default='')
    parser.add_argument('--verbose', '-v', help='Indicates verbose output', action='store_true')
    parser.add_argument(
        '--disable-timeout', help='Disables the timeout for the analysis', action='store_true')
    args = parser.parse_args()

    if not args.trace and not args.tracelist:
        print(
            'ERROR: No trace given for processing. '
            'Please use --trace for a single trace or --tracelist for a JSON file containing a list of traces.'
        )
        return
    if args.trace and args.tracelist:
        print('ERROR: Cannot use --trace and --tracelist together.')
        return
    if args.output_dir and not args.apply_changes:
        print('ERROR: Output directory not used without applied changes')
        return

    should_apply_changes = args.apply_changes
    output_dir = os.path.join(args.output_dir, DEFAULT_MERGE_FOLDER_NAME)

    trace_name_list = list()
    if args.trace:
        trace_name_list.append(args.trace)
    if args.tracelist:
        trace_name_list.extend(get_trace_list_from_json(args.tracelist))

    for trace_id, trace_name in enumerate(trace_name_list):
        if args.verbose:
            print('Processing trace `{}` ({}/{})'.format(trace_name, trace_id + 1,
                                                         len(trace_name_list)))

        trace_dir = os.path.join(SCRIPT_DIR, trace_name)
        if not os.path.exists(trace_dir):
            if args.verbose:
                print('Trace `{}` does not exist'.format(trace_name))
            continue

        # Copy the trace to the output directory only if the changes should be applied to the code
        if should_apply_changes:
            if not os.path.exists(output_dir):
                os.makedirs(output_dir)
            new_trace_dir = os.path.join(output_dir, trace_name)

            if os.path.exists(new_trace_dir):
                shutil.rmtree(new_trace_dir)
            shutil.copytree(trace_dir, new_trace_dir)
            trace_dir = new_trace_dir

            for file in os.listdir(new_trace_dir):
                os.chmod(os.path.join(new_trace_dir, file), stat.S_IREAD | stat.S_IWRITE)

        # Analyze the code for interleaved client attributes and merge them if needed
        output_stats, _ = analyze_trace_for_interleaved_attributes(trace_dir, should_apply_changes,
                                                                   args.verbose,
                                                                   args.disable_timeout)
        if args.verbose:
            print('Output stats for attribute memory comparison for trace `{}`: \n{}\n'.format(
                trace_name, '\n'.join([
                    '* {}: {}'.format(stat_name, stat_value)
                    for stat_name, stat_value in output_stats.items()
                ])))


if __name__ == '__main__':
    sys.exit(main())
