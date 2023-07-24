# coding=utf8

# Copyright (c) 2015, Google Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

"""Enumerates source files for consumption by various build systems."""

import optparse
import os
import subprocess
import sys
import json


# OS_ARCH_COMBOS maps from OS and platform to the OpenSSL assembly "style" for
# that platform and the extension used by asm files.
#
# TODO(https://crbug.com/boringssl/524): This probably should be a map, but some
# downstream scripts import this to find what folders to add/remove from git.
OS_ARCH_COMBOS = [
    ('apple', 'arm', 'ios32', [], 'S'),
    ('apple', 'aarch64', 'ios64', [], 'S'),
    ('apple', 'x86', 'macosx', ['-fPIC', '-DOPENSSL_IA32_SSE2'], 'S'),
    ('apple', 'x86_64', 'macosx', [], 'S'),
    ('linux', 'arm', 'linux32', [], 'S'),
    ('linux', 'aarch64', 'linux64', [], 'S'),
    ('linux', 'x86', 'elf', ['-fPIC', '-DOPENSSL_IA32_SSE2'], 'S'),
    ('linux', 'x86_64', 'elf', [], 'S'),
    ('win', 'x86', 'win32n', ['-DOPENSSL_IA32_SSE2'], 'asm'),
    ('win', 'x86_64', 'nasm', [], 'asm'),
    ('win', 'aarch64', 'win64', [], 'S'),
]

# NON_PERL_FILES enumerates assembly files that are not processed by the
# perlasm system.
NON_PERL_FILES = {
    ('linux', 'arm'): [
        'src/crypto/curve25519/asm/x25519-asm-arm.S',
        'src/crypto/poly1305/poly1305_arm_asm.S',
    ],
    ('linux', 'x86_64'): [
        'src/crypto/hrss/asm/poly_rq_mul.S',
    ],
}

PREFIX = None
EMBED_TEST_DATA = True


def PathOf(x):
  return x if not PREFIX else os.path.join(PREFIX, x)


LICENSE_TEMPLATE = """Copyright (c) 2015, Google Inc.

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.""".split("\n")

def LicenseHeader(comment):
  lines = []
  for line in LICENSE_TEMPLATE:
    if not line:
      lines.append(comment)
    else:
      lines.append("%s %s" % (comment, line))
  lines.append("")
  return "\n".join(lines)


class Android(object):

  def __init__(self):
    self.header = LicenseHeader("#") + """
# This file is created by generate_build_files.py. Do not edit manually.
"""

  def PrintVariableSection(self, out, name, files):
    out.write('%s := \\\n' % name)
    for f in sorted(files):
      out.write('  %s\\\n' % f)
    out.write('\n')

  def WriteFiles(self, files, asm_outputs):
    # New Android.bp format
    with open('sources.bp', 'w+') as blueprint:
      blueprint.write(self.header.replace('#', '//'))

      #  Separate out BCM files to allow different compilation rules (specific to Android FIPS)
      bcm_c_files = files['bcm_crypto']
      non_bcm_c_files = [file for file in files['crypto'] if file not in bcm_c_files]
      non_bcm_asm = self.FilterBcmAsm(asm_outputs, False)
      bcm_asm = self.FilterBcmAsm(asm_outputs, True)

      self.PrintDefaults(blueprint, 'libcrypto_sources', non_bcm_c_files, non_bcm_asm)
      self.PrintDefaults(blueprint, 'libcrypto_bcm_sources', bcm_c_files, bcm_asm)
      self.PrintDefaults(blueprint, 'libssl_sources', files['ssl'])
      self.PrintDefaults(blueprint, 'bssl_sources', files['tool'])
      self.PrintDefaults(blueprint, 'boringssl_test_support_sources', files['test_support'])
      self.PrintDefaults(blueprint, 'boringssl_crypto_test_sources', files['crypto_test'])
      self.PrintDefaults(blueprint, 'boringssl_ssl_test_sources', files['ssl_test'])

    # Legacy Android.mk format, only used by Trusty in new branches
    with open('sources.mk', 'w+') as makefile:
      makefile.write(self.header)
      makefile.write('\n')
      self.PrintVariableSection(makefile, 'crypto_sources', files['crypto'])

      for ((osname, arch), asm_files) in asm_outputs:
        if osname != 'linux':
          continue
        self.PrintVariableSection(
            makefile, '%s_%s_sources' % (osname, arch), asm_files)

  def PrintDefaults(self, blueprint, name, files, asm_outputs={}):
    """Print a cc_defaults section from a list of C files and optionally assembly outputs"""
    blueprint.write('\n')
    blueprint.write('cc_defaults {\n')
    blueprint.write('    name: "%s",\n' % name)
    blueprint.write('    srcs: [\n')
    for f in sorted(files):
      blueprint.write('        "%s",\n' % f)
    blueprint.write('    ],\n')

    if asm_outputs:
      blueprint.write('    target: {\n')
      for ((osname, arch), asm_files) in asm_outputs:
        if osname != 'linux':
          continue
        if arch == 'aarch64':
          arch = 'arm64'

        blueprint.write('        linux_%s: {\n' % arch)
        blueprint.write('            srcs: [\n')
        for f in sorted(asm_files):
          blueprint.write('                "%s",\n' % f)
        blueprint.write('            ],\n')
        blueprint.write('        },\n')
      blueprint.write('    },\n')

    blueprint.write('}\n')

  def FilterBcmAsm(self, asm, want_bcm):
    """Filter a list of assembly outputs based on whether they belong in BCM

    Args:
      asm: Assembly file lists to filter
      want_bcm: If true then include BCM files, otherwise do not

    Returns:
      A copy of |asm| with files filtered according to |want_bcm|
    """
    # TODO(https://crbug.com/boringssl/542): Rather than filtering by filename,
    # use the variable listed in the CMake perlasm line, available in
    # ExtractPerlAsmFromCMakeFile.
    return [(archinfo, filter(lambda p: ("/crypto/fipsmodule/" in p) == want_bcm, files))
            for (archinfo, files) in asm]


class AndroidCMake(object):

  def __init__(self):
    self.header = LicenseHeader("#") + """
# This file is created by generate_build_files.py. Do not edit manually.
# To specify a custom path prefix, set BORINGSSL_ROOT before including this
# file, or use list(TRANSFORM ... PREPEND) from CMake 3.12.

"""

  def PrintVariableSection(self, out, name, files):
    out.write('set(%s\n' % name)
    for f in sorted(files):
      # Ideally adding the prefix would be the caller's job, but
      # list(TRANSFORM ... PREPEND) is only available starting CMake 3.12. When
      # sources.cmake is the source of truth, we can ask Android to either write
      # a CMake function or update to 3.12.
      out.write('  ${BORINGSSL_ROOT}%s\n' % f)
    out.write(')\n')

  def WriteFiles(self, files, asm_outputs):
    # The Android emulator uses a custom CMake buildsystem.
    #
    # TODO(davidben): Move our various source lists into sources.cmake and have
    # Android consume that directly.
    with open('android-sources.cmake', 'w+') as out:
      out.write(self.header)

      self.PrintVariableSection(out, 'crypto_sources', files['crypto'])
      self.PrintVariableSection(out, 'ssl_sources', files['ssl'])
      self.PrintVariableSection(out, 'tool_sources', files['tool'])
      self.PrintVariableSection(out, 'test_support_sources',
                                files['test_support'])
      self.PrintVariableSection(out, 'crypto_test_sources',
                                files['crypto_test'])
      self.PrintVariableSection(out, 'ssl_test_sources', files['ssl_test'])

      for ((osname, arch), asm_files) in asm_outputs:
        self.PrintVariableSection(
            out, 'crypto_sources_%s_%s' % (osname, arch), asm_files)


class Bazel(object):
  """Bazel outputs files suitable for including in Bazel files."""

  def __init__(self):
    self.firstSection = True
    self.header = \
"""# This file is created by generate_build_files.py. Do not edit manually.

"""

  def PrintVariableSection(self, out, name, files):
    if not self.firstSection:
      out.write('\n')
    self.firstSection = False

    out.write('%s = [\n' % name)
    for f in sorted(files):
      out.write('    "%s",\n' % PathOf(f))
    out.write(']\n')

  def WriteFiles(self, files, asm_outputs):
    with open('BUILD.generated.bzl', 'w+') as out:
      out.write(self.header)

      self.PrintVariableSection(out, 'ssl_headers', files['ssl_headers'])
      self.PrintVariableSection(out, 'fips_fragments', files['fips_fragments'])
      self.PrintVariableSection(
          out, 'ssl_internal_headers', files['ssl_internal_headers'])
      self.PrintVariableSection(out, 'ssl_sources', files['ssl'])
      self.PrintVariableSection(out, 'crypto_headers', files['crypto_headers'])
      self.PrintVariableSection(
          out, 'crypto_internal_headers', files['crypto_internal_headers'])
      self.PrintVariableSection(out, 'crypto_sources', files['crypto'])
      self.PrintVariableSection(out, 'tool_sources', files['tool'])
      self.PrintVariableSection(out, 'tool_headers', files['tool_headers'])

      for ((osname, arch), asm_files) in asm_outputs:
        self.PrintVariableSection(
            out, 'crypto_sources_%s_%s' % (osname, arch), asm_files)

      # Generate combined source lists for gas and nasm. Consumers have a choice
      # of using the per-platform ones or the combined ones. In the combined
      # mode, Windows x86 and Windows x86_64 must still be special-cased, but
      # otherwise all assembly files can be linked together.
      out.write('\n')
      out.write('crypto_sources_asm = []\n')
      for (osname, arch, _, _, asm_ext) in OS_ARCH_COMBOS:
        if asm_ext == 'S':
          out.write('crypto_sources_asm.extend(crypto_sources_%s_%s)\n' %
                    (osname, arch))
      out.write('\n')
      out.write('crypto_sources_nasm = []\n')
      for (osname, arch, _, _, asm_ext) in OS_ARCH_COMBOS:
        if asm_ext == 'asm':
          out.write('crypto_sources_nasm.extend(crypto_sources_%s_%s)\n' %
                    (osname, arch))

    with open('BUILD.generated_tests.bzl', 'w+') as out:
      out.write(self.header)

      out.write('test_support_sources = [\n')
      for filename in sorted(files['test_support'] +
                             files['test_support_headers'] +
                             files['crypto_internal_headers'] +
                             files['ssl_internal_headers']):
        if os.path.basename(filename) == 'malloc.cc':
          continue
        out.write('    "%s",\n' % PathOf(filename))

      out.write(']\n')

      self.PrintVariableSection(out, 'crypto_test_sources',
                                files['crypto_test'])
      self.PrintVariableSection(out, 'ssl_test_sources', files['ssl_test'])
      self.PrintVariableSection(out, 'crypto_test_data',
                                files['crypto_test_data'])
      self.PrintVariableSection(out, 'urandom_test_sources',
                                files['urandom_test'])


class Eureka(object):

  def __init__(self):
    self.header = LicenseHeader("#") + """
# This file is created by generate_build_files.py. Do not edit manually.

"""

  def PrintVariableSection(self, out, name, files):
    out.write('%s := \\\n' % name)
    for f in sorted(files):
      out.write('  %s\\\n' % f)
    out.write('\n')

  def WriteFiles(self, files, asm_outputs):
    # Legacy Android.mk format
    with open('eureka.mk', 'w+') as makefile:
      makefile.write(self.header)

      self.PrintVariableSection(makefile, 'crypto_sources', files['crypto'])
      self.PrintVariableSection(makefile, 'ssl_sources', files['ssl'])
      self.PrintVariableSection(makefile, 'tool_sources', files['tool'])

      for ((osname, arch), asm_files) in asm_outputs:
        if osname != 'linux':
          continue
        self.PrintVariableSection(
            makefile, '%s_%s_sources' % (osname, arch), asm_files)


class GN(object):

  def __init__(self):
    self.firstSection = True
    self.header = LicenseHeader("#") + """
# This file is created by generate_build_files.py. Do not edit manually.

"""

  def PrintVariableSection(self, out, name, files):
    if not self.firstSection:
      out.write('\n')
    self.firstSection = False

    out.write('%s = [\n' % name)
    for f in sorted(files):
      out.write('  "%s",\n' % f)
    out.write(']\n')

  def WriteFiles(self, files, asm_outputs):
    with open('BUILD.generated.gni', 'w+') as out:
      out.write(self.header)

      self.PrintVariableSection(out, 'crypto_sources',
                                files['crypto'] +
                                files['crypto_internal_headers'])
      self.PrintVariableSection(out, 'crypto_headers',
                                files['crypto_headers'])
      self.PrintVariableSection(out, 'ssl_sources',
                                files['ssl'] + files['ssl_internal_headers'])
      self.PrintVariableSection(out, 'ssl_headers', files['ssl_headers'])
      self.PrintVariableSection(out, 'tool_sources',
                                files['tool'] + files['tool_headers'])

      for ((osname, arch), asm_files) in asm_outputs:
        self.PrintVariableSection(
            out, 'crypto_sources_%s_%s' % (osname, arch), asm_files)

      fuzzers = [os.path.splitext(os.path.basename(fuzzer))[0]
                 for fuzzer in files['fuzz']]
      self.PrintVariableSection(out, 'fuzzers', fuzzers)

    with open('BUILD.generated_tests.gni', 'w+') as out:
      self.firstSection = True
      out.write(self.header)

      self.PrintVariableSection(out, 'test_support_sources',
                                files['test_support'] +
                                files['test_support_headers'])
      self.PrintVariableSection(out, 'crypto_test_sources',
                                files['crypto_test'])
      self.PrintVariableSection(out, 'crypto_test_data',
                                files['crypto_test_data'])
      self.PrintVariableSection(out, 'ssl_test_sources', files['ssl_test'])


class GYP(object):

  def __init__(self):
    self.header = LicenseHeader("#") + """
# This file is created by generate_build_files.py. Do not edit manually.

"""

  def PrintVariableSection(self, out, name, files):
    out.write('    \'%s\': [\n' % name)
    for f in sorted(files):
      out.write('      \'%s\',\n' % f)
    out.write('    ],\n')

  def WriteFiles(self, files, asm_outputs):
    with open('boringssl.gypi', 'w+') as gypi:
      gypi.write(self.header + '{\n  \'variables\': {\n')

      self.PrintVariableSection(gypi, 'boringssl_ssl_sources',
                                files['ssl'] + files['ssl_headers'] +
                                files['ssl_internal_headers'])
      self.PrintVariableSection(gypi, 'boringssl_crypto_sources',
                                files['crypto'] + files['crypto_headers'] +
                                files['crypto_internal_headers'])

      for ((osname, arch), asm_files) in asm_outputs:
        self.PrintVariableSection(gypi, 'boringssl_%s_%s_sources' %
                                  (osname, arch), asm_files)

      gypi.write('  }\n}\n')

class CMake(object):

  def __init__(self):
    self.header = LicenseHeader("#") + R'''
# This file is created by generate_build_files.py. Do not edit manually.

cmake_minimum_required(VERSION 3.10)

project(BoringSSL LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fno-common -fno-exceptions -fno-rtti")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden -fno-common")
endif()

# pthread_rwlock_t requires a feature flag on glibc.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=700")
endif()

if(WIN32)
  add_definitions(-D_HAS_EXCEPTIONS=0)
  add_definitions(-DWIN32_LEAN_AND_MEAN)
  add_definitions(-DNOMINMAX)
  # Allow use of fopen.
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_definitions(-DBORINGSSL_IMPLEMENTATION)

if(OPENSSL_NO_ASM)
  add_definitions(-DOPENSSL_NO_ASM)
else()
  # On x86 and x86_64 Windows, we use the NASM output.
  if(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|amd64|x86|i[3-6]86")
    enable_language(ASM_NASM)
    set(OPENSSL_NASM TRUE)
    set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -gcv8")
  else()
    enable_language(ASM)
    set(OPENSSL_ASM TRUE)
    # Work around https://gitlab.kitware.com/cmake/cmake/-/issues/20771 in older
    # CMake versions.
    if(APPLE AND CMAKE_VERSION VERSION_LESS 3.19)
      if(CMAKE_OSX_SYSROOT)
        set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -isysroot \"${CMAKE_OSX_SYSROOT}\"")
      endif()
      foreach(arch ${CMAKE_OSX_ARCHITECTURES})
        set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -arch ${arch}")
      endforeach()
    endif()
    if(NOT WIN32)
      set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wa,--noexecstack")
    endif()
    # Clang's integerated assembler does not support debug symbols.
    if(NOT CMAKE_ASM_COMPILER_ID MATCHES "Clang")
      set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wa,-g")
    endif()
  endif()
endif()

if(BUILD_SHARED_LIBS)
  add_definitions(-DBORINGSSL_SHARED_LIBRARY)
  # Enable position-independent code globally. This is needed because
  # some library targets are OBJECT libraries.
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
endif()

'''

  def PrintLibrary(self, out, name, files, libs=[]):
    out.write('add_library(\n')
    out.write('  %s\n\n' % name)

    for f in sorted(files):
      out.write('  %s\n' % PathOf(f))

    out.write(')\n\n')
    if libs:
      out.write('target_link_libraries(%s %s)\n\n' % (name, ' '.join(libs)))

  def PrintExe(self, out, name, files, libs):
    out.write('add_executable(\n')
    out.write('  %s\n\n' % name)

    for f in sorted(files):
      out.write('  %s\n' % PathOf(f))

    out.write(')\n\n')
    out.write('target_link_libraries(%s %s)\n\n' % (name, ' '.join(libs)))

  def PrintVariable(self, out, name, files):
    out.write('set(\n')
    out.write('  %s\n\n' % name)
    for f in sorted(files):
      out.write('  %s\n' % PathOf(f))
    out.write(')\n\n')

  def WriteFiles(self, files, asm_outputs):
    with open('CMakeLists.txt', 'w+') as cmake:
      cmake.write(self.header)

      asm_sources = []
      nasm_sources = []
      for ((osname, arch), asm_files) in asm_outputs:
        if (osname, arch) in (('win', 'x86'), ('win', 'x86_64')):
          nasm_sources.extend(asm_files)
        else:
          asm_sources.extend(asm_files)
      self.PrintVariable(cmake, 'CRYPTO_SOURCES_ASM', sorted(asm_sources))
      self.PrintVariable(cmake, 'CRYPTO_SOURCES_NASM', sorted(nasm_sources))

      cmake.write(
R'''if(OPENSSL_ASM)
  list(APPEND CRYPTO_SOURCES_ASM_USED ${CRYPTO_SOURCES_ASM})
endif()
if(OPENSSL_NASM)
  list(APPEND CRYPTO_SOURCES_ASM_USED ${CRYPTO_SOURCES_NASM})
endif()

''')

      self.PrintLibrary(cmake, 'crypto',
          files['crypto'] + ['${CRYPTO_SOURCES_ASM_USED}'])
      cmake.write('target_include_directories(crypto PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>)\n\n')
      self.PrintLibrary(cmake, 'ssl', files['ssl'], ['crypto'])
      self.PrintExe(cmake, 'bssl', files['tool'], ['ssl', 'crypto'])

      cmake.write(
R'''if(NOT ANDROID)
  find_package(Threads REQUIRED)
  target_link_libraries(crypto Threads::Threads)
endif()

if(WIN32)
  target_link_libraries(crypto ws2_32)
endif()

''')

class JSON(object):
  def WriteFiles(self, files, asm_outputs):
    sources = dict(files)
    for ((osname, arch), asm_files) in asm_outputs:
      sources['crypto_%s_%s' % (osname, arch)] = asm_files
    with open('sources.json', 'w+') as f:
      json.dump(sources, f, sort_keys=True, indent=2)

def FindCMakeFiles(directory):
  """Returns list of all CMakeLists.txt files recursively in directory."""
  cmakefiles = []

  for (path, _, filenames) in os.walk(directory):
    for filename in filenames:
      if filename == 'CMakeLists.txt':
        cmakefiles.append(os.path.join(path, filename))

  return cmakefiles

def OnlyFIPSFragments(path, dent, is_dir):
  return is_dir or (path.startswith(
      os.path.join('src', 'crypto', 'fipsmodule', '')) and
      NoTests(path, dent, is_dir))

def NoTestsNorFIPSFragments(path, dent, is_dir):
  return (NoTests(path, dent, is_dir) and
      (is_dir or not OnlyFIPSFragments(path, dent, is_dir)))

def NoTests(path, dent, is_dir):
  """Filter function that can be passed to FindCFiles in order to remove test
  sources."""
  if is_dir:
    return dent != 'test'
  return 'test.' not in dent


def OnlyTests(path, dent, is_dir):
  """Filter function that can be passed to FindCFiles in order to remove
  non-test sources."""
  if is_dir:
    return dent != 'test'
  return '_test.' in dent


def AllFiles(path, dent, is_dir):
  """Filter function that can be passed to FindCFiles in order to include all
  sources."""
  return True


def NoTestRunnerFiles(path, dent, is_dir):
  """Filter function that can be passed to FindCFiles or FindHeaderFiles in
  order to exclude test runner files."""
  # NOTE(martinkr): This prevents .h/.cc files in src/ssl/test/runner, which
  # are in their own subpackage, from being included in boringssl/BUILD files.
  return not is_dir or dent != 'runner'


def NotGTestSupport(path, dent, is_dir):
  return 'gtest' not in dent and 'abi_test' not in dent


def SSLHeaderFiles(path, dent, is_dir):
  return dent in ['ssl.h', 'tls1.h', 'ssl23.h', 'ssl3.h', 'dtls1.h', 'srtp.h']


def FindCFiles(directory, filter_func):
  """Recurses through directory and returns a list of paths to all the C source
  files that pass filter_func."""
  cfiles = []

  for (path, dirnames, filenames) in os.walk(directory):
    for filename in filenames:
      if not filename.endswith('.c') and not filename.endswith('.cc'):
        continue
      if not filter_func(path, filename, False):
        continue
      cfiles.append(os.path.join(path, filename))

    for (i, dirname) in enumerate(dirnames):
      if not filter_func(path, dirname, True):
        del dirnames[i]

  cfiles.sort()
  return cfiles


def FindHeaderFiles(directory, filter_func):
  """Recurses through directory and returns a list of paths to all the header files that pass filter_func."""
  hfiles = []

  for (path, dirnames, filenames) in os.walk(directory):
    for filename in filenames:
      if not filename.endswith('.h'):
        continue
      if not filter_func(path, filename, False):
        continue
      hfiles.append(os.path.join(path, filename))

      for (i, dirname) in enumerate(dirnames):
        if not filter_func(path, dirname, True):
          del dirnames[i]

  hfiles.sort()
  return hfiles


def ExtractPerlAsmFromCMakeFile(cmakefile):
  """Parses the contents of the CMakeLists.txt file passed as an argument and
  returns a list of all the perlasm() directives found in the file."""
  perlasms = []
  with open(cmakefile) as f:
    for line in f:
      line = line.strip()
      if not line.startswith('perlasm('):
        continue
      if not line.endswith(')'):
        raise ValueError('Bad perlasm line in %s' % cmakefile)
      # Remove "perlasm(" from start and ")" from end
      params = line[8:-1].split()
      if len(params) != 4:
        raise ValueError('Bad perlasm line in %s' % cmakefile)
      perlasms.append({
          'arch': params[1],
          'output': os.path.join(os.path.dirname(cmakefile), params[2]),
          'input': os.path.join(os.path.dirname(cmakefile), params[3]),
      })

  return perlasms


def ReadPerlAsmOperations():
  """Returns a list of all perlasm() directives found in CMake config files in
  src/."""
  perlasms = []
  cmakefiles = FindCMakeFiles('src')

  for cmakefile in cmakefiles:
    perlasms.extend(ExtractPerlAsmFromCMakeFile(cmakefile))

  return perlasms


def PerlAsm(output_filename, input_filename, perlasm_style, extra_args):
  """Runs the a perlasm script and puts the output into output_filename."""
  base_dir = os.path.dirname(output_filename)
  if not os.path.isdir(base_dir):
    os.makedirs(base_dir)
  subprocess.check_call(
      ['perl', input_filename, perlasm_style] + extra_args + [output_filename])


def WriteAsmFiles(perlasms):
  """Generates asm files from perlasm directives for each supported OS x
  platform combination."""
  asmfiles = {}

  for perlasm in perlasms:
    for (osname, arch, perlasm_style, extra_args, asm_ext) in OS_ARCH_COMBOS:
      if arch != perlasm['arch']:
        continue
      # TODO(https://crbug.com/boringssl/524): Now that we incorporate osname in
      # the output filename, the asm files can just go in a single directory.
      # For now, we keep them in target-specific directories to avoid breaking
      # downstream scripts.
      key = (osname, arch)
      outDir = '%s-%s' % key
      output = perlasm['output']
      if not output.startswith('src'):
        raise ValueError('output missing src: %s' % output)
      output = os.path.join(outDir, output[4:])
      output = '%s-%s.%s' % (output, osname, asm_ext)
      PerlAsm(output, perlasm['input'], perlasm_style, extra_args)
      asmfiles.setdefault(key, []).append(output)

  for (key, non_perl_asm_files) in NON_PERL_FILES.items():
    asmfiles.setdefault(key, []).extend(non_perl_asm_files)

  for files in asmfiles.values():
    files.sort()

  return asmfiles


def ExtractVariablesFromCMakeFile(cmakefile):
  """Parses the contents of the CMakeLists.txt file passed as an argument and
  returns a dictionary of exported source lists."""
  variables = {}
  in_set_command = False
  set_command = []
  with open(cmakefile) as f:
    for line in f:
      if '#' in line:
        line = line[:line.index('#')]
      line = line.strip()

      if not in_set_command:
        if line.startswith('set('):
          in_set_command = True
          set_command = []
      elif line == ')':
        in_set_command = False
        if not set_command:
          raise ValueError('Empty set command')
        variables[set_command[0]] = set_command[1:]
      else:
        set_command.extend([c for c in line.split(' ') if c])

  if in_set_command:
    raise ValueError('Unfinished set command')
  return variables


def main(platforms):
  cmake = ExtractVariablesFromCMakeFile(os.path.join('src', 'sources.cmake'))
  crypto_c_files = (FindCFiles(os.path.join('src', 'crypto'), NoTestsNorFIPSFragments) +
                    FindCFiles(os.path.join('src', 'third_party', 'fiat'), NoTestsNorFIPSFragments))
  fips_fragments = FindCFiles(os.path.join('src', 'crypto', 'fipsmodule'), OnlyFIPSFragments)
  ssl_source_files = FindCFiles(os.path.join('src', 'ssl'), NoTests)
  tool_c_files = FindCFiles(os.path.join('src', 'tool'), NoTests)
  tool_h_files = FindHeaderFiles(os.path.join('src', 'tool'), AllFiles)

  # BCM shared library C files
  bcm_crypto_c_files = [
      os.path.join('src', 'crypto', 'fipsmodule', 'bcm.c')
  ]

  # Generate err_data.c
  with open('err_data.c', 'w+') as err_data:
    subprocess.check_call(['go', 'run', 'err_data_generate.go'],
                          cwd=os.path.join('src', 'crypto', 'err'),
                          stdout=err_data)
  crypto_c_files.append('err_data.c')
  crypto_c_files.sort()

  test_support_c_files = FindCFiles(os.path.join('src', 'crypto', 'test'),
                                    NotGTestSupport)
  test_support_h_files = (
      FindHeaderFiles(os.path.join('src', 'crypto', 'test'), AllFiles) +
      FindHeaderFiles(os.path.join('src', 'ssl', 'test'), NoTestRunnerFiles))

  crypto_test_files = []
  if EMBED_TEST_DATA:
    # Generate crypto_test_data.cc
    with open('crypto_test_data.cc', 'w+') as out:
      subprocess.check_call(
          ['go', 'run', 'util/embed_test_data.go'] + cmake['CRYPTO_TEST_DATA'],
          cwd='src',
          stdout=out)
    crypto_test_files += ['crypto_test_data.cc']

  crypto_test_files += FindCFiles(os.path.join('src', 'crypto'), OnlyTests)
  crypto_test_files += [
      'src/crypto/test/abi_test.cc',
      'src/crypto/test/file_test_gtest.cc',
      'src/crypto/test/gtest_main.cc',
  ]
  # urandom_test.cc is in a separate binary so that it can be test PRNG
  # initialisation.
  crypto_test_files = [
      file for file in crypto_test_files
      if not file.endswith('/urandom_test.cc')
  ]
  crypto_test_files.sort()

  ssl_test_files = FindCFiles(os.path.join('src', 'ssl'), OnlyTests)
  ssl_test_files += [
      'src/crypto/test/abi_test.cc',
      'src/crypto/test/gtest_main.cc',
  ]
  ssl_test_files.sort()

  urandom_test_files = [
      'src/crypto/fipsmodule/rand/urandom_test.cc',
  ]

  fuzz_c_files = FindCFiles(os.path.join('src', 'fuzz'), NoTests)

  ssl_h_files = FindHeaderFiles(os.path.join('src', 'include', 'openssl'),
                                SSLHeaderFiles)

  def NotSSLHeaderFiles(path, filename, is_dir):
    return not SSLHeaderFiles(path, filename, is_dir)
  crypto_h_files = FindHeaderFiles(os.path.join('src', 'include', 'openssl'),
                                   NotSSLHeaderFiles)

  ssl_internal_h_files = FindHeaderFiles(os.path.join('src', 'ssl'), NoTests)
  crypto_internal_h_files = (
      FindHeaderFiles(os.path.join('src', 'crypto'), NoTests) +
      FindHeaderFiles(os.path.join('src', 'third_party', 'fiat'), NoTests))

  files = {
      'bcm_crypto': bcm_crypto_c_files,
      'crypto': crypto_c_files,
      'crypto_headers': crypto_h_files,
      'crypto_internal_headers': crypto_internal_h_files,
      'crypto_test': crypto_test_files,
      'crypto_test_data': sorted('src/' + x for x in cmake['CRYPTO_TEST_DATA']),
      'fips_fragments': fips_fragments,
      'fuzz': fuzz_c_files,
      'ssl': ssl_source_files,
      'ssl_headers': ssl_h_files,
      'ssl_internal_headers': ssl_internal_h_files,
      'ssl_test': ssl_test_files,
      'tool': tool_c_files,
      'tool_headers': tool_h_files,
      'test_support': test_support_c_files,
      'test_support_headers': test_support_h_files,
      'urandom_test': urandom_test_files,
  }

  asm_outputs = sorted(WriteAsmFiles(ReadPerlAsmOperations()).items())

  for platform in platforms:
    platform.WriteFiles(files, asm_outputs)

  return 0

ALL_PLATFORMS = {
    'android': Android,
    'android-cmake': AndroidCMake,
    'bazel': Bazel,
    'cmake': CMake,
    'eureka': Eureka,
    'gn': GN,
    'gyp': GYP,
    'json': JSON,
}

if __name__ == '__main__':
  parser = optparse.OptionParser(
      usage='Usage: %%prog [--prefix=<path>] [all|%s]' %
      '|'.join(sorted(ALL_PLATFORMS.keys())))
  parser.add_option('--prefix', dest='prefix',
      help='For Bazel, prepend argument to all source files')
  parser.add_option(
      '--embed_test_data', type='choice', dest='embed_test_data',
      action='store', default="true", choices=["true", "false"],
      help='For Bazel or GN, don\'t embed data files in crypto_test_data.cc')
  options, args = parser.parse_args(sys.argv[1:])
  PREFIX = options.prefix
  EMBED_TEST_DATA = (options.embed_test_data == "true")

  if not args:
    parser.print_help()
    sys.exit(1)

  if 'all' in args:
    platforms = [platform() for platform in ALL_PLATFORMS.values()]
  else:
    platforms = []
    for s in args:
      platform = ALL_PLATFORMS.get(s)
      if platform is None:
        parser.print_help()
        sys.exit(1)
      platforms.append(platform())

  sys.exit(main(platforms))
