/* Automatically generated from kjs_navigator.cpp using ../../../JavaScriptCore/kjs/create_hash_table. DO NOT EDIT ! */

namespace KJS {

const struct HashEntry NavigatorTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { "productSub", Navigator::ProductSub, DontDelete|ReadOnly, 0, 0 },
   { "product", Navigator::Product, DontDelete|ReadOnly, 0, 0 },
   { "plugins", Navigator::_Plugins, DontDelete|ReadOnly, 0, 0 },
   { "appName", Navigator::AppName, DontDelete|ReadOnly, 0, &NavigatorTableEntries[13] },
   { 0, 0, 0, 0, 0 },
   { "appCodeName", Navigator::AppCodeName, DontDelete|ReadOnly, 0, &NavigatorTableEntries[14] },
   { 0, 0, 0, 0, 0 },
   { "mimeTypes", Navigator::_MimeTypes, DontDelete|ReadOnly, 0, 0 },
   { "javaEnabled", Navigator::JavaEnabled, DontDelete|Function, 0, 0 },
   { "appVersion", Navigator::AppVersion, DontDelete|ReadOnly, 0, 0 },
   { "platform", Navigator::Platform, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "language", Navigator::Language, DontDelete|ReadOnly, 0, &NavigatorTableEntries[15] },
   { "userAgent", Navigator::UserAgent, DontDelete|ReadOnly, 0, 0 },
   { "vendor", Navigator::Vendor, DontDelete|ReadOnly, 0, &NavigatorTableEntries[16] },
   { "cookieEnabled", Navigator::CookieEnabled, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable NavigatorTable = { 2, 17, NavigatorTableEntries, 13 };

} // namespace
