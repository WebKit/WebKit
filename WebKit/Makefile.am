all-am: previous-clean-timestamp
	pbxbuild -buildstyle $(BUILDSTYLE)
clean-am:
	pbxbuild clean

previous-clean-timestamp: force-clean-timestamp
	pbxbuild clean && touch $@

FRAMEWORK_NAME = WebKit
include $(top_srcdir)/embed.am
