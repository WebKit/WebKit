#!/usr/bin/python

# Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# A script to make sure the source file list for the Bakefiles is up-to-date
# with the MSVC project files. 

import sys, os
from xml.dom import minidom

WebKitRoot = ".."


class MSVS8Compiler:
    def __init__(self):
        self.precomp_headers = True
        self.warning_level = "default"
        self.defines = []
        self.includes = []

    def fromXML(self, tool):
        if tool.attributes.has_key("AdditionalIncludeDirectories"):
            includes_string = tool.attributes["AdditionalIncludeDirectories"].value
            includes_string = includes_string.replace("&quot;", '"')
            includes_string = includes_string.replace("$", "$(DOLLAR)")
            self.includes = includes_string.split(";")

        if tool.attributes.has_key("PreprocessorDefinitions"):
            self.defines = tool.attributes["PreprocessorDefinitions"].value.split(";")

class MSVS8Config:
    def __init__(self):
        self.target_type="exe"
        self.target_name="Release"
        self.output_dir = ""
        self.build_dir = ""
        self.pre_build_step = ""
        self.compiler = MSVS8Compiler()

    def fromXML(self, config):
        if config.attributes.has_key("Name"):
            self.target_name = config.attributes["Name"].value

        config_type = config.attributes["ConfigurationType"].value
        if config_type == "1":
            self.target_type = "exe"
        elif config_type == "2":
            self.target_type = "dll"
        elif config_type == "4":
            self.target_type = "lib"
        else:
            print "Unknown project type %s. Exiting..." % (config_type)
            sys.exit(1)

        tools = config.getElementsByTagName("Tool")
        
        for tool in tools: 
           if tool.attributes.has_key("Name") and tool.attributes["Name"].value == "VCPreBuildEventTool" and tool.attributes.has_key("VCPreBuildEventTool"):
               self.pre_build_step = tool.attributes["VCPreBuildEventTool"].value
               continue

           if tool.attributes.has_key("Name") and tool.attributes["Name"].value == "VCCLCompilerTool":
               self.compiler.fromXML(tool)
                                
    def asBkl(self, doc):
        target = doc.createElement(self.target_type)
        target.setAttribute("id", self.target_name)

        return target

class MSVS8Filter:
    def __init__(self):
        self.files = []
        self.name = ""
        self.varname = ""
        self.prefix = "WEBCORE_"

    def fromXML(self, filter):
        if filter.attributes.has_key("Name"):
            self.name = filter.attributes["Name"].value
            self.varname = self.prefix + "SOURCES_" + self.name.upper()

        for node in filter.childNodes:
            if node.nodeName == "File" and node.attributes.has_key("RelativePath"):
                filename = node.attributes["RelativePath"].value.replace("$", "$(DOLLAR)")
                filename = filename.replace("\\", "/")
                filename = "\t\t" + filename.replace("../../", "")
                if os.path.splitext(filename)[1] in [".c", ".cpp"]:
                    self.files.append(filename)

    def asBkl(self, doc):
        sources = doc.createElement("set")
        if self.name != "":
            sources.setAttribute("var", self.varname)
            # currently we 'flatten' the MSVC sources hierarchy to a simple list
            # so we may end up with duplicates for self.varname when the root
            # and subfolders share the same name. For now, just make sure the 
            # sources are added together as part of the target
            sources.setAttribute("append", "1")

        sources_text = "\n"
        for afile in self.files:
            sources_text += afile + "\n"

        sources.appendChild(doc.createTextNode(sources_text))
        return sources

class MSVS8Project:
    def __init__(self):
        self.configs = []
        self.file_list = []
        self.prefix = "WEBCORE_"

    def loadFromXML(self, filename):
        doc = minidom.parse(filename)
        configs = doc.getElementsByTagName("Configuration")
        for config in configs:
            config_obj = MSVS8Config()
            config_obj.fromXML(config)
            self.configs.append(config_obj)

        if filename.find("JavaScriptCore") != -1:
            self.prefix = "JSCORE_"

        files = doc.getElementsByTagName("Filter")
        for node in files:
            files = MSVS8Filter()
            files.prefix = self.prefix
            files.fromXML(node)
            self.file_list.append(files)

    def saveAsBkl(self, filename):
        doc = minidom.Document()
        makefile = doc.createElement("makefile")
        source_tags = []
        for files in self.file_list:
            makefile.appendChild(files.asBkl(doc))

        doc.appendChild(makefile)

        outfile = open(filename, "w")
        outfile.write(doc.toprettyxml())
        outfile.close()
        
jsdir = os.path.join(WebKitRoot, "JavaScriptCore")
wcdir = os.path.join(WebKitRoot, "WebCore")

files = { jsdir: os.path.join(jsdir, "JavaScriptCore.vcproj", "JavaScriptCore", "JavaScriptCore.vcproj"),
          wcdir: os.path.join(wcdir, "WebCore.vcproj", "WebCore", "WebCore.vcproj")
        }

for adir in files:
    project = MSVS8Project()
    project.loadFromXML(files[adir])
    outputfile = os.path.join(adir, os.path.splitext(os.path.basename(files[adir]))[0] + "Sources.bkl")
    project.saveAsBkl(outputfile)
