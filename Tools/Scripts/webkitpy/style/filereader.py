# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2010 Chris Jerdonek (chris.jerdonek@gmail.com)
# Copyright (C) 2010 ProFUSION embedded systems
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

"""Supports reading and processing text files."""

import logging
import sys

from webkitpy.common.host import Host


_log = logging.getLogger(__name__)


class TextFileReader(object):

    """Supports reading and processing text files.

       Attributes:
         file_count: The total number of files passed to this instance
                     for processing, including non-text files and files
                     that should be skipped.
         delete_only_file_count: The total number of files that are not
                                 processed this instance actually because
                                 the files don't have any modified lines
                                 but should be treated as processed.

    """

    def __init__(self, filesystem, processor):
        """Create an instance.

        Arguments:
          processor: A ProcessorBase instance.

        """

        self.filesystem = filesystem
        self._processor = processor
        self._files = {}
        self.delete_only_file_count = 0

    @property
    def file_count(self):
        return len(self._files) - self.delete_only_file_count

    def _read_lines(self, file_path):
        """Read the file at a path, and return its lines.

        Raises:
          IOError: If the file does not exist or cannot be read.

        """
        # Support the UNIX convention of using "-" for stdin.
        if file_path == '-':
            file = self.filesystem.open_stdin()
        else:
            # We do not open the file with universal newline support
            # (codecs does not support it anyway), so the resulting
            # lines contain trailing "\r" characters if we are reading
            # a file with CRLF endings.
            file = self.filesystem.open_text_file_for_reading(file_path, 'replace')

        try:
            contents = file.read()
        finally:
            file.close()

        lines = contents.split('\n')
        return lines

    def process_file(self, file_path, **kwargs):
        """Process the given file by calling the processor's process() method.

        Args:
          file_path: The path of the file to process.
          **kwargs: Any additional keyword parameters that should be passed
                    to the processor's process() method.  The process()
                    method should support these keyword arguments.

        Raises:
          SystemExit: If no file at file_path exists.

        """
        abs_file_path = self.filesystem.abspath(file_path)
        if abs_file_path not in self._files:
            self._files[abs_file_path] = None
        if kwargs.get('line_numbers'):
            # Deleted files will be 'None', but if a file has modified lines, this information should override the 'None'
            if self._files[abs_file_path] is None:
                self._files[abs_file_path] = []
            self._files[abs_file_path] = self._files[abs_file_path] + kwargs['line_numbers']

        if not self.filesystem.exists(file_path) and file_path != "-":
            _log.error("File does not exist: '%s'" % file_path)
            raise IOError("File does not exist")

        if not self._processor.should_process(file_path):
            _log.debug("Skipping file: '%s'" % file_path)
            return
        _log.debug("Processing file: '%s'" % file_path)

        try:
            lines = self._read_lines(file_path)
        except IOError as err:
            message = ("Could not read file. Skipping: '%s'\n  %s" % (file_path, err))
            _log.warn(message)
            return

        self._processor.process(lines, file_path, **kwargs)

    def _process_directory(self, directory):
        """Process all files in the given directory, recursively."""
        for file_path in self.filesystem.files_under(directory):
            self.process_file(file_path)

    def process_paths(self, paths):
        for path in paths:
            if self.filesystem.isdir(path):
                self._process_directory(directory=path)
            else:
                self.process_file(path)

    def do_association_check(self, cwd, host=Host()):
        self._processor.do_association_check(self._files, cwd, host=host)

    def delete_file(self, file_path=None):
        """Keep track of deleted files.

        Files which has no modified or newly-added lines don't need
        to check style, but they may effect the association check.
        """
        if file_path:
            self._files[self.filesystem.abspath(file_path)] = None
        self.delete_only_file_count += 1
