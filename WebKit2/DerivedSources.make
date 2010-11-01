VPATH = \
    $(WebKit2)/PluginProcess \
    $(WebKit2)/Shared/Plugins \
    $(WebKit2)/WebProcess/Plugins \
    $(WebKit2)/WebProcess/WebPage \
    $(WebKit2)/WebProcess \
    $(WebKit2)/UIProcess \
    $(WebKit2)/UIProcess/Plugins \
#

MESSAGE_RECEIVERS = \
    NPObjectMessageReceiver \
    PluginControllerProxy \
    PluginProcess \
    PluginProcessProxy \
    PluginProxy \
    WebPage \
    WebPageProxy \
    WebProcess \
    WebProcessConnection \
#

SCRIPTS = \
    $(WebKit2)/Scripts/generate-message-receiver.py \
    $(WebKit2)/Scripts/generate-messages-header.py \
    $(WebKit2)/Scripts/webkit2/__init__.py \
    $(WebKit2)/Scripts/webkit2/messages.py \
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
