#!/usr/bin/env python

# Copyright (C) 2007 Kevin Ollivier  All rights reserved.
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
# Make sure any port-independent files added to the Bakefile are
# added to GTK, QT, etc. so that file updates can happen in one place.

import os, sys
from xml.dom import minidom

scriptDir = os.path.abspath(sys.path[0])
wkroot = os.path.abspath(os.path.join(scriptDir, "../.."))

def getWebCoreFilesDict():
    """
    This method parses the WebCoreSources.bkl file, which has a list of all sources not specific
    to any port, and returns the result as a dictionary with items of the form
    (groupName, groupFiles). 
    """
    sources = {}
    sources_prefix = "WEBCORE_"
    filepath = os.path.join(wkroot, "WebCore/WebCoreSources.bkl")
    assert(os.path.exists(filepath))
    
    doc = minidom.parse(filepath)
    for sourceGroup in doc.getElementsByTagName("set"):
        groupName = ""
        if sourceGroup.attributes.has_key("var"):
            groupName = sourceGroup.attributes["var"].value
            groupName = groupName.replace(sources_prefix, "")
            
        sourcesList = []
        for node in sourceGroup.childNodes:
            if node.nodeType == node.TEXT_NODE:
                sourcesText = node.nodeValue.strip()
                sourcesList = sourcesText.split("\n")
                
        assert(groupName != "")
        assert(sourcesList != [])
        
        sources[groupName] = sourcesList
        
    return sources

def generateWebCoreSourcesGTKAndQT(sources):
    """
    Convert the dictionary obtained from getWebCoreFilesDict() into a Unix makefile syntax,
    which IIUC is suitable for both GTK and QT build systems. To take advantage of this,
    QT and GTK would have to include the file "WebCore/sources.inc" into their makefiles.
    """
    makefileString = ""
    
    for key in sources.keys():
        makefileString += key + "+="
        for source in sources[key]:
            makefileString += " \\\n\t\t" + source.strip()
            
        makefileString += "\n\n"
    
    makefileString += "BASE_SOURCES +="
    for key in sources.keys():
        makefileString += " \\\n\t\t" + key
    
    outfile = os.path.join(wkroot, "WebCore/sources.inc")
    sourcefile = open(outfile, "w")
    sourcefile.write(makefileString)
    sourcefile.close()
    
sources = getWebCoreFilesDict()
generateWebCoreSourcesGTKAndQT(sources)

# Coming soon - MSVC and hopefully XCode support!
