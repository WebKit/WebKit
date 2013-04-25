/*
 * Copyright (C) 2013 Cisco Systems, Inc. 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Red istributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND ITS CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "UnitTestUtils/EWKTestBase.h"
#include "UnitTestUtils/EWKTestConfig.h"
#include <EWebKit.h>

using namespace EWKUnitTests;

/**
 * @brief Unit test for checking set/get of css media type by ewk settings API.
 */
TEST_F(EWKTestBase, ewk_settings_css_media_type)
{
    ASSERT_STREQ(ewk_settings_css_media_type_get(), 0);

    ewk_settings_css_media_type_set("handheld");
    ASSERT_STREQ(ewk_settings_css_media_type_get(), "handheld");

    ewk_settings_css_media_type_set("tv");
    ASSERT_STREQ(ewk_settings_css_media_type_get(), "tv");

    ewk_settings_css_media_type_set("screen");
    ASSERT_STREQ(ewk_settings_css_media_type_get(), "screen");

    ewk_settings_css_media_type_set(0);
    ASSERT_STREQ(ewk_settings_css_media_type_get(), 0);
}
