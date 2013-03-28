# This is a wrapper makefile for the gyp-generated one which supports rerunning gyp when the input changes.
include Makefile

Makefile: $(srcdir)/Source/WTF/WTF.gypi \
		  $(srcdir)/Source/WebCore/WebCore.gyp/WebCoreGTK.gyp \
		  $(srcdir)/Source/WebCore/WebCore.gyp/ConvertFileToHeaderWithCharacterArray.gypi \
		  $(srcdir)/Source/WebCore/WebCore.gyp/MakeNames.gypi \
		  $(srcdir)/Source/ThirdParty/ANGLE/ANGLE.gyp/ANGLE.gyp \
		  $(srcdir)/Source/JavaScriptCore/JavaScriptCore.gypi \
		  $(srcdir)/Source/JavaScriptCore/JavaScriptCore.gyp/JavaScriptCoreGTK.gyp \
		  $(srcdir)/Source/WebCore/WebCore.gypi \
		  $(srcdir)/Source/WTF/WTF.gyp/WTFGTK.gyp \
		  $(srcdir)/Source/WebKit/gtk/gyp/Dependencies.gyp \
		  $(srcdir)/Source/ThirdParty/ANGLE/ANGLE.gypi \
		  $(srcdir)/build/Configuration.gypi
	@echo "Rerunning gyp..."; ./run-gyp
