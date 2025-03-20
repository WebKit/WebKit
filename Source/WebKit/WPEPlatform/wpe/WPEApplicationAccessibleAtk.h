/*
 * Copyright (C) 2025 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(ATK)
#include "WPEToplevel.h"
#include <atk/atk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define WPE_TYPE_APPLICATION_ACCESSIBLE_ATK (wpe_application_accessible_atk_get_type())
G_DECLARE_FINAL_TYPE (WPEApplicationAccessibleAtk, wpe_application_accessible_atk, WPE, APPLICATION_ACCESSIBLE_ATK, AtkObject)

AtkObject* wpeApplicationAccessibleAtkNew();
void wpeApplicationAccessibleAtkToplevelCreated(WPEApplicationAccessibleAtk*, WPEToplevel*);
void wpeApplicationAccessibleAtkToplevelDestroyed(WPEApplicationAccessibleAtk*, WPEToplevel*);
int wpeApplicationAccessibleAtkGetToplevelIndex(WPEApplicationAccessibleAtk*, WPEToplevel*);

G_END_DECLS

#endif // USE(ATK)
