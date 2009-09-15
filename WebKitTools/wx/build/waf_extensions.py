# Copyright (C) 2009 Kevin Ollivier  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
#
# This module is for code where we override waf's default behavior or extend waf

import os
import subprocess
import sys

import Utils

# version of exec_command that handles Windows command lines longer than 32000 chars
def exec_command(s, **kw):
    filename = ''
    if sys.platform.startswith('win') and len(' '.join(s)) > 32000:
        import tempfile
        (fd, filename) = tempfile.mkstemp()
        os.write(fd, ' '.join(s[1:]))
        os.close(fd)
        
        s = [s[0], '@' + filename]
        
    if 'log' in kw:
        kw['stdout'] = kw['stderr'] = kw['log']
        del(kw['log'])
    kw['shell'] = isinstance(s, str)

    def cleanup():
        try:
            if os.path.exists(filename):
                os.remove(filename)
        except:
            pass

    try:
        proc = subprocess.Popen(s, **kw)
        result = proc.wait()
        cleanup()
        return result
        
    except OSError:
        cleanup()
        raise
        
Utils.exec_command = exec_command

# Better performing h_file to keep hashing from consuming lots of time
import stat
def h_file(filename):
    st = os.stat(filename)
    if stat.S_ISDIR(st[stat.ST_MODE]): raise IOError('not a file')
    m = Utils.md5()
    m.update(str(st.st_mtime))
    m.update(str(st.st_size))
    m.update(filename)
    return m.digest()

Utils.h_file = h_file
