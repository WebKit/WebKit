all-am: previous-clean-timestamp
	pbxbuild -buildstyle $(BUILDSTYLE)
clean-am:
	pbxbuild clean

previous-clean-timestamp: force-clean-timestamp
	make clean && touch $@
