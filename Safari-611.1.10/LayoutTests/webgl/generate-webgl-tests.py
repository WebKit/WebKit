# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""generates webgl layout tests from the Khronos WebGL Conformance Tests"""

# To use this, get a copy of the WebGL conformance tests then run this script
# eg.
#
#   cd ~/temp
#   git clone git://github.com/KhronosGroup/WebGL.git
#   cd ~/WebKit/LayoutTests/webgl
#   python generate-webgl-tests.py -w ~/temp/WebGL/sdk/tests -e
#
# Now check run the LayoutTests, update TestExpectations and check in the
# result.

import copy
import os
import os.path
import sys
import re
import json
import shutil
from optparse import OptionParser

if sys.version < '2.6':
  print 'Wrong Python Version !!!: Need >= 2.6'
  sys.exit(1)


GLOBAL_OPTIONS = {
  # version use. Tests at or below this will be included.
  "version": "2.0.0",

  # version used for unlabled tests
  "default-version": "2.0",

  # If set, the version we require. Tests below this will be ignored.
  "min-version": "1.0.3",
}


def ReadFile(filename):
  """Reads a file as a string"""
  file = open(filename, "r")
  data = file.read()
  file.close()
  return data


def WriteFile(filename, data):
  """Writes a string as a file"""
  print "Writing: ", filename
  dirname = os.path.dirname(filename)
  if not os.path.exists(dirname):
    os.makedirs(dirname)
  file = open(filename, "wb")
  file.write(data)
  file.close()


def CopyTree(src, dst, ignore=None):
  """Recursively copy a directory tree"""
  names = os.listdir(src)
  if ignore is not None:
    ignored_names = ignore(src, names)
  else:
    ignored_names = set()

  if not os.path.exists(dst):
    os.makedirs(dst)
  errors = []
  for name in names:
    if name in ignored_names:
      continue
    srcname = os.path.join(src, name)
    dstname = os.path.join(dst, name)
    try:
      if os.path.isdir(srcname):
        CopyTree(srcname, dstname, ignore)
      else:
        # Will raise a SpecialFileError for unsupported file types
        shutil.copyfile(srcname, dstname)
    # catch the Error from the recursive copytree so that we can
    # continue with other files
    except Error, err:
      errors.extend(err.args[0])
    except EnvironmentError, why:
      errors.append((srcname, dstname, str(why)))
  if errors:
    raise Error, errors


def FileReader(filename):
  """A File generator that returns only non empty, non comment lines"""
  file = open(filename, "r")
  lines = file.readlines()
  file.close()
  for line_number, line in enumerate(lines):
    line = line.strip()
    if (len(line) > 0 and
        not line.startswith("#") and
        not line.startswith(";") and
        not line.startswith("//")):
      yield line_number + 1, line


def GreaterThanOrEqualToVersion(have, want):
  """Compares to version strings"""
  have = have.split(" ")[0].split(".")
  want = want.split(" ")[0].split(".")
  for ndx, want_str in enumerate(want):
    want_num = int(want_str)
    have_num = 0
    if ndx < len(have):
      have_num = int(have[ndx])
    if have_num < want_num:
      return False
    if have_num >= want_num:
      return True
  return True


def GetTestList(list_filename, dest_dir, hierarchical_options):
  global GLOBAL_OPTIONS
  tests = []
  prefix = os.path.dirname(list_filename)
  for line_number, line in FileReader(list_filename):
    args = line.split()
    test_options = {}
    non_options = []
    use_test = True
    while len(args):
      arg = args.pop(0)
      if arg.startswith("-"):
        if not arg.startswith("--"):
          raise "%s:%d bad option" % (list_filename, line_number)
        option = arg[2:]
        if option == 'slow':
          pass
        elif option == 'min-version' or option == "max-version":
          test_options[option] = args.pop(0)
        else:
          raise Exception("%s:%d unknown option '%s'" % (list_filename, line_number, arg))
      else:
        non_options.append(arg)
    url = os.path.join(prefix, " ".join(non_options))

    if not url.endswith(".txt"):
      if "min-version" in test_options:
        min_version = test_options["min-version"]
      else:
        min_version = hierarchical_options["default-version"]

      if "min-version" in GLOBAL_OPTIONS:
        use_test = GreaterThanOrEqualToVersion(min_version, GLOBAL_OPTIONS["min-version"])
      else:
        use_test = GreaterThanOrEqualToVersion(GLOBAL_OPTIONS["version"], min_version)

    if not use_test:
      continue

    if url.endswith(".txt"):
      if "min-version" in test_options:
        hierarchical_options["default-version"] = test_options["min-version"]
      tests = tests + GetTestList(
          os.path.join(prefix, url), dest_dir, copy.copy(hierarchical_options))
    else:
      tests.append({"url": url})
  return tests


def main(argv):
  """This is the main function."""
  global GLOBAL_OPTIONS

  parser = OptionParser()
  parser.add_option(
      "-v", "--verbose", action="store_true",
      help="prints more output.")
  parser.add_option(
      "-w", "--webgl-conformance-test", dest="source_dir",
      help="path to webgl conformance tests. REQUIRED")
  parser.add_option(
      "-n", "--no-copy", action="store_true",
      help="do not copy tests")
  parser.add_option(
      "-e", "--generate-expectations", action="store_true",
      help="generatet the test expectations")
  parser.add_option(
      "-o", "--output-dir", dest="output_dir",
      help="base directory for output. defaults to \'.\'",
      default=".")

  (options, args) = parser.parse_args(args=argv)

  if not options.source_dir:
    parser.print_help()
    return 1

  os.chdir(os.path.dirname(__file__) or '.')

  source_dir = options.source_dir;
  output_dir = options.output_dir;
  webgl_tests_dir = os.path.join(output_dir, "resources/webgl_test_files")

  # copy all the files from the WebGL conformance tests.
  if not options.no_copy:
    CopyTree(
        source_dir, webgl_tests_dir, shutil.ignore_patterns(
            '.git', '*.pyc', 'tmp*'))

  test_template = ReadFile("resources/webgl-wrapper-template.html")
  expectation_template = ReadFile("resources/webgl-expectation-template.txt")

  # generate wrappers for all the tests
  tests = GetTestList(os.path.join(source_dir, "00_test_list.txt"), ".",
                      copy.copy(GLOBAL_OPTIONS))

  for test in tests:
    url = os.path.relpath(test["url"], source_dir)
    dst = os.path.join(output_dir, url)
    dst_dir = os.path.dirname(dst)
    src = os.path.relpath(os.path.join(webgl_tests_dir, url), dst_dir).replace("\\", "/")
    base_url = os.path.relpath(output_dir, dst_dir).replace("\\", "/")
    subs = {
      "url": src,
      "url_name": os.path.basename(url),
      "base_url": base_url,
    }
    WriteFile(dst, test_template % subs)
    if options.generate_expectations:
      expectation_filename = os.path.splitext(dst)[0] + "-expected.txt"
      WriteFile(expectation_filename, expectation_template % subs)



if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))





