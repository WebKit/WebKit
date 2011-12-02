# Copyright (C) 2010 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

VPATH = \
    $(WebKit2) \
    $(WebKit2)/PluginProcess \
    $(WebKit2)/Shared/Plugins \
    $(WebKit2)/WebProcess/ApplicationCache \
    $(WebKit2)/WebProcess/Authentication \
    $(WebKit2)/WebProcess/Cookies \
    $(WebKit2)/WebProcess/FullScreen \
    $(WebKit2)/WebProcess/Geolocation \
    $(WebKit2)/WebProcess/IconDatabase \
    $(WebKit2)/WebProcess/KeyValueStorage \
    $(WebKit2)/WebProcess/MediaCache \
    $(WebKit2)/WebProcess/Notifications \
    $(WebKit2)/WebProcess/Plugins \
    $(WebKit2)/WebProcess/ResourceCache \
    $(WebKit2)/WebProcess/WebCoreSupport \
    $(WebKit2)/WebProcess/WebPage \
    $(WebKit2)/WebProcess \
    $(WebKit2)/UIProcess \
    $(WebKit2)/UIProcess/Downloads \
    $(WebKit2)/UIProcess/Plugins \
#

MESSAGE_RECEIVERS = \
    AuthenticationManager \
    DrawingArea \
    DrawingAreaProxy \
    DownloadProxy \
    NPObjectMessageReceiver \
    PluginControllerProxy \
    PluginProcess \
    PluginProcessConnection \
    PluginProcessProxy \
    PluginProxy \
    WebApplicationCacheManager \
    WebApplicationCacheManagerProxy \
    WebCookieManager \
    WebCookieManagerProxy \
    WebContext \
    WebDatabaseManager \
    WebDatabaseManagerProxy \
    WebFullScreenManager \
    WebFullScreenManagerProxy \
    WebGeolocationManager \
    WebGeolocationManagerProxy \
    WebIconDatabase \
    WebIconDatabaseProxy \
    WebInspector \
    WebInspectorProxy \
    WebKeyValueStorageManager \
    WebKeyValueStorageManagerProxy \
    WebMediaCacheManager \
    WebMediaCacheManagerProxy \
    WebNotificationManagerProxy \
    WebNotificationManager \
    WebPage \
    WebPageProxy \
    WebProcess \
    WebProcessConnection \
    WebProcessProxy \
    WebResourceCacheManager \
    WebResourceCacheManagerProxy \
#

SCRIPTS = \
    $(WebKit2)/Scripts/generate-message-receiver.py \
    $(WebKit2)/Scripts/generate-messages-header.py \
    $(WebKit2)/Scripts/webkit2/__init__.py \
    $(WebKit2)/Scripts/webkit2/messages.py \
    $(WebKit2)/Scripts/webkit2/model.py \
    $(WebKit2)/Scripts/webkit2/parser.py \
#

.PHONY : all

all : \
    $(MESSAGE_RECEIVERS:%=%MessageReceiver.cpp) \
    $(MESSAGE_RECEIVERS:%=%Messages.h) \
#

%MessageReceiver.cpp : %.messages.in $(SCRIPTS)
	@echo Generating messages header for $*...
	@python $(WebKit2)/Scripts/generate-message-receiver.py $< > $@

%Messages.h : %.messages.in $(SCRIPTS)
	@echo Generating message receiver for $*...
	@python $(WebKit2)/Scripts/generate-messages-header.py $< > $@

# ------------------------

# Windows-specific rules

ifeq ($(OS),Windows_NT)

all : HeaderDetection.h

HeaderDetection.h : DerivedSources.make
	if [ -f "$(WEBKITLIBRARIESDIR)/include/WebKitQuartzCoreAdditions/WebKitQuartzCoreAdditionsBase.h" ] && [ ! -f "$(WEBKITLIBRARIESDIR)/include/cairo/cairo.h" ]; then echo "#define HAVE_WKQCA 1" > $@; else echo > $@; fi

endif # Windows_NT
