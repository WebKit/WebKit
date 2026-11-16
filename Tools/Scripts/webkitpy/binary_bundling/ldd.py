# Copyright (C) 2018, 2020, 2021 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
from pathlib import PurePath
import os
import shutil
import subprocess

_log = logging.getLogger(__name__)


class SharedObjectResolver():
    def __init__(self, ldd, resolver_environment=None):
        self._ldd = ldd
        self._get_interpreter_method = None
        self._resolver_environment = resolver_environment
        binaries_for_implemented_get_interpreter_methods = ['patchelf', 'readelf']
        for required_binary in binaries_for_implemented_get_interpreter_methods:
            if shutil.which(required_binary):
                self._get_interpreter_method = required_binary
                break
        if self._get_interpreter_method is None:
            string_methods = f"`{'` or `'.join(binaries_for_implemented_get_interpreter_methods)}`"
            _log.error(f"Could not find any of the programs {string_methods} in $PATH")
            raise Exception(f"Missing required program. Need any of {string_methods}")

    def _run_cmd_and_get_output(self, command):
        _log.debug("EXEC %s" % command)
        command_process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding='utf-8', env=self._resolver_environment)
        stdout, stderr = command_process.communicate()
        return command_process.returncode, stdout, stderr

    def _get_patchelf_interpreter_objname(self, object):
        retcode, stdout, stderr = self._run_cmd_and_get_output(['patchelf', '--print-interpreter', object])
        if retcode != 0:
            _log.debug("patchelf stdout:\n%s\npatchelf stderr:\n%s" % (stdout, stderr))
            if 'cannot find section' in stdout:
                # This is fine; we only expect an interpreter in the main binary.
                return None
            raise RuntimeError(f'The patchelf command returned non-zero status for object {object}.\nstdout:{stdout}\n%s\nstderr:{stderr}\n')
        return stdout.strip()

    def _get_readelf_interpreter_objname(self, object):
        retcode, stdout, stderr = self._run_cmd_and_get_output(['readelf', '-l', object])
        if retcode != 0:
            _log.debug("readelf stdout:\n%s\nreadelf stderr:\n%s" % (stdout, stderr))
            raise RuntimeError(f'The readelf command returned non-zero status for object {object}.\nstdout:{stdout}\n%s\nstderr:{stderr}\n')
        found_interp_line = False
        for line in stdout.splitlines():
            if '.interp' in line:
                found_interp_line = True
            if 'Requesting program interpreter' in line:
                return line.split(':', 1)[1].removesuffix(']').strip()
        if found_interp_line:
            raise RuntimeError(f'readelf found a .interp line but not the interpreter path.\nstdout:{stdout}\n%s\nstderr:{stderr}\n')
        # This is fine, it didn't found an 'interp' section so there is no interpreter (is likely a library)
        return None

    def _get_interpreter_objname(self, object):
        # Note: we use patchelf/readelf to get the interpreter because
        # this works regardless of the architecture of the ELF file.
        if self._get_interpreter_method == 'patchelf':
            interpreter_path_str = self._get_patchelf_interpreter_objname(object)
        elif self._get_interpreter_method == 'readelf':
            interpreter_path_str = self._get_readelf_interpreter_objname(object)
        else:
            assert(False), 'Not reached'
        return PurePath(interpreter_path_str).name if interpreter_path_str else None

    def _get_libs_and_interpreter(self, object):
        interpreter = None
        retcode, stdout, stderr = self._run_cmd_and_get_output([self._ldd, object])
        _log.debug("ldd stdout:\n%s" % stdout)
        if retcode != 0:
            raise RuntimeError('The %s command returned non-zero status for object %s' % (self._ldd, object))
        libs = []
        for line in stdout.splitlines():
            line = line.strip()
            orig_line = line
            if '=>' in line:
                line = line.split('=>')[1].strip()
                if 'not found' in line:
                    raise RuntimeError(f'ldd can not resolve all dependencies for object "{object}". Error in line "{orig_line}".')
                line = line.split(' ')[0].strip()
                if os.path.isfile(line):
                    libs.append(line)
            else:
                line = line.split(' ')[0].strip()
                if os.path.isfile(line):
                    interpreter = line
        if interpreter is None:
            # This is the case for non-native binaries. For those, we
            # can use a cross-ldd (xldd), but then the interpreter
            # looks like any other shared object in the output of
            # ldd. Try to identify it by looking at the object name
            # from the interpreter string.
            interpreter_objname = self._get_interpreter_objname(object)
            for lib in libs:
                if PurePath(lib).name == interpreter_objname:
                    interpreter = lib
                    break
            # If we found an interpreter, remove it from the libs.
            libs = [lib for lib in libs if lib != interpreter]
        return libs, interpreter

    def _ldd_recursive_get_libs_and_interpreter(self, object, already_checked_libs=[]):
        libs, interpreter = self._get_libs_and_interpreter(object)
        if libs:
            for lib in libs:
                if lib in already_checked_libs:
                    continue
                # avoid recursion loops (libfreetype.so.6 <-> libharfbuzz.so.0)
                already_checked_libs.append(lib)
                sub_libs, sub_interpreter = self._ldd_recursive_get_libs_and_interpreter(lib, already_checked_libs)
                libs.extend(sub_libs)
                if sub_interpreter and interpreter and sub_interpreter != interpreter:
                    raise RuntimeError('library %s has interpreter %s but object %s has interpreter %s' % (lib, sub_interpreter, object, interpreter))
        return list(set(libs)), interpreter

    def get_libs_and_interpreter(self, object, already_checked_libs=[]):
        return self._ldd_recursive_get_libs_and_interpreter(object, already_checked_libs)
