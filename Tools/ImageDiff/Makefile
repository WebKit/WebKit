SCRIPTS_PATH = ../Scripts

# Temporary until https://bugs.webkit.org/show_bug.cgi?id=168939 is completed
all:
	$(SCRIPTS_PATH)/build-imagediff $(OTHER_OPTIONS)

debug d development dev develop:
	$(SCRIPTS_PATH)/set-webkit-configuration --debug
	$(SCRIPTS_PATH)/build-imagediff $(OTHER_OPTIONS)

release r deployment dep deploy:
	$(SCRIPTS_PATH)/set-webkit-configuration --release
	$(SCRIPTS_PATH)/build-imagediff $(OTHER_OPTIONS)
clean:
	$(SCRIPTS_PATH)/build-imagediff --clean
