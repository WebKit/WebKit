all-am: previous-clean-timestamp
	pbxbuild -buildstyle $(BUILDSTYLE) OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS) 
clean-am:
	pbxbuild clean OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS) 
	rm -rf $(SYMROOTS)/WebKit.build
	rm -rf $(SYMROOTS)/WebKit.framework
	rm -rf $(SYMROOTS)/Safari.app/Frameworks/WebKit.framework

WebView.subproj/WebUserAgentSpoofTable.c: WebView.subproj/WebUserAgentSpoofTable.gperf Makefile.am previous-clean-timestamp
	gperf -CEot -L 'ANSI-C' -k '*' -N _web_findSpoofTableEntry $< > $@ 

BUILT_SOURCES = WebView.subproj/WebUserAgentSpoofTable.c
CLEANFILES = $(BUILT_SOURCES)

previous-clean-timestamp: force-clean-timestamp
	make clean && touch $@

doc: all
	../Tools/Scripts/make-docs $(SYMROOTS)/Documentation/WebKit $(SYMROOTS)/WebKit.framework/Headers
