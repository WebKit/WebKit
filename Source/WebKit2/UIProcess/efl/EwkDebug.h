/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#ifndef EwkDebug_h
#define EwkDebug_h

#include "ewk_main_private.h"

#define CRITICAL(...) EINA_LOG_DOM_CRIT(EwkMain::singleton().logDomainId(), __VA_ARGS__)
#define ERR(...) EINA_LOG_DOM_ERR(EwkMain::singleton().logDomainId(), __VA_ARGS__)
#define WARN(...) EINA_LOG_DOM_WARN(EwkMain::singleton().logDomainId(), __VA_ARGS__)
#define INFO(...) EINA_LOG_DOM_INFO(EwkMain::singleton().logDomainId(), __VA_ARGS__)
#define DBG(...) EINA_LOG_DOM_DBG(EwkMain::singleton().logDomainId(), __VA_ARGS__)

#endif // EwkDebug_h
