# Copyright (C) 2012 Balazs Ankes (bank@inf.u-szeged.hu) University of Szeged
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
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

from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.common import checksvnconfigfile
from webkitpy.common.system.deprecated_logging import log
from webkitpy.common.checkout.scm.detection import SCMDetector
from webkitpy.common.system.systemhost import SystemHost


class AddSvnMimetypeForPng(AbstractStep):
    def __init__(self, tool, options, host=None, scm=None):
        self._tool = tool
        self._options = options
        self._host = host or SystemHost()
        self._fs = self._host.filesystem
        self._detector = scm or SCMDetector(self._fs, self._host.executive).detect_scm_system(self._fs.getcwd())

    def run(self, state):
        png_files = self._check_pngs(self._changed_files(state))

        if len(png_files) > 0:
            detection = self._detector.display_name()

            if detection == "git":
                (file_read, autoprop, png) = checksvnconfigfile.check(self, self._host, self._fs)
                config_file_path = checksvnconfigfile._config_file_path(self, self._host, self._fs)

                if file_read:
                    log("There is no SVN config file. The svn:mime-type of pngs won't set.")
                    if not self._tool.user.confirm("Are you sure you want to continue?", default="n"):
                        self._exit(1)
                elif autoprop and png:
                    log(checksvnconfigfile.errorstr_autoprop(config_file_path) + checksvnconfigfile.errorstr_png(config_file_path))
                    if not self._tool.user.confirm("Do you want to continue?", default="n"):
                        self._exit(1)
                elif autoprop:
                    log(checksvnconfigfile.errorstr_autoprop(config_file_path))
                    if not self._tool.user.confirm("Do you want to continue?", default="n"):
                        self._exit(1)
                elif png:
                    log(checksvnconfigfile.errorstr_png(config_file_path))
                    if not self._tool.user.confirm("Do you want to continue?", default="n"):
                        self._exit(1)

            elif detection == "svn":
                for filename in png_files:
                    if filename.endswith('.png') and detector.exists(filename) and detector.propget('svn:mime-type', filename) == "":
                        print "Adding image/png mime-type to %s" % filename
                        detector.propset('svn:mime-type', 'image/png', filename)

    def _check_pngs(self, changed_files):
        png_files = []
        for filename in changed_files:
            if filename.endswith('.png'):
                png_files.append(filename)
        return png_files
