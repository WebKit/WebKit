/*
 * Copyright (C) 2014 Ryuan Choi <ryuan.choi@gmail.com>.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file    EWebKit_Extension.h
 * @brief   Contains the header files that are required by ewebkit2-extension.
 *
 * EWebKit_Extension is the way to extend WebProcess of ewebkit2.
 *
 * In order to create the extension,
 * First, ewk_extension_init must be implemented in the shared object like below:
 *
 * @code
 * #include "EWebKit_Extension.h"
 *
 * void ewk_extension_init(Ewk_Extension *extension)
 * {
 *    // provide Ewk_Extension_Client callbacks as client.
 *    ewk_extension_client_add(extension, &client);
 * }
 * @endcode
 *
 * And compiles C or C++ files as shared object like below:
 *
 * @verbatim gcc -o libsample.so sample.c -shared -fPIC `pkg-config --cflags --libs eina ewebkit2-extension` @endverbatim
 *
 * Then, install that object into the path which ewk_context_new_with_extensions_path() specifies.
 *
 * @see ewk_context_new_with_extensions_path
 * @see ewk_extension_client_set
 * @see Ewk_Extension_Initialize_Function
 */

#ifndef EWebKit_Extension_h
#define EWebKit_Extension_h

#include "ewk_extension.h"

#endif // EWebKit_Extension_h
