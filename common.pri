# common project include file for JavaScriptCore and WebCore

contains(JAVASCRIPTCORE_JIT,yes): DEFINES+=ENABLE_JIT=1
contains(JAVASCRIPTCORE_JIT,no): DEFINES+=ENABLE_JIT=0

