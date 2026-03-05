#!/usr/bin/env python3

import glob
import logging
import math
import os
import shlex
import shutil
import subprocess

from functools import cache

logger = logging.getLogger(__name__)


def locate_binary_xcrun(sdk, binary_name):
    completed_process = subprocess.run(['/usr/bin/xcrun', '-sdk', sdk, '--find', binary_name],
                                       check=False, text=True, capture_output=True)
    if completed_process.returncode:
        return None
    return completed_process.stdout.strip()


def simplify_profile_weights(profile_weights):
    simplified_profile_weights = []

    weight_sum = 0
    max_weight = 0
    # We need to turn percentages into weights > 1, but we don't want crazy high multipliers.
    # For example, if we have weights 0.35 and 0.65, we don't need a 7:13 ratio when 5:9 is good enough.
    max_multiplier = 15
    for group, weight in profile_weights:
        weight_sum = weight_sum + weight
        if weight > max_weight:
            max_weight = weight

    gcd = int(max_weight * max_multiplier)
    for group, weight in profile_weights:
        gcd = math.gcd(gcd, int((weight / weight_sum) * max_multiplier))

    for i in range(0, len(profile_weights)):
        group, weight = profile_weights[i]
        simplified_profile_weights.append((group, int((weight / weight_sum) * max_multiplier) // gcd))

    return simplified_profile_weights


class ExecutablesFromEnvAndXcode:
    PREFERRED_EXECUTABLE_INDEX = 0
    EXECUTABLE_NAME = None

    @classmethod
    @cache
    def detect_binaries(cls):
        llvm_profdata_binaries = []

        llvm_profdata_from_search_path = shutil.which(cls.EXECUTABLE_NAME)
        if llvm_profdata_from_search_path:
            llvm_profdata_binaries.append(llvm_profdata_from_search_path)

        for sdk_name in ('macosx.internal', 'iphoneos.internal', 'macosx', 'iphoneos'):
            binary_path = locate_binary_xcrun(sdk_name, cls.EXECUTABLE_NAME)
            if not binary_path:
                continue
            if binary_path in llvm_profdata_binaries:
                continue
            llvm_profdata_binaries.append(binary_path)

        logger.debug(f'Available {cls.EXECUTABLE_NAME} from {llvm_profdata_binaries}')

        return llvm_profdata_binaries

    @classmethod
    def preference_ordered_paths(cls):
        count = len(cls.detect_binaries())
        for _ in range(count):
            cls.PREFERRED_EXECUTABLE_INDEX = (cls.PREFERRED_EXECUTABLE_INDEX + 1) % count
            yield cls.detect_binaries()[cls.PREFERRED_EXECUTABLE_INDEX]

    @classmethod
    def run(cls, command, *args, check=False, stdout=None, stderr=None, capture_output=False,
            **kwargs) -> subprocess.CompletedProcess:
        kwarg_capture_output = capture_output or (stdout is None and stderr is None)

        completed_process = None
        for binary_path in cls.preference_ordered_paths():
            logger.debug(f'Running {shlex.join([binary_path, *command])}')
            completed_process = subprocess.run([binary_path, *command], *args,
                                               check=False, capture_output=kwarg_capture_output,
                                               stdout=stdout, stderr=stderr, **kwargs)
            if not completed_process.returncode:
                break

            logger.debug(f'Failed to {command} with binary {binary_path}\n'
                         f'return_code: {completed_process.returncode}\n'
                         f'stdout: {completed_process.stdout}\n'
                         f'stderr: {completed_process.stderr}\n')

        if check:
            completed_process.check_returncode()

        return completed_process


class LLVMProfDataExecutable(ExecutablesFromEnvAndXcode):
    EXECUTABLE_NAME = 'llvm-profdata'


class LLVMProfileData:
    @classmethod
    def show(cls, profile_path):
        list_functions_process = LLVMProfDataExecutable.run(['show', '--all-functions', '--value-cutoff=10',
                                                             profile_path], stdout=subprocess.PIPE, text=True)

        return subprocess.run(['/usr/bin/c++filt', '-n'], input=list_functions_process.stdout,
                              capture_output=True, text=True, check=True)

    @classmethod
    def merge(cls, output_file, unweighted_profiles=(), weighted_profiles=()):
        command = ['merge', '--sparse', *unweighted_profiles]
        for profile_path, weight in weighted_profiles:
            lib_profile_path = profile_path
            command.extend(['--weighted-input', f'{weight},{lib_profile_path}'])

        command.extend(['--output', output_file])

        return LLVMProfDataExecutable.run(command, capture_output=True, text=True)

    @classmethod
    def compress(cls, input_profile, output_file):
        return subprocess.run(['/usr/bin/compression_tool', '-encode', '-i', input_profile, '-o', output_file,
                               '-a', 'lzfse'], capture_output=True, check=True, text=True)

    @classmethod
    def decompress(cls, input_profile, output_file):
        subprocess.run(['/usr/bin/touch', output_file], check=True)
        return subprocess.run(['/usr/bin/compression_tool', '-decode', '-i', input_profile, '-o', output_file,
                               '-a', 'lzfse'], capture_output=True, check=True, text=True)


def merge_raw_profiles_in_directory_by_prefixes(prefix_list, input_directory, output_directory=None,
                                                input_suffix='.profraw', output_suffix='.profdata'):
    output_files = []
    for prefix in prefix_list:
        logger.info(f'Merging {prefix}')
        pattern = f'{prefix}*{input_suffix}'
        input_profiles = glob.glob(os.path.join(input_directory, pattern))
        output_file = os.path.join(output_directory or input_directory, f'{prefix}{output_suffix}')
        merge_process = LLVMProfileData.merge(output_file, unweighted_profiles=input_profiles)
        logger.info(f'stdout: {merge_process.stdout}')
        logger.info(f'stderr: {merge_process.stderr}')
        merge_process.check_returncode()
        output_files.append(output_file)
        logger.info(f'{prefix} is successfully merged')

    return output_files
