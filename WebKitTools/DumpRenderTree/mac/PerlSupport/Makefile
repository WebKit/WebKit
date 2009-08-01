
CONFIGURATION_BUILD_DIR ?= .
OUTPUT_DIR=$(CONFIGURATION_BUILD_DIR)

WRAPPER=$(OUTPUT_DIR)/DerivedSources/DumpRenderTree/DumpRenderTreeSupport_wrap.c
PERL_MODULE=$(OUTPUT_DIR)/DumpRenderTreeSupport.pm
DYLIB=$(OUTPUT_DIR)/DumpRenderTreeSupport.dylib

all: $(DYLIB) $(PERL_MODULE)

clean:
	rm -f $(WRAPPER) $(PERL_MODULE) $(DYLIB)

$(WRAPPER) $(PERL_MODULE): DumpRenderTreeSupport.c
	mkdir -p $$(dirname $(WRAPPER))
	swig -o $(WRAPPER) -outdir $(OUTPUT_DIR) -perl -module DumpRenderTreeSupport $<

$(DYLIB): DumpRenderTreeSupport.c $(WRAPPER)
	gcc -g -shared -o $(DYLIB) `perl -MExtUtils::Embed -eperl_inc` `perl -MExtUtils::Embed -eldopts` $^
