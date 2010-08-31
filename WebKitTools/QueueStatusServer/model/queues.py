# Copyright (C) 2010 Google Inc. All rights reserved.
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

import re


queues = [
    "commit-queue",
    "style-queue",
    "chromium-ews",
    "qt-ews",
    "gtk-ews",
    "mac-ews",
    "win-ews",
]


# FIXME: We need some sort of Queue object.
def _title_case(string):
    words = string.split(" ")
    words = map(lambda word: word.capitalize(), words)
    return " ".join(words)


def display_name_for_queue(queue_name):
    # HACK: chromium-ews is incorrectly named.
    display_name = queue_name.replace("chromium-ews", "cr-linux-ews")

    display_name = display_name.replace("-", " ")
    display_name = display_name.replace("cr", "chromium")
    display_name = _title_case(display_name)
    display_name = display_name.replace("Ews", "EWS")
    return display_name


def name_with_underscores(dashed_name):
    regexp = re.compile("-")
    return regexp.sub("_", dashed_name)
