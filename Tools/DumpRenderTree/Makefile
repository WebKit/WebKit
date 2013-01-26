SCRIPTS_PATH = ../Scripts

ifneq (,$(findstring iphoneos,$(SDK)))
	OTHER_OPTIONS += -target All-iOS
else ifneq (,$(findstring iphonesimulator,$(SDK)))
	OTHER_OPTIONS += -target All-iOS
endif

include ../../Makefile.shared
