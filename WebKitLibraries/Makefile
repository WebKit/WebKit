SCRIPTS_PATH = ../Tools/Scripts

ifeq ($(shell sw_vers -productVersion | cut -d. -f-2), 10.10)
libs:
	@true
cleanlibs:
	@true
else
libs:
	perl $(SCRIPTS_PATH)/copy-webkitlibraries-to-product-directory --wksi
cleanlibs:
	perl $(SCRIPTS_PATH)/copy-webkitlibraries-to-product-directory --clean
endif

all: libs

debug d:
	$(SCRIPTS_PATH)/set-webkit-configuration --debug
	@$(MAKE) libs

release r:
	$(SCRIPTS_PATH)/set-webkit-configuration --release
	@$(MAKE) libs

release+assert ra:
	$(SCRIPTS_PATH)/set-webkit-configuration --release
	@$(MAKE) libs

testing t:
	$(SCRIPTS_PATH)/set-webkit-configuration --debug --force-optimization-level=O3
	@$(MAKE) libs

analyze:
	$(SCRIPTS_PATH)/set-webkit-configuration --debug
	@$(MAKE) libs

clean:
	@$(MAKE) cleanlibs
