include ../Makefile.shared

PDFJS_SRCROOT = $(SRCROOT)$(PATH_FROM_ROOT)/../ThirdParty/pdfjs
$(PDFJS_SRCROOT):
	mkdir -p $@
	cp -R ../ThirdParty/pdfjs/ $(PDFJS_SRCROOT)

installsrc: $(PDFJS_SRCROOT)
