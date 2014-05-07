SCRIPTS_PATH = ../Tools/Scripts

libs:
	perl $(SCRIPTS_PATH)/copy-webkitlibraries-to-product-directory --wksi --llvm

all: libs

debug d:
	$(SCRIPTS_PATH)/set-webkit-configuration --debug
	@$(MAKE) libs

release r:
	$(SCRIPTS_PATH)/set-webkit-configuration --release
	@$(MAKE) libs

