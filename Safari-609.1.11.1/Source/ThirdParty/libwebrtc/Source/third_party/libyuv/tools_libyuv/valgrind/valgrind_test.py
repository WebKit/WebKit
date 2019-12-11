#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Runs an exe through Valgrind and puts the intermediate files in a
directory.
"""

import datetime
import glob
import logging
import optparse
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile

import common

import memcheck_analyze

class BaseTool(object):
  """Abstract class for running dynamic error detection tools.

  Always subclass this and implement ToolCommand with framework- and
  tool-specific stuff.
  """

  def __init__(self):
    temp_parent_dir = None
    self.log_parent_dir = ""
    if common.IsWindows():
      # gpu process on Windows Vista+ runs at Low Integrity and can only
      # write to certain directories (http://crbug.com/119131)
      #
      # TODO(bruening): if scripts die in middle and don't clean up temp
      # dir, we'll accumulate files in profile dir.  should remove
      # really old files automatically.
      profile = os.getenv("USERPROFILE")
      if profile:
        self.log_parent_dir = profile + "\\AppData\\LocalLow\\"
        if os.path.exists(self.log_parent_dir):
          self.log_parent_dir = common.NormalizeWindowsPath(self.log_parent_dir)
          temp_parent_dir = self.log_parent_dir
    # Generated every time (even when overridden)
    self.temp_dir = tempfile.mkdtemp(prefix="vg_logs_", dir=temp_parent_dir)
    self.log_dir = self.temp_dir # overridable by --keep_logs
    self.option_parser_hooks = []
    # TODO(glider): we may not need some of the env vars on some of the
    # platforms.
    self._env = {
      "G_SLICE" : "always-malloc",
      "NSS_DISABLE_UNLOAD" : "1",
      "NSS_DISABLE_ARENA_FREE_LIST" : "1",
      "GTEST_DEATH_TEST_USE_FORK": "1",
    }

  def ToolName(self):
    raise NotImplementedError, "This method should be implemented " \
                               "in the tool-specific subclass"

  def Analyze(self, check_sanity=False):
    raise NotImplementedError, "This method should be implemented " \
                               "in the tool-specific subclass"

  def RegisterOptionParserHook(self, hook):
    # Frameworks and tools can add their own flags to the parser.
    self.option_parser_hooks.append(hook)

  def CreateOptionParser(self):
    # Defines Chromium-specific flags.
    self._parser = optparse.OptionParser("usage: %prog [options] <program to "
                                         "test>")
    self._parser.disable_interspersed_args()
    self._parser.add_option("-t", "--timeout",
                      dest="timeout", metavar="TIMEOUT", default=10000,
                      help="timeout in seconds for the run (default 10000)")
    self._parser.add_option("", "--build-dir",
                            help="the location of the compiler output")
    self._parser.add_option("", "--source-dir",
                            help="path to top of source tree for this build"
                                 "(used to normalize source paths in baseline)")
    self._parser.add_option("", "--gtest_filter", default="",
                            help="which test case to run")
    self._parser.add_option("", "--gtest_repeat",
                            help="how many times to run each test")
    self._parser.add_option("", "--gtest_print_time", action="store_true",
                            default=False,
                            help="show how long each test takes")
    self._parser.add_option("", "--ignore_exit_code", action="store_true",
                            default=False,
                            help="ignore exit code of the test "
                                 "(e.g. test failures)")
    self._parser.add_option("", "--keep_logs", action="store_true",
                            default=False,
                            help="store memory tool logs in the <tool>.logs "
                                 "directory instead of /tmp.\nThis can be "
                                 "useful for tool developers/maintainers.\n"
                                 "Please note that the <tool>.logs directory "
                                 "will be clobbered on tool startup.")

    # To add framework- or tool-specific flags, please add a hook using
    # RegisterOptionParserHook in the corresponding subclass.
    # See ValgrindTool for an example.
    for hook in self.option_parser_hooks:
      hook(self, self._parser)

  def ParseArgv(self, args):
    self.CreateOptionParser()

    # self._tool_flags will store those tool flags which we don't parse
    # manually in this script.
    self._tool_flags = []
    known_args = []

    """ We assume that the first argument not starting with "-" is a program
    name and all the following flags should be passed to the program.
    TODO(timurrrr): customize optparse instead
    """
    while len(args) > 0 and args[0][:1] == "-":
      arg = args[0]
      if (arg == "--"):
        break
      if self._parser.has_option(arg.split("=")[0]):
        known_args += [arg]
      else:
        self._tool_flags += [arg]
      args = args[1:]

    if len(args) > 0:
      known_args += args

    self._options, self._args = self._parser.parse_args(known_args)

    self._timeout = int(self._options.timeout)
    self._source_dir = self._options.source_dir
    if self._options.keep_logs:
      # log_parent_dir has trailing slash if non-empty
      self.log_dir = self.log_parent_dir + "%s.logs" % self.ToolName()
      if os.path.exists(self.log_dir):
        shutil.rmtree(self.log_dir)
      os.mkdir(self.log_dir)
      logging.info("Logs are in " + self.log_dir)

    self._ignore_exit_code = self._options.ignore_exit_code
    if self._options.gtest_filter != "":
      self._args.append("--gtest_filter=%s" % self._options.gtest_filter)
    if self._options.gtest_repeat:
      self._args.append("--gtest_repeat=%s" % self._options.gtest_repeat)
    if self._options.gtest_print_time:
      self._args.append("--gtest_print_time")

    return True

  def Setup(self, args):
    return self.ParseArgv(args)

  def ToolCommand(self):
    raise NotImplementedError, "This method should be implemented " \
                               "in the tool-specific subclass"

  def Cleanup(self):
    # You may override it in the tool-specific subclass
    pass

  def Execute(self):
    """ Execute the app to be tested after successful instrumentation.
    Full execution command-line provided by subclassers via proc."""
    logging.info("starting execution...")
    proc = self.ToolCommand()
    for var in self._env:
      common.PutEnvAndLog(var, self._env[var])
    return common.RunSubprocess(proc, self._timeout)

  def RunTestsAndAnalyze(self, check_sanity):
    exec_retcode = self.Execute()
    analyze_retcode = self.Analyze(check_sanity)

    if analyze_retcode:
      logging.error("Analyze failed.")
      logging.info("Search the log for '[ERROR]' to see the error reports.")
      return analyze_retcode

    if exec_retcode:
      if self._ignore_exit_code:
        logging.info("Test execution failed, but the exit code is ignored.")
      else:
        logging.error("Test execution failed.")
        return exec_retcode
    else:
      logging.info("Test execution completed successfully.")

    if not analyze_retcode:
      logging.info("Analysis completed successfully.")

    return 0

  def Main(self, args, check_sanity, min_runtime_in_seconds):
    """Call this to run through the whole process: Setup, Execute, Analyze"""
    start_time = datetime.datetime.now()
    retcode = -1
    if self.Setup(args):
      retcode = self.RunTestsAndAnalyze(check_sanity)
      shutil.rmtree(self.temp_dir, ignore_errors=True)
      self.Cleanup()
    else:
      logging.error("Setup failed")
    end_time = datetime.datetime.now()
    runtime_in_seconds = (end_time - start_time).seconds
    hours = runtime_in_seconds / 3600
    seconds = runtime_in_seconds % 3600
    minutes = seconds / 60
    seconds = seconds % 60
    logging.info("elapsed time: %02d:%02d:%02d" % (hours, minutes, seconds))
    if (min_runtime_in_seconds > 0 and
        runtime_in_seconds < min_runtime_in_seconds):
      logging.error("Layout tests finished too quickly. "
                    "It should have taken at least %d seconds. "
                    "Something went wrong?" % min_runtime_in_seconds)
      retcode = -1
    return retcode

  def Run(self, args, module, min_runtime_in_seconds=0):
    MODULES_TO_SANITY_CHECK = ["base"]

    check_sanity = module in MODULES_TO_SANITY_CHECK
    return self.Main(args, check_sanity, min_runtime_in_seconds)


class ValgrindTool(BaseTool):
  """Abstract class for running Valgrind tools.

  Always subclass this and implement ToolSpecificFlags() and
  ExtendOptionParser() for tool-specific stuff.
  """
  def __init__(self):
    super(ValgrindTool, self).__init__()
    self.RegisterOptionParserHook(ValgrindTool.ExtendOptionParser)

  def UseXML(self):
    # Override if tool prefers nonxml output
    return True

  def ExtendOptionParser(self, parser):
    parser.add_option("", "--suppressions", default=[],
                            action="append",
                            help="path to a valgrind suppression file")
    parser.add_option("", "--indirect", action="store_true",
                            default=False,
                            help="set BROWSER_WRAPPER rather than "
                                 "running valgrind directly")
    parser.add_option("", "--indirect_webkit_layout", action="store_true",
                            default=False,
                            help="set --wrapper rather than running Dr. Memory "
                                 "directly.")
    parser.add_option("", "--trace_children", action="store_true",
                            default=False,
                            help="also trace child processes")
    parser.add_option("", "--num-callers",
                            dest="num_callers", default=30,
                            help="number of callers to show in stack traces")
    parser.add_option("", "--generate_dsym", action="store_true",
                          default=False,
                          help="Generate .dSYM file on Mac if needed. Slow!")

  def Setup(self, args):
    if not BaseTool.Setup(self, args):
      return False
    return True

  def ToolCommand(self):
    """Get the valgrind command to run."""
    # Note that self._args begins with the exe to be run.
    tool_name = self.ToolName()

    # Construct the valgrind command.
    if 'CHROME_VALGRIND' in os.environ:
      path = os.path.join(os.environ['CHROME_VALGRIND'], "bin", "valgrind")
    else:
      path = "valgrind"
    proc = [path, "--tool=%s" % tool_name]

    proc += ["--num-callers=%i" % int(self._options.num_callers)]

    if self._options.trace_children:
      proc += ["--trace-children=yes"]
      proc += ["--trace-children-skip='*dbus-daemon*'"]
      proc += ["--trace-children-skip='*dbus-launch*'"]
      proc += ["--trace-children-skip='*perl*'"]
      proc += ["--trace-children-skip='*python*'"]
      # This is really Python, but for some reason Valgrind follows it.
      proc += ["--trace-children-skip='*lsb_release*'"]

    proc += self.ToolSpecificFlags()
    proc += self._tool_flags

    suppression_count = 0
    for suppression_file in self._options.suppressions:
      if os.path.exists(suppression_file):
        suppression_count += 1
        proc += ["--suppressions=%s" % suppression_file]

    if not suppression_count:
      logging.warning("WARNING: NOT USING SUPPRESSIONS!")

    logfilename = self.log_dir + ("/%s." % tool_name) + "%p"
    if self.UseXML():
      proc += ["--xml=yes", "--xml-file=" + logfilename]
    else:
      proc += ["--log-file=" + logfilename]

    # The Valgrind command is constructed.

    # Handle --indirect_webkit_layout separately.
    if self._options.indirect_webkit_layout:
      # Need to create the wrapper before modifying |proc|.
      wrapper = self.CreateBrowserWrapper(proc, webkit=True)
      proc = self._args
      proc.append("--wrapper")
      proc.append(wrapper)
      return proc

    if self._options.indirect:
      wrapper = self.CreateBrowserWrapper(proc)
      os.environ["BROWSER_WRAPPER"] = wrapper
      logging.info('export BROWSER_WRAPPER=' + wrapper)
      proc = []
    proc += self._args
    return proc

  def ToolSpecificFlags(self):
    raise NotImplementedError, "This method should be implemented " \
                               "in the tool-specific subclass"

  def CreateBrowserWrapper(self, proc, webkit=False):
    """The program being run invokes Python or something else that can't stand
    to be valgrinded, and also invokes the Chrome browser. In this case, use a
    magic wrapper to only valgrind the Chrome browser. Build the wrapper here.
    Returns the path to the wrapper. It's up to the caller to use the wrapper
    appropriately.
    """
    command = " ".join(proc)
    # Add the PID of the browser wrapper to the logfile names so we can
    # separate log files for different UI tests at the analyze stage.
    command = command.replace("%p", "$$.%p")

    (fd, indirect_fname) = tempfile.mkstemp(dir=self.log_dir,
                                            prefix="browser_wrapper.",
                                            text=True)
    f = os.fdopen(fd, "w")
    f.write('#!/bin/bash\n'
            'echo "Started Valgrind wrapper for this test, PID=$$" >&2\n')

    f.write('DIR=`dirname $0`\n'
            'TESTNAME_FILE=$DIR/testcase.$$.name\n\n')

    if webkit:
      # Webkit layout_tests pass the URL as the first line of stdin.
      f.write('tee $TESTNAME_FILE | %s "$@"\n' % command)
    else:
      # Try to get the test case name by looking at the program arguments.
      # i.e. Chromium ui_tests used --test-name arg.
      # TODO(timurrrr): This doesn't handle "--test-name Test.Name"
      # TODO(timurrrr): ui_tests are dead. Where do we use the non-webkit
      # wrapper now? browser_tests? What do they do?
      f.write('for arg in $@\ndo\n'
              '  if [[ "$arg" =~ --test-name=(.*) ]]\n  then\n'
              '    echo ${BASH_REMATCH[1]} >$TESTNAME_FILE\n'
              '  fi\n'
              'done\n\n'
              '%s "$@"\n' % command)

    f.close()
    os.chmod(indirect_fname, stat.S_IRUSR|stat.S_IXUSR)
    return indirect_fname

  def CreateAnalyzer(self):
    raise NotImplementedError, "This method should be implemented " \
                               "in the tool-specific subclass"

  def GetAnalyzeResults(self, check_sanity=False):
    # Glob all the files in the log directory
    filenames = glob.glob(self.log_dir + "/" + self.ToolName() + ".*")

    # If we have browser wrapper, the logfiles are named as
    # "toolname.wrapper_PID.valgrind_PID".
    # Let's extract the list of wrapper_PIDs and name it ppids
    ppids = set([int(f.split(".")[-2]) \
                for f in filenames if re.search("\.[0-9]+\.[0-9]+$", f)])

    analyzer = self.CreateAnalyzer()
    if len(ppids) == 0:
      # Fast path - no browser wrapper was set.
      return analyzer.Report(filenames, None, check_sanity)

    ret = 0
    for ppid in ppids:
      testcase_name = None
      try:
        f = open(self.log_dir + ("/testcase.%d.name" % ppid))
        testcase_name = f.read().strip()
        f.close()
        wk_layout_prefix="third_party/WebKit/LayoutTests/"
        wk_prefix_at = testcase_name.rfind(wk_layout_prefix)
        if wk_prefix_at != -1:
          testcase_name = testcase_name[wk_prefix_at + len(wk_layout_prefix):]
      except IOError:
        pass
      print "====================================================="
      print " Below is the report for valgrind wrapper PID=%d." % ppid
      if testcase_name:
        print " It was used while running the `%s` test." % testcase_name
      else:
        print " You can find the corresponding test"
        print " by searching the above log for 'PID=%d'" % ppid
      sys.stdout.flush()

      ppid_filenames = [f for f in filenames \
                        if re.search("\.%d\.[0-9]+$" % ppid, f)]
      # check_sanity won't work with browser wrappers
      assert check_sanity == False
      ret |= analyzer.Report(ppid_filenames, testcase_name)
      print "====================================================="
      sys.stdout.flush()

    if ret != 0:
      print ""
      print "The Valgrind reports are grouped by test names."
      print "Each test has its PID printed in the log when the test was run"
      print "and at the beginning of its Valgrind report."
      print "Hint: you can search for the reports by Ctrl+F -> `=#`"
      sys.stdout.flush()

    return ret


# TODO(timurrrr): Split into a separate file.
class Memcheck(ValgrindTool):
  """Memcheck
  Dynamic memory error detector for Linux & Mac

  http://valgrind.org/info/tools.html#memcheck
  """

  def __init__(self):
    super(Memcheck, self).__init__()
    self.RegisterOptionParserHook(Memcheck.ExtendOptionParser)

  def ToolName(self):
    return "memcheck"

  def ExtendOptionParser(self, parser):
    parser.add_option("--leak-check", "--leak_check", type="string",
                      default="yes",  # --leak-check=yes is equivalent of =full
                      help="perform leak checking at the end of the run")
    parser.add_option("", "--show_all_leaks", action="store_true",
                      default=False,
                      help="also show less blatant leaks")
    parser.add_option("", "--track_origins", action="store_true",
                      default=False,
                      help="Show whence uninitialized bytes came. 30% slower.")

  def ToolSpecificFlags(self):
    ret = ["--gen-suppressions=all", "--demangle=no"]
    ret += ["--leak-check=%s" % self._options.leak_check]

    if self._options.show_all_leaks:
      ret += ["--show-reachable=yes"]
    else:
      ret += ["--show-possibly-lost=no"]

    if self._options.track_origins:
      ret += ["--track-origins=yes"]

    # TODO(glider): this is a temporary workaround for http://crbug.com/51716
    # Let's see whether it helps.
    if common.IsMac():
      ret += ["--smc-check=all"]

    return ret

  def CreateAnalyzer(self):
    use_gdb = common.IsMac()
    return memcheck_analyze.MemcheckAnalyzer(self._source_dir,
                                            self._options.show_all_leaks,
                                            use_gdb=use_gdb)

  def Analyze(self, check_sanity=False):
    ret = self.GetAnalyzeResults(check_sanity)

    if ret != 0:
      logging.info("Please see http://dev.chromium.org/developers/how-tos/"
                   "using-valgrind for the info on Memcheck/Valgrind")
    return ret


class ToolFactory:
  def Create(self, tool_name):
    if tool_name == "memcheck":
      return Memcheck()
    try:
      platform_name = common.PlatformNames()[0]
    except common.NotImplementedError:
      platform_name = sys.platform + "(Unknown)"
    raise RuntimeError, "Unknown tool (tool=%s, platform=%s)" % (tool_name,
                                                                 platform_name)

def CreateTool(tool):
  return ToolFactory().Create(tool)
