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
# Helper functions for the WebKit build.

import commands
import glob
import os
import platform
import re
import shutil
import sys
import urllib
import urlparse

def get_output(command):
    """
    Windows-compatible function for getting output from a command.
    """
    if sys.platform.startswith('win'):
        f = os.popen(command)
        return f.read().strip()
    else:
        return commands.getoutput(command)
    
def get_excludes(root, patterns):
    """
    Get a list of exclude patterns going down several dirs. 
    TODO: Make this fully recursive.
    """
    excludes = []
    
    for pattern in patterns:
        subdir_pattern = os.sep + '*'
        for subdir in [subdir_pattern, subdir_pattern*2, subdir_pattern*3]:
            adir = root + subdir + os.sep + pattern
            files = glob.glob(adir)
            for afile in files:
                excludes.append(os.path.basename(afile))

    return excludes
    
def get_dirs_for_features(root, features, dirs):
    """
    Find which directories to include in the list of build dirs based upon the 
    enabled port(s) and features.
    """
    outdirs = dirs
    for adir in dirs:
        for feature in features:
            relpath = os.path.join(adir, feature)
            featuredir = os.path.join(root, relpath)
            if os.path.exists(featuredir) and not relpath in outdirs:
                outdirs.append(relpath)
            
    return outdirs

def download_if_newer(url, destdir):
    """
    Checks if the file on the server is newer than the one in the user's tree,
    and if so, downloads it. 
    
    Returns the filename of the downloaded file if downloaded, or None if 
    the existing file matches the one on the server.
    """
    obj = urlparse.urlparse(url)
    filename = os.path.basename(obj.path)
    destfile = os.path.join(destdir, filename)
    
    urlobj = urllib.urlopen(url)
    size = long(urlobj.info().getheader('Content-Length'))

    def download_callback(downloaded, block_size, total_size): 
        downloaded = block_size * downloaded
        if downloaded > total_size:
            downloaded = total_size
        sys.stdout.write('%s %d of %d bytes downloaded\r' % (filename, downloaded, total_size))
    
    # NB: We don't check modified time as Python doesn't yet handle timezone conversion
    # properly when converting strings to time objects.
    if not os.path.exists(destfile) or os.path.getsize(destfile) != size:
        urllib.urlretrieve(url, destfile, download_callback)
        print ''
        return destfile
    
    return None
    
def update_wx_deps(conf, wk_root, msvc_version='msvc2008'):
    """
    Download and update tools needed to build the wx port.
    """
    import Logs
    Logs.info('Ensuring wxWebKit dependencies are up-to-date.')
    
    wklibs_dir = os.path.join(wk_root, 'WebKitLibraries')
    waf = download_if_newer('http://wxwebkit.wxcommunity.com/downloads/deps/waf', os.path.join(wk_root, 'WebKitTools', 'wx'))
    if waf:
        # TODO: Make the build restart itself after an update.
        Logs.warn('Build system updated, please restart build.')
        sys.exit(1)

    # since this module is still experimental
    wxpy_dir = os.path.join(wk_root, 'WebKit', 'wx', 'bindings', 'python')
    swig_module = download_if_newer('http://wxwebkit.wxcommunity.com/downloads/deps/swig.py.txt', wxpy_dir)
    if swig_module:
        shutil.copy(os.path.join(wxpy_dir, 'swig.py.txt'), os.path.join(wxpy_dir, 'swig.py'))

    if sys.platform.startswith('win'):
        Logs.info('downloading deps package')
        archive = download_if_newer('http://wxwebkit.wxcommunity.com/downloads/deps/wxWebKitDeps-%s.zip' % msvc_version, wklibs_dir)
        if archive and os.path.exists(archive):
            os.system('unzip -o %s -d %s' % (archive, os.path.join(wklibs_dir, msvc_version)))
    
    elif sys.platform.startswith('darwin'):
        # export the right compiler for building the dependencies
        if platform.release().startswith('10'): # Snow Leopard
            os.environ['CC'] = conf.env['CC'][0]
            os.environ['CXX'] = conf.env['CXX'][0]
        os.system('%s/WebKitTools/wx/install-unix-extras' % wk_root)
        
def includeDirsForSources(sources):
    include_dirs = []
    for group in sources:
        for source in group:
            dirname = os.path.dirname(source)
            if not dirname in include_dirs:
                include_dirs.append(dirname)
            
    return include_dirs

def flattenSources(sources):
    flat_sources = []
    for group in sources:
        flat_sources.extend(group)
        
    return flat_sources

def git_branch_name():
    try:
        branches = commands.getoutput("git branch --no-color")
        match = re.search('^\* (.*)', branches, re.MULTILINE)
        if match:
            return ".%s" % match.group(1)
    except:
        pass

    return ""

def get_config(wk_root):
    config_file = os.path.join(wk_root, 'WebKitBuild', 'Configuration')
    config = 'Debug'

    if os.path.exists(config_file):
        config = open(config_file).read()

    return config

def svn_revision():
    if os.system("git-svn info") == 0:
        info = commands.getoutput("git-svn info ../..")
    else:
        info = commands.getoutput("svn info")
    
    for line in info.split("\n"):
        if line.startswith("Revision: "):
            return line.replace("Revision: ", "").strip()
    
    return ""
