##
## Copyright 2009, The Android Open Source Project
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##  * Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
##  * Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
## EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
## PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
## CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
## EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
## PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
## PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
## OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##

# wtf source files

LOCAL_SRC_FILES := \
	pcre/pcre_compile.cpp \
	pcre/pcre_exec.cpp \
	pcre/pcre_tables.cpp \
	pcre/pcre_ucp_searchfuncs.cpp \
	pcre/pcre_xclass.cpp \
	\
	wtf/Assertions.cpp \
	wtf/ByteArray.cpp \
	wtf/CurrentTime.cpp \
	wtf/DateMath.cpp \
	wtf/FastMalloc.cpp \
	wtf/HashTable.cpp \
	wtf/MainThread.cpp \
	wtf/RandomNumber.cpp \
	wtf/RefCountedLeakCounter.cpp \
	wtf/TCSystemAlloc.cpp \
	wtf/ThreadIdentifierDataPthreads.cpp \
	wtf/Threading.cpp \
	wtf/ThreadingPthreads.cpp \
	wtf/WTFThreadData.cpp \
	\
	wtf/TypeTraits.cpp \
	wtf/dtoa.cpp \
	\
	wtf/android/MainThreadAndroid.cpp \
	\
	wtf/text/AtomicString.cpp \
	wtf/text/CString.cpp \
	wtf/text/StringImpl.cpp \
	wtf/text/WTFString.cpp \
	\
	wtf/unicode/CollatorDefault.cpp \
	wtf/unicode/UTF8.cpp \
	\
	wtf/unicode/icu/CollatorICU.cpp

CHARTABLES := $(intermediates)/chartables.c
$(CHARTABLES): PRIVATE_PATH := $(LOCAL_PATH)
$(CHARTABLES): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/pcre/dftables $@
$(CHARTABLES): $(LOCAL_PATH)/pcre/dftables
$(CHARTABLES): $(LOCAL_PATH)/pcre/pcre_internal.h
	$(transform-generated-source)

$(intermediates)/pcre/pcre_tables.o : $(CHARTABLES)

# We do not add $(CHARTABLES) to LOCAL_GENERATED_SOURCES because the chartables.c file
# is explicitly #included in pcre_tables.cpp.
