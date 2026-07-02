/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionRegisteredScriptsSQLiteStore.h"

#import "CocoaHelpers.h"
#import "WKNSData.h"
#import "WebExtensionSQLiteHelpers.h"
#import <wtf/JSONValues.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

void WebExtensionRegisteredScriptsSQLiteStore::migrateData()
{
    static NSSet *allowedClasses = [NSSet setWithObjects:NSDictionary.class, NSNumber.class, NSString.class, NSArray.class, nil];

    assertIsCurrent(queue());

    String errorMessage;
    if (!openDatabaseIfNecessary(errorMessage, false))
        return;

    ASSERT(errorMessage.isEmpty());

    if (RefPtr rows = SQLiteDatabaseFetch(*database(), "SELECT * FROM registered_scripts"_s)) {
        RefPtr row = rows->next();
        while (row != nullptr) {
            NSError *error;
            String scriptID = row->getString(0);
            ASSERT(!scriptID.isEmpty());

            NSDictionary<NSString *, id> *script = [NSKeyedUnarchiver unarchivedObjectOfClasses:allowedClasses fromData:wrapper(row->getData(1)).get() error:&error];

            if (error) {
                RELEASE_LOG_ERROR(Extensions, "Failed to deserialize registered content scripts for extension %s", uniqueIdentifier().utf8().data());
                continue;
            }

            // Convert the NSDictionary to a JSON Object, which can be used to update the database to a new format
            // All original data should be a String, Number, Array, or Dictionary, and those are all
            // serializable to JSON
            String jsonString = encodeJSONString(script);
            DatabaseResult result = SQLiteDatabaseExecute(*database(), "UPDATE registered_scripts SET script = ? WHERE key = ?"_s, jsonString, scriptID);
            if (result != SQLITE_DONE)
                RELEASE_LOG_ERROR(Extensions, "Failed to update registered content script for extension %s.", uniqueIdentifier().utf8().data());

            row = rows->next();
        }
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
