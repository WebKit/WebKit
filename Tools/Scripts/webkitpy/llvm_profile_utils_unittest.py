import contextlib
import glob
import os
import subprocess
import sys
import tempfile
import unittest

from unittest import mock

from webkitpy.llvm_profile_utils import (FRAMEWORK_NAME_PATTERN, LLVMProfDataExecutable, LLVMProfileData,
                                         ProfiledFramework, REQUIRED_FRAMEWORK_NAMES,
                                         merge_raw_profiles_in_directory, resolve_profiled_frameworks,
                                         weighted_profiles_for_framework)

SCRIPTS_DIRECTORY = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PGO_PROFILE = os.path.join(SCRIPTS_DIRECTORY, 'pgo-profile')


def write_file(path, contents=b'profile'):
    with open(path, 'wb') as profile:
        profile.write(contents)
    return path


class ResolveProfiledFrameworksTest(unittest.TestCase):
    def _directory(self, stack, filenames):
        directory = stack.enter_context(tempfile.TemporaryDirectory())
        for filename in filenames:
            write_file(os.path.join(directory, filename))
        return directory

    def _names(self, frameworks):
        return [framework.name for framework in frameworks]

    def test_the_required_frameworks_are_the_three_instrumented_ones(self):
        self.assertEqual(REQUIRED_FRAMEWORK_NAMES, ('JavaScriptCore', 'WebCore', 'WebKit'))

    def test_an_empty_directory_still_yields_the_required_frameworks(self):
        with contextlib.ExitStack() as stack:
            directory = self._directory(stack, [])

            self.assertEqual(resolve_profiled_frameworks([directory]),
                             (ProfiledFramework('JavaScriptCore'), ProfiledFramework('WebCore'),
                              ProfiledFramework('WebKit')))

    def test_a_required_framework_with_no_profile_is_still_returned_and_still_required(self):
        with contextlib.ExitStack() as stack:
            directory = self._directory(stack, ['WebGPU.profdata'])

            frameworks = resolve_profiled_frameworks([directory])

            self.assertEqual(frameworks,
                             (ProfiledFramework('JavaScriptCore'), ProfiledFramework('WebCore'),
                              ProfiledFramework('WebKit'), ProfiledFramework('WebGPU', optional=True)))
            for framework in frameworks[:3]:
                self.assertFalse(framework.optional, msg=framework.name)

    def test_discovered_frameworks_are_optional_sorted_and_after_the_required_ones(self):
        with contextlib.ExitStack() as stack:
            directory = self._directory(stack, ['WebGPU.profdata', 'ANGLE.profdata', 'bmalloc.profdata',
                                                'WebCore.profdata'])

            self.assertEqual(resolve_profiled_frameworks([directory]),
                             (ProfiledFramework('JavaScriptCore'), ProfiledFramework('WebCore'),
                              ProfiledFramework('WebKit'),
                              ProfiledFramework('ANGLE', optional=True),
                              ProfiledFramework('WebGPU', optional=True),
                              ProfiledFramework('bmalloc', optional=True)))

    def test_a_discovered_required_framework_is_not_repeated_as_optional(self):
        with contextlib.ExitStack() as stack:
            directory = self._directory(stack, ['JavaScriptCore.profdata', 'WebCore.profdata',
                                                'WebKit.profdata'])

            self.assertEqual(self._names(resolve_profiled_frameworks([directory])),
                             list(REQUIRED_FRAMEWORK_NAMES))

    def test_frameworks_are_unioned_across_directories_and_de_duplicated(self):
        with contextlib.ExitStack() as stack:
            speedometer = self._directory(stack, ['WebCore.profdata', 'WebGPU.profdata'])
            jetstream = self._directory(stack, ['WebCore.profdata', 'ANGLE.profdata'])
            motionmark = self._directory(stack, ['WebGPU.profdata'])

            self.assertEqual(self._names(resolve_profiled_frameworks([speedometer, jetstream, motionmark])),
                             ['JavaScriptCore', 'WebCore', 'WebKit', 'ANGLE', 'WebGPU'])

    def test_raw_profile_names_are_parsed_back_into_framework_names(self):
        with contextlib.ExitStack() as stack:
            directory = self._directory(stack, [
                'JavaScriptCore_18358510797273082036_0_pid14418.profraw',
                'WebGPU_5744521472931324355_0_pid14485.profraw',
                'WebKit_11317137020231888704_0_pid15738.profraw',
            ])

            self.assertEqual(self._names(resolve_profiled_frameworks([directory], suffix='.profraw')),
                             ['JavaScriptCore', 'WebCore', 'WebKit', 'WebGPU'])

    def test_raw_profile_names_are_parsed_whatever_the_expansions_produced(self):
        for filename in ('WebGPU_11317137020231888704_0_pid15738_1.profraw',
                         'WebGPU_12345_pid1.profraw',
                         'WebGPU_%m_pid%p%c.profraw'):
            with contextlib.ExitStack() as stack:
                directory = self._directory(stack, [filename])

                self.assertEqual(self._names(resolve_profiled_frameworks([directory], suffix='.profraw')),
                                 ['JavaScriptCore', 'WebCore', 'WebKit', 'WebGPU'], msg=filename)

    def test_several_raw_profiles_of_one_framework_discover_it_once(self):
        with contextlib.ExitStack() as stack:
            directory = self._directory(stack, ['WebGPU_1_0_pid1.profraw', 'WebGPU_1_0_pid2.profraw',
                                                'WebGPU_2_0_pid3.profraw'])

            self.assertEqual(self._names(resolve_profiled_frameworks([directory], suffix='.profraw')),
                             ['JavaScriptCore', 'WebCore', 'WebKit', 'WebGPU'])

    def test_a_raw_profile_with_no_underscore_is_ignored_with_a_warning(self):
        for filename in ('default.profraw', 'WebGPU.profraw', 'JavaScriptCore.profraw'):
            with contextlib.ExitStack() as stack:
                directory = self._directory(stack, [filename])

                with self.assertLogs('webkitpy.llvm_profile_utils', 'WARNING') as logs:
                    frameworks = resolve_profiled_frameworks([directory], suffix='.profraw')

                self.assertEqual(self._names(frameworks), list(REQUIRED_FRAMEWORK_NAMES), msg=filename)
                expected = f'expected {filename[:-len(".profraw")]}_%m_pid%p%c.profraw'
                self.assertTrue(any(expected in line for line in logs.output),
                                msg=f'{filename}: {logs.output}')

    def test_every_discovered_raw_profile_name_is_one_merges_glob_matches(self):
        with contextlib.ExitStack() as stack:
            directory = self._directory(stack, ['WebGPU_1_0_pid1.profraw', 'ANGLE_%m_pid%p%c.profraw',
                                                'bmalloc_.profraw', 'default.profraw'])

            with self.assertLogs('webkitpy.llvm_profile_utils', 'WARNING'):
                frameworks = resolve_profiled_frameworks([directory], suffix='.profraw')

            optional_names = [framework.name for framework in frameworks if framework.optional]
            self.assertEqual(optional_names, ['ANGLE', 'WebGPU', 'bmalloc'])
            for name in optional_names:
                self.assertTrue(glob.glob(os.path.join(directory, f'{name}_*.profraw')), msg=name)

    def test_only_raw_profiles_must_carry_the_naming_convention(self):
        with contextlib.ExitStack() as stack:
            profdata = self._directory(stack, ['WebGPU.profdata'])
            compressed = self._directory(stack, ['WebGPU.profdata.compressed'])

            self.assertEqual(self._names(resolve_profiled_frameworks([profdata])),
                             [*REQUIRED_FRAMEWORK_NAMES, 'WebGPU'])
            self.assertEqual(self._names(resolve_profiled_frameworks([compressed],
                                                                     suffix='.profdata.compressed')),
                             [*REQUIRED_FRAMEWORK_NAMES, 'WebGPU'])

    def test_a_discovered_profdata_name_rebuilds_the_path_it_came_from(self):
        for suffix in ('.profdata', '.profdata.compressed'):
            with contextlib.ExitStack() as stack:
                directory = self._directory(stack, [f'WebGPU{suffix}'])

                for framework in resolve_profiled_frameworks([directory], suffix=suffix):
                    if not framework.optional:
                        continue
                    self.assertTrue(os.path.isfile(os.path.join(directory, f'{framework.name}{suffix}')),
                                    msg=f'{suffix}: {framework.name}')

    def test_profdata_and_profdata_compressed_do_not_cross_match(self):
        with contextlib.ExitStack() as stack:
            directory = self._directory(stack, ['WebGPU.profdata', 'ANGLE.profdata.compressed'])

            self.assertEqual(self._names(resolve_profiled_frameworks([directory])),
                             ['JavaScriptCore', 'WebCore', 'WebKit', 'WebGPU'])
            self.assertEqual(self._names(resolve_profiled_frameworks([directory],
                                                                     suffix='.profdata.compressed')),
                             ['JavaScriptCore', 'WebCore', 'WebKit', 'ANGLE'])

    def test_files_that_are_not_profiles_are_ignored(self):
        with contextlib.ExitStack() as stack:
            directory = self._directory(stack, ['run-benchmark-http.log', 'screenshot.jpg',
                                                'WebGPU.profdata.tmp', 'profdata', '.profdata'])

            self.assertEqual(self._names(resolve_profiled_frameworks([directory])),
                             list(REQUIRED_FRAMEWORK_NAMES))
            self.assertEqual(self._names(resolve_profiled_frameworks([directory], suffix='.profraw')),
                             list(REQUIRED_FRAMEWORK_NAMES))

    def test_a_directory_named_like_a_profile_is_not_discovered(self):
        with contextlib.ExitStack() as stack:
            directory = self._directory(stack, [])
            os.makedirs(os.path.join(directory, 'WebGPU.profdata'))
            os.symlink(os.path.join(directory, 'nowhere'), os.path.join(directory, 'ANGLE.profdata'))

            self.assertEqual(self._names(resolve_profiled_frameworks([directory])),
                             list(REQUIRED_FRAMEWORK_NAMES))

    def test_a_filename_that_is_not_a_valid_framework_name_is_skipped_with_a_warning(self):
        for filename in ('9Lives.profdata', '-X.profdata', 'Web Core.profdata', 'Web_Core.profdata',
                         'Web.Core.profdata'):
            with contextlib.ExitStack() as stack:
                directory = self._directory(stack, [filename])

                with self.assertLogs('webkitpy.llvm_profile_utils', 'WARNING') as logs:
                    frameworks = resolve_profiled_frameworks([directory])

                self.assertEqual(self._names(frameworks), list(REQUIRED_FRAMEWORK_NAMES), msg=filename)
                self.assertTrue(any('is not a valid framework name' in line for line in logs.output),
                                msg=f'{filename}: {logs.output}')

    def test_framework_name_pattern_rejects_embedded_or_trailing_newline(self):
        self.assertIsNone(FRAMEWORK_NAME_PATTERN.fullmatch('WebCore\n'))
        self.assertIsNone(FRAMEWORK_NAME_PATTERN.fullmatch('Web\nCore'))

    def test_plausible_framework_names_are_discovered(self):
        names = ['ANGLE', 'WTF', 'WebGPU', 'WebKit-2', 'bmalloc', 'libc++']
        with contextlib.ExitStack() as stack:
            directory = self._directory(stack, [f'{name}.profdata' for name in names])

            self.assertEqual(self._names(resolve_profiled_frameworks([directory])),
                             list(REQUIRED_FRAMEWORK_NAMES) + sorted(names))

    def test_the_resolved_list_is_logged_once(self):
        with contextlib.ExitStack() as stack:
            directory = self._directory(stack, ['WebGPU.profdata'])

            with self.assertLogs('webkitpy.llvm_profile_utils', 'INFO') as logs:
                resolve_profiled_frameworks([directory])

            resolved = [line for line in logs.output if 'discovered' in line]
            self.assertEqual(len(resolved), 1, logs.output)
            self.assertIn('JavaScriptCore, WebCore, WebKit', resolved[0])
            self.assertIn('discovered WebGPU', resolved[0])

    def test_a_single_directory_passed_as_a_string_is_rejected(self):
        with self.assertRaisesRegex(TypeError, 'not a single path'):
            resolve_profiled_frameworks('/tmp/profiles')


class WeightedProfilesForFrameworkTest(unittest.TestCase):
    def _group_directories(self, stack, profiles_per_group):
        directories = []
        for profiles in profiles_per_group:
            directory = stack.enter_context(tempfile.TemporaryDirectory())
            for name in profiles:
                write_file(os.path.join(directory, f'{name}.profdata'), b'profdata')
            directories.append(directory)
        return directories

    def test_required_framework_uses_every_group_even_when_a_profile_is_missing(self):
        with contextlib.ExitStack() as stack:
            speedometer, jetstream, motionmark = self._group_directories(
                stack, [['WebCore'], [], ['WebCore']])

            weighted = weighted_profiles_for_framework(
                ProfiledFramework('WebCore'),
                [(speedometer, 0.6), (jetstream, 0.2), (motionmark, 0.2)])

            self.assertEqual(weighted, [
                (os.path.join(speedometer, 'WebCore.profdata'), 3),
                (os.path.join(jetstream, 'WebCore.profdata'), 1),
                (os.path.join(motionmark, 'WebCore.profdata'), 1),
            ])

    def test_optional_framework_drops_missing_groups_and_reweights(self):
        with contextlib.ExitStack() as stack:
            speedometer, jetstream, motionmark = self._group_directories(
                stack, [['WebGPU'], ['WebGPU'], []])

            with self.assertLogs('webkitpy.llvm_profile_utils', 'DEBUG') as logs:
                weighted = weighted_profiles_for_framework(
                    ProfiledFramework('WebGPU', optional=True),
                    [(speedometer, 0.6), (jetstream, 0.2), (motionmark, 0.2)])

            self.assertEqual(weighted, [
                (os.path.join(speedometer, 'WebGPU.profdata'), 11),
                (os.path.join(jetstream, 'WebGPU.profdata'), 3),
            ])
            self.assertTrue(any(os.path.join(motionmark, 'WebGPU.profdata') in line for line in logs.output))

    def test_optional_framework_present_in_a_single_group_uses_it_alone(self):
        with contextlib.ExitStack() as stack:
            speedometer, jetstream = self._group_directories(stack, [['WebGPU'], []])

            self.assertEqual(
                weighted_profiles_for_framework(ProfiledFramework('WebGPU', optional=True),
                                                [(speedometer, 0.6), (jetstream, 0.2)]),
                [(os.path.join(speedometer, 'WebGPU.profdata'), 5)])

    def test_a_group_whose_profile_is_a_directory_or_a_dangling_symlink_is_dropped(self):
        with contextlib.ExitStack() as stack:
            speedometer, jetstream, motionmark = self._group_directories(stack, [['WebGPU'], [], []])
            os.makedirs(os.path.join(jetstream, 'WebGPU.profdata'))
            os.symlink(os.path.join(motionmark, 'nothing-here'),
                       os.path.join(motionmark, 'WebGPU.profdata'))

            self.assertEqual(
                weighted_profiles_for_framework(ProfiledFramework('WebGPU', optional=True),
                                                [(speedometer, 0.6), (jetstream, 0.2), (motionmark, 0.2)]),
                [(os.path.join(speedometer, 'WebGPU.profdata'), 5)])

    def test_optional_framework_present_everywhere_matches_required_weights(self):
        with contextlib.ExitStack() as stack:
            speedometer, jetstream, motionmark = self._group_directories(
                stack, [['WebGPU'], ['WebGPU'], ['WebGPU']])
            group_weights = [(speedometer, 0.6), (jetstream, 0.2), (motionmark, 0.2)]

            self.assertEqual(
                weighted_profiles_for_framework(ProfiledFramework('WebGPU'), group_weights),
                [(os.path.join(speedometer, 'WebGPU.profdata'), 3),
                 (os.path.join(jetstream, 'WebGPU.profdata'), 1),
                 (os.path.join(motionmark, 'WebGPU.profdata'), 1)])
            self.assertEqual(
                weighted_profiles_for_framework(ProfiledFramework('WebGPU', optional=True), group_weights),
                weighted_profiles_for_framework(ProfiledFramework('WebGPU'), group_weights))

    def test_no_groups_at_all_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'at least one'):
            weighted_profiles_for_framework(ProfiledFramework('WebCore'), [])
        with self.assertRaisesRegex(ValueError, 'at least one'):
            weighted_profiles_for_framework(ProfiledFramework('WebGPU', optional=True), [])


class MergeRawProfilesTest(unittest.TestCase):
    def _recording_merge(self, calls):
        def merge(output_file, unweighted_profiles=(), weighted_profiles=()):
            calls.append((output_file, sorted(unweighted_profiles)))
            return subprocess.CompletedProcess(args=['llvm-profdata'], returncode=0, stdout='', stderr='')
        return merge

    def _write_raw_profiles(self, directory, names):
        for name in names:
            write_file(os.path.join(directory, f'{name}_arm64_pid1_0.profraw'), b'profraw')

    def test_every_framework_it_is_given_is_merged_and_its_output_returned(self):
        calls = []
        with tempfile.TemporaryDirectory() as directory:
            self._write_raw_profiles(directory, ['WebCore', 'WebGPU'])

            with mock.patch.object(LLVMProfileData, 'merge', self._recording_merge(calls)):
                output_files = merge_raw_profiles_in_directory(
                    [ProfiledFramework('WebCore'), ProfiledFramework('WebGPU', optional=True)], directory)

            self.assertEqual(output_files, [os.path.join(directory, 'WebCore.profdata'),
                                            os.path.join(directory, 'WebGPU.profdata')])
            self.assertEqual(calls, [(os.path.join(directory, 'WebCore.profdata'),
                                      [os.path.join(directory, 'WebCore_arm64_pid1_0.profraw')]),
                                     (os.path.join(directory, 'WebGPU.profdata'),
                                      [os.path.join(directory, 'WebGPU_arm64_pid1_0.profraw')])])

    def test_required_framework_without_raw_profiles_still_invokes_llvm_profdata(self):
        calls = []
        with tempfile.TemporaryDirectory() as directory:
            with mock.patch.object(LLVMProfileData, 'merge', self._recording_merge(calls)):
                merge_raw_profiles_in_directory([ProfiledFramework('WebKit')], directory)

            self.assertEqual(calls, [(os.path.join(directory, 'WebKit.profdata'), [])])

    def test_a_framework_does_not_swallow_a_longer_named_frameworks_profiles(self):
        calls = []
        with tempfile.TemporaryDirectory() as directory:
            self._write_raw_profiles(directory, ['WebKit', 'WebKitLegacy'])

            with mock.patch.object(LLVMProfileData, 'merge', self._recording_merge(calls)):
                merge_raw_profiles_in_directory(
                    [ProfiledFramework('WebKit'), ProfiledFramework('WebKitLegacy', optional=True)], directory)

            self.assertEqual(calls, [(os.path.join(directory, 'WebKit.profdata'),
                                      [os.path.join(directory, 'WebKit_arm64_pid1_0.profraw')]),
                                     (os.path.join(directory, 'WebKitLegacy.profdata'),
                                      [os.path.join(directory, 'WebKitLegacy_arm64_pid1_0.profraw')])])


REQUIRES_COMPRESSION_TOOL = unittest.skipUnless(os.path.exists('/usr/bin/compression_tool'),
                                                'compression_tool is macOS only')
REQUIRES_LLVM_PROFDATA = unittest.skipUnless(bool(LLVMProfDataExecutable.detect_binaries()),
                                             'no llvm-profdata in PATH or in an Xcode SDK')


class PGOProfileSubCommandTest(unittest.TestCase):
    def _run(self, *arguments):
        return subprocess.run([sys.executable, PGO_PROFILE, *arguments], capture_output=True, text=True)

    def _write_raw_profile(self, directory, name, counter):
        return write_file(os.path.join(directory, f'{name}_1234567890_0_pid1.profraw'),
                          f'{name}_function\n0x1234\n1\n{counter}\n'.encode())

    def _write_raw_profiles(self, directory, names):
        for index, name in enumerate(names):
            self._write_raw_profile(directory, name, index + 1)

    @REQUIRES_LLVM_PROFDATA
    def test_merge_also_merges_a_framework_it_discovered(self):
        with tempfile.TemporaryDirectory() as directory:
            self._write_raw_profiles(directory, [*REQUIRED_FRAMEWORK_NAMES, 'WebGPU'])
            write_file(os.path.join(directory, 'run-benchmark-http.log'))
            write_file(os.path.join(directory, 'screenshot.jpg'))

            process = self._run('merge', directory)

            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(sorted(name for name in os.listdir(directory) if name.endswith('.profdata')),
                             ['JavaScriptCore.profdata', 'WebCore.profdata', 'WebGPU.profdata',
                              'WebKit.profdata'])

    @REQUIRES_LLVM_PROFDATA
    def test_merge_ignores_a_raw_profile_that_does_not_follow_the_naming_convention(self):
        with tempfile.TemporaryDirectory() as directory:
            self._write_raw_profiles(directory, REQUIRED_FRAMEWORK_NAMES)
            write_file(os.path.join(directory, 'default.profraw'), b'default_function\n0x1234\n1\n1\n')

            process = self._run('merge', directory)

            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(sorted(name for name in os.listdir(directory) if name.endswith('.profdata')),
                             ['JavaScriptCore.profdata', 'WebCore.profdata', 'WebKit.profdata'])
            self.assertIn('expected default_%m_pid%p%c.profraw', process.stderr)

    def test_merge_still_fails_when_a_required_framework_produced_nothing(self):
        with tempfile.TemporaryDirectory() as directory:
            self._write_raw_profiles(directory, ['WebGPU'])

            process = self._run('merge', directory)

            self.assertNotEqual(process.returncode, 0)

    @REQUIRES_LLVM_PROFDATA
    def test_combine_drops_the_groups_without_a_discovered_framework_and_reweights(self):
        with tempfile.TemporaryDirectory() as speedometer, tempfile.TemporaryDirectory() as jetstream, \
                tempfile.TemporaryDirectory() as motionmark, tempfile.TemporaryDirectory() as output_directory:
            for group in (speedometer, jetstream, motionmark):
                self._write_raw_profiles(group, REQUIRED_FRAMEWORK_NAMES)
            for group in (speedometer, jetstream):
                self._write_raw_profile(group, 'WebGPU', 5)
            for group in (speedometer, jetstream, motionmark):
                self.assertEqual(self._run('merge', group).returncode, 0)
            self.assertFalse(os.path.exists(os.path.join(motionmark, 'WebGPU.profdata')))

            process = self._run('combine', '--speedometer3', speedometer, '--jetstream3', jetstream,
                                '--motionmark', motionmark, '--output', output_directory)

            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(sorted(os.listdir(output_directory)),
                             ['JavaScriptCore.profdata', 'WebCore.profdata', 'WebGPU.profdata',
                              'WebKit.profdata'])

            def weights_line(name):
                lines = [line for line in process.stderr.splitlines()
                         if f'Merging {name} with simplified weights:' in line]
                self.assertEqual(len(lines), 1, msg=f'{name}: {process.stderr}')
                return lines[0]

            webgpu = weights_line('WebGPU')
            self.assertIn(f"('{os.path.join(speedometer, 'WebGPU.profdata')}', 11)", webgpu)
            self.assertIn(f"('{os.path.join(jetstream, 'WebGPU.profdata')}', 3)", webgpu)
            self.assertNotIn(motionmark, webgpu)

            webcore = weights_line('WebCore')
            self.assertIn(f"('{os.path.join(speedometer, 'WebCore.profdata')}', 3)", webcore)
            self.assertIn(f"('{os.path.join(jetstream, 'WebCore.profdata')}', 1)", webcore)
            self.assertIn(f"('{os.path.join(motionmark, 'WebCore.profdata')}', 1)", webcore)

    @REQUIRES_LLVM_PROFDATA
    def test_combine_uses_a_discovered_framework_from_the_single_group_that_has_it(self):
        with tempfile.TemporaryDirectory() as speedometer, tempfile.TemporaryDirectory() as jetstream, \
                tempfile.TemporaryDirectory() as output_directory:
            for group in (speedometer, jetstream):
                self._write_raw_profiles(group, REQUIRED_FRAMEWORK_NAMES)
                self.assertEqual(self._run('merge', group).returncode, 0)
            self._write_raw_profile(speedometer, 'WebGPU', 5)
            self.assertEqual(self._run('merge', speedometer).returncode, 0)

            process = self._run('combine', '--speedometer3', speedometer, '--jetstream3', jetstream,
                                '--output', output_directory)

            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(sorted(os.listdir(output_directory)),
                             ['JavaScriptCore.profdata', 'WebCore.profdata', 'WebGPU.profdata',
                              'WebKit.profdata'])

    @REQUIRES_COMPRESSION_TOOL
    def test_compress_also_compresses_a_framework_it_discovered(self):
        with tempfile.TemporaryDirectory() as input_directory, tempfile.TemporaryDirectory() as output_directory:
            for name in (*REQUIRED_FRAMEWORK_NAMES, 'WebGPU'):
                write_file(os.path.join(input_directory, f'{name}.profdata'),
                           b'not a real profile, but compression_tool does not care' * 100)
            write_file(os.path.join(input_directory, 'run-benchmark-http.log'))

            process = self._run('compress', '--input', input_directory, '--output', output_directory)

            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(sorted(os.listdir(output_directory)),
                             ['JavaScriptCore.profdata.compressed', 'WebCore.profdata.compressed',
                              'WebGPU.profdata.compressed', 'WebKit.profdata.compressed'])

    @REQUIRES_COMPRESSION_TOOL
    def test_decompress_round_trips_a_framework_it_discovered(self):
        with tempfile.TemporaryDirectory() as profdata_directory, \
                tempfile.TemporaryDirectory() as compressed_directory, \
                tempfile.TemporaryDirectory() as round_trip_directory:
            contents = b'not a real profile, but compression_tool does not care' * 100
            for name in (*REQUIRED_FRAMEWORK_NAMES, 'WebGPU'):
                write_file(os.path.join(profdata_directory, f'{name}.profdata'), contents)

            compress = self._run('compress', '--input', profdata_directory, '--output', compressed_directory)
            self.assertEqual(compress.returncode, 0, compress.stderr)

            decompress = self._run('decompress', '--input', compressed_directory,
                                   '--output', round_trip_directory)

            self.assertEqual(decompress.returncode, 0, decompress.stderr)
            self.assertEqual(sorted(os.listdir(round_trip_directory)),
                             ['JavaScriptCore.profdata', 'WebCore.profdata', 'WebGPU.profdata',
                              'WebKit.profdata'])
            with open(os.path.join(round_trip_directory, 'WebGPU.profdata'), 'rb') as profile:
                self.assertEqual(profile.read(), contents)

    def test_compress_still_fails_for_a_missing_required_framework(self):
        with tempfile.TemporaryDirectory() as input_directory, tempfile.TemporaryDirectory() as output_directory:
            write_file(os.path.join(input_directory, 'WebGPU.profdata'))

            process = self._run('compress', '--input', input_directory, '--output', output_directory)

            self.assertNotEqual(process.returncode, 0)

    @REQUIRES_COMPRESSION_TOOL
    def test_a_compression_tool_failure_reports_what_the_tool_said(self):
        with tempfile.TemporaryDirectory() as input_directory, tempfile.TemporaryDirectory() as output_directory:
            process = self._run('compress', '--input', input_directory, '--output', output_directory)

            error_lines = [line for line in process.stderr.splitlines() if ' - ERROR: ' in line]
            self.assertTrue(error_lines, process.stderr)
            for line in error_lines:
                self.assertTrue(line.split(' - ERROR: ', 1)[1].strip(), process.stderr)

    def test_every_sub_command_logs_the_framework_list_it_resolved(self):
        with tempfile.TemporaryDirectory() as directory, tempfile.TemporaryDirectory() as output_directory:
            write_file(os.path.join(directory, 'WebGPU.profdata'))
            write_file(os.path.join(directory, 'WebGPU.profdata.compressed'))
            self._write_raw_profile(directory, 'WebGPU', 1)
            command_lines = {
                'merge': ('merge', directory),
                'combine': ('combine', '--speedometer3', directory, '--output', output_directory),
                'compress': ('compress', '--input', directory, '--output', output_directory),
                'decompress': ('decompress', '--input', directory, '--output', output_directory),
            }
            for name, arguments in command_lines.items():
                stderr = self._run(*arguments).stderr
                self.assertIn('discovered WebGPU', stderr, msg=name)
                self.assertEqual(sum(1 for line in stderr.splitlines() if 'discovered WebGPU' in line),
                                 1, msg=f'{name}: {stderr}')
