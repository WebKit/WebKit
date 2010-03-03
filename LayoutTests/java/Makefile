# Copyright (C) 2010 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS “AS IS”
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

ifeq ($(findstring 10.4, $(shell sw_vers -productVersion)), 10.4)
	USE_NORMAL_PLUGIN_JAR_PATH = 1
endif

ifeq ($(findstring 10.5, $(shell sw_vers -productVersion)), 10.5)
	USE_NORMAL_PLUGIN_JAR_PATH = 1
endif

JC = javac
ifeq ($(USE_NORMAL_PLUGIN_JAR_PATH), 1)
	CLASSPATH = /System/Library/Frameworks/JavaVM.framework/Home/lib/plugin.jar
else
	# The above path works on Tiger and Leopard, but is a broken link on SnowLeopard, <rdar://7578937>.
    CLASSPATH = /System/Library/Frameworks/JavaVM.framework/Versions/A/Resources/Deploy.bundle/Contents/Resources/Java/plugin.jar
endif
JFLAGS = -classpath $(CLASSPATH):jstests.jar
.SUFFIXES: .java .class

.java.class:
	$(JC) $(JFLAGS) $*.java

CLASSES = $(wildcard *.java)

default: classes

classes: $(CLASSES:.java=.class)

clean:
	$(RM) *.class
