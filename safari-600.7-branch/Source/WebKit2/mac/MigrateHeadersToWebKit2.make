PRIVATE_HEADERS_DIR = $(TARGET_BUILD_DIR)/$(PRIVATE_HEADERS_FOLDER_PATH)

WEBKIT2_HEADERS = $(addprefix $(PRIVATE_HEADERS_DIR)/, $(notdir $(wildcard $(BUILT_PRODUCTS_DIR)/WebKit.framework/PrivateHeaders/*.h))) $(addprefix $(PRIVATE_HEADERS_DIR)/, $(filter-out WKPreferences.h WKError.h, $(notdir $(wildcard $(BUILT_PRODUCTS_DIR)/WebKit.framework/Headers/*.h))))

all : $(WEBKIT2_HEADERS)

WEBKIT2_HEADER_MIGRATE_CMD = echo "\#import <WebKit/"`basename $<`">" > $@

$(PRIVATE_HEADERS_DIR)/% : $(BUILT_PRODUCTS_DIR)/WebKit.framework/PrivateHeaders/% MigrateHeadersFromWebKitLegacy.make
	$(WEBKIT2_HEADER_MIGRATE_CMD)

$(PRIVATE_HEADERS_DIR)/% : $(BUILT_PRODUCTS_DIR)/WebKit.framework/Headers/% MigrateHeadersFromWebKitLegacy.make
	$(WEBKIT2_HEADER_MIGRATE_CMD)
