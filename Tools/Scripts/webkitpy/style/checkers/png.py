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


"""Supports checking WebKit style in png files."""

import os
import re

from webkitpy.common.system.systemhost import SystemHost
from webkitpy.common.checkout.scm.detection import SCMDetector


class PNGChecker(object):
    """Check svn:mime-type for checking style"""

    categories = set(['image/png'])

    def __init__(self, file_path, handle_style_error, scm=None, host=None):
        self._file_path = file_path
        self._handle_style_error = handle_style_error
        self._host = host or SystemHost()
        self._fs = self._host.filesystem
        self._detector = scm or SCMDetector(self._fs, self._host.executive).detect_scm_system(self._fs.getcwd())

    def check(self, inline=None):
        errorstr = ""
        config_file_path = ""
        detection = self._detector.display_name()

        if detection == "git":
            config_file_path = self._config_file_path()
            there_is_enable_line = False
            there_is_png_line = False

            try:
                config_file = self._fs.read_text_file(config_file_path)
            except IOError:
                errorstr = "There is no " + config_file_path
                self._handle_style_error(0, 'image/png', 5, errorstr)
                return

            errorstr_autoprop = "Have to enable auto props in the subversion config file (" + config_file_path + " \"enable-auto-props = yes\"). "
            errorstr_png = "Have to set the svn:mime-type in the subversion config file (" + config_file_path + " \"*.png = svn:mime-type=image/png\")."

            for line in config_file.split('\n'):
                if not there_is_enable_line:
                    match = re.search("^\s*enable-auto-props\s*=\s*yes", line)
                    if match:
                        there_is_enable_line = True
                        errorstr_autoprop = ""
                        continue

                if not there_is_png_line:
                    match = re.search("^\s*\*\.png\s*=\s*svn:mime-type=image/png", line)
                    if match:
                        there_is_png_line = True
                        errorstr_png = ""
                        continue

            errorstr = errorstr_autoprop + errorstr_png
            if errorstr:
                self._handle_style_error(0, 'image/png', 5, errorstr)

        elif detection == "svn":
            prop_get = self._detector.propget("svn:mime-type", self._file_path)
            if prop_get != "image/png":
                errorstr = "Set the svn:mime-type property (svn propset svn:mime-type image/png " + self._file_path + ")."
                self._handle_style_error(0, 'image/png', 5, errorstr)

    def _config_file_path(self):
        config_file = ""
        if self._host.platform.is_win():
            config_file_path = self._fs.join(os.environ['APPDATA'], "Subversion\config")
        else:
            config_file_path = self._fs.join(self._fs.expanduser("~"), ".subversion/config")
        return config_file_path
