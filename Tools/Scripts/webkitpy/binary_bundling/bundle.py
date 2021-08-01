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
import os
import shutil
import subprocess

_log = logging.getLogger(__name__)


class BinaryBundler:
    VAR_MYDIR = 'MYDIR'
    VAR_INTERPRETER = 'INTERPRETER'

    def __init__(self, destination_dir, should_strip_objects):
        if shutil.which('patchelf') is None:
            _log.error("Could not find `patchelf` in $PATH")
            raise Exception("Missing required binary `patchelf`")
        self._destination_dir = destination_dir
        self._should_strip_objects = should_strip_objects

    def destination_dir(self):
        return self._destination_dir

    def copy_and_remove_rpath(self, orig_file, type='bin', destination_dir=None):
        dir_suffix = 'lib' if type == 'interpreter' else type
        destination_dir = os.path.join(self._destination_dir, dir_suffix)
        if not os.path.isdir(destination_dir):
            os.makedirs(destination_dir)

        if not os.path.isfile(orig_file):
            raise ValueError('Can not find file %s' % orig_file)

        _log.info('Add to bundle [%s]: %s' % (type, orig_file))
        try:
            shutil.copy(orig_file, destination_dir)
        except shutil.SameFileError:
            # May reasonably happen if the caller tries to bundle the files 'in place'.
            pass

        patch_elf_command = ['patchelf', '--remove-rpath', os.path.join(destination_dir, os.path.basename(orig_file))]
        if subprocess.call(patch_elf_command) != 0:
            _log.error('The patchelf command returned non-zero status')

        if self._should_strip_objects:
            if shutil.which('strip'):
                strip_command = ['strip', '--strip-unneeded', os.path.join(destination_dir, os.path.basename(orig_file))]
                if subprocess.call(strip_command) != 0:
                    _log.error('The strip command returned non-zero status')
            else:
                _log.warning('strip not found. Not stripping object')

    def generate_wrapper_script(self, interpreter, binary_to_wrap, extra_environment_variables={}):
        if not os.path.isfile(os.path.join(self._destination_dir, 'bin', binary_to_wrap)):
            raise RuntimeError('Cannot find binary to wrap for %s' % binary_to_wrap)
        _log.info('Generate wrapper script %s' % binary_to_wrap)
        script_file = os.path.join(self._destination_dir, binary_to_wrap)

        with open(script_file, 'w') as script_handle:
            script_handle.write('#!/bin/sh\n')
            script_handle.write('%s="$(dirname $(readlink -f $0))"\n' % self.VAR_MYDIR)
            script_handle.write('export LD_LIBRARY_PATH="${%s}/lib"\n' % self.VAR_MYDIR)

            if interpreter is not None:
                script_handle.write('INTERPRETER="${%s}/lib/%s"\n' % (self.VAR_MYDIR, os.path.basename(interpreter)))

            # May use the value of INTERPRETER, so has to come after
            # the definition.
            for var, value in extra_environment_variables.items():
                script_handle.write('export %s=\"%s\"\n' % (var, value))

            if interpreter is not None:
                script_handle.write('exec "${INTERPRETER}" "${%s}/bin/%s" "$@"\n' % (self.VAR_MYDIR, binary_to_wrap))
            else:
                script_handle.write('exec "${%s}/bin/%s" "$@"\n' % (self.VAR_MYDIR, binary_to_wrap))

        os.chmod(script_file, 0o755)
