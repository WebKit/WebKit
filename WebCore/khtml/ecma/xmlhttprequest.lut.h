/* Automatically generated from xmlhttprequest.cpp using ../../../JavaScriptCore/kjs/create_hash_table. DO NOT EDIT ! */

namespace KJS {

const struct HashEntry XMLHttpRequestProtoTableEntries[] = {
   { "open", XMLHttpRequest::Open, DontDelete|Function, 5, 0 },
   { 0, 0, 0, 0, 0 },
   { "getResponseHeader", XMLHttpRequest::GetResponseHeader, DontDelete|Function, 1, 0 },
   { "setRequestHeader", XMLHttpRequest::SetRequestHeader, DontDelete|Function, 2, 0 },
   { "abort", XMLHttpRequest::Abort, DontDelete|Function, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "getAllResponseHeaders", XMLHttpRequest::GetAllResponseHeaders, DontDelete|Function, 0, &XMLHttpRequestProtoTableEntries[7] },
   { "send", XMLHttpRequest::Send, DontDelete|Function, 1, 0 }
};

const struct HashTable XMLHttpRequestProtoTable = { 2, 8, XMLHttpRequestProtoTableEntries, 7 };

} // namespace

namespace KJS {

const struct HashEntry XMLHttpRequestTableEntries[] = {
   { "responseXML", XMLHttpRequest::ResponseXML, DontDelete|ReadOnly, 0, &XMLHttpRequestTableEntries[8] },
   { 0, 0, 0, 0, 0 },
   { "onreadystatechange", XMLHttpRequest::Onreadystatechange, DontDelete, 0, 0 },
   { "readyState", XMLHttpRequest::ReadyState, DontDelete|ReadOnly, 0, 0 },
   { "status", XMLHttpRequest::Status, DontDelete|ReadOnly, 0, 0 },
   { "responseText", XMLHttpRequest::ResponseText, DontDelete|ReadOnly, 0, &XMLHttpRequestTableEntries[7] },
   { 0, 0, 0, 0, 0 },
   { "statusText", XMLHttpRequest::StatusText, DontDelete|ReadOnly, 0, 0 },
   { "onload", XMLHttpRequest::Onload, DontDelete, 0, 0 }
};

const struct HashTable XMLHttpRequestTable = { 2, 9, XMLHttpRequestTableEntries, 7 };

} // namespace
