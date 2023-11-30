/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "CoreIPCLocale.h"

#import <wtf/HashMap.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/StringHash.h>

namespace WebKit {

bool CoreIPCLocale::isValidIdentifier(const String& identifier)
{
    if ([[NSLocale availableLocaleIdentifiers] containsObject:identifier])
        return true;
    if (canonicalLocaleStringReplacement(identifier))
        return true;
    return false;
}

CoreIPCLocale::CoreIPCLocale(NSLocale *locale)
    : m_identifier([locale localeIdentifier])
{
}

CoreIPCLocale::CoreIPCLocale(String&& identifier)
    : m_identifier([[NSLocale currentLocale] localeIdentifier])
{
    if ([[NSLocale availableLocaleIdentifiers] containsObject:identifier])
        m_identifier = identifier;
    else if (auto fixedLocale = canonicalLocaleStringReplacement(identifier))
        m_identifier = *fixedLocale;
}

RetainPtr<id> CoreIPCLocale::toID() const
{
    return [[NSLocale alloc] initWithLocaleIdentifier:(NSString *)m_identifier];
}

std::optional<String> CoreIPCLocale::canonicalLocaleStringReplacement(const String& identifier)
{
    /*
        To ensure that all possible valid locale strings are handled, this list provides
        a lookup from what was returned from -localeIdentifier to what needs to go into
        -initWithLocaleIdentifier to ensure what was encoded can be decoded on the other side.

        See rdar://118571809.

        The test in IPCSerialization.mm will fail if something changes with the desired behavior.

        This lookup was generated with the following code and then sorted:
            for (NSString* input in [NSLocale availableLocaleIdentifiers]) {
                NSString* output = [[NSLocale localeWithLocaleIdentifier: input ] localeIdentifier];
                if(![output isEqualToString:input])
                    NSLog(@"{ \"%@\"_s, \"%@\"_s },", output, input);
            }
     */
    static NeverDestroyed<HashMap<String, String>> replacements = HashMap<String, String> {
        { "az"_s, "az_Latn"_s },
        { "az-Cyrl"_s, "az_Cyrl"_s },
        { "az-Cyrl_AZ"_s, "az_Cyrl_AZ"_s },
        { "az_AZ"_s, "az_Latn_AZ"_s },
        { "ber-Latn"_s, "ber_Latn"_s },
        { "ber-Latn_MA"_s, "ber_Latn_MA"_s },
        { "ber-Tfng"_s, "ber_Tfng"_s },
        { "ber-Tfng_MA"_s, "ber_Tfng_MA"_s },
        { "bs"_s, "bs_Latn"_s },
        { "bs-Cyrl"_s, "bs_Cyrl"_s },
        { "bs-Cyrl_BA"_s, "bs_Cyrl_BA"_s },
        { "bs_BA"_s, "bs_Latn_BA"_s },
        { "ff"_s, "ff_Latn"_s },
        { "ff-Adlm"_s, "ff_Adlm"_s },
        { "ff-Adlm_BF"_s, "ff_Adlm_BF"_s },
        { "ff-Adlm_CM"_s, "ff_Adlm_CM"_s },
        { "ff-Adlm_GH"_s, "ff_Adlm_GH"_s },
        { "ff-Adlm_GM"_s, "ff_Adlm_GM"_s },
        { "ff-Adlm_GN"_s, "ff_Adlm_GN"_s },
        { "ff-Adlm_GW"_s, "ff_Adlm_GW"_s },
        { "ff-Adlm_LR"_s, "ff_Adlm_LR"_s },
        { "ff-Adlm_MR"_s, "ff_Adlm_MR"_s },
        { "ff-Adlm_NE"_s, "ff_Adlm_NE"_s },
        { "ff-Adlm_NG"_s, "ff_Adlm_NG"_s },
        { "ff-Adlm_SL"_s, "ff_Adlm_SL"_s },
        { "ff-Adlm_SN"_s, "ff_Adlm_SN"_s },
        { "ff_BF"_s, "ff_Latn_BF"_s },
        { "ff_CM"_s, "ff_Latn_CM"_s },
        { "ff_GH"_s, "ff_Latn_GH"_s },
        { "ff_GM"_s, "ff_Latn_GM"_s },
        { "ff_GN"_s, "ff_Latn_GN"_s },
        { "ff_GW"_s, "ff_Latn_GW"_s },
        { "ff_LR"_s, "ff_Latn_LR"_s },
        { "ff_MR"_s, "ff_Latn_MR"_s },
        { "ff_NE"_s, "ff_Latn_NE"_s },
        { "ff_NG"_s, "ff_Latn_NG"_s },
        { "ff_SL"_s, "ff_Latn_SL"_s },
        { "ff_SN"_s, "ff_Latn_SN"_s },
        { "hi-Latn"_s, "hi_Latn"_s },
        { "hi-Latn_IN"_s, "hi_Latn_IN"_s },
        { "ks-Arab"_s, "ks_Arab"_s },
        { "ks-Arab_IN"_s, "ks_Arab_IN"_s },
        { "ks-Deva"_s, "ks_Deva"_s },
        { "ks-Deva_IN"_s, "ks_Deva_IN"_s },
        { "ks_IN"_s, "ks_Aran_IN"_s },
        { "mni-Beng"_s, "mni_Beng"_s },
        { "mni-Beng_IN"_s, "mni_Beng_IN"_s },
        { "mni-Mtei"_s, "mni_Mtei"_s },
        { "mni-Mtei_IN"_s, "mni_Mtei_IN"_s },
        { "ms-Arab"_s, "ms_Arab"_s },
        { "ms-Arab_BN"_s, "ms_Arab_BN"_s },
        { "ms-Arab_MY"_s, "ms_Arab_MY"_s },
        { "nb"_s, "no"_s },
        { "pa"_s, "pa_Guru"_s },
        { "pa-Arab"_s, "pa_Arab"_s },
        { "pa-Arab_PK"_s, "pa_Arab_PK"_s },
        { "pa-Aran_PK"_s, "pa_Aran_PK"_s },
        { "pa_IN"_s, "pa_Guru_IN"_s },
        { "rej-Rjng"_s, "rej_Rjng"_s },
        { "rej-Rjng_ID"_s, "rej_Rjng_ID"_s },
        { "rhg-Rohg"_s, "rhg_Rohg"_s },
        { "rhg-Rohg_BD"_s, "rhg_Rohg_BD"_s },
        { "rhg-Rohg_MM"_s, "rhg_Rohg_MM"_s },
        { "sat-Deva"_s, "sat_Deva"_s },
        { "sat-Deva_IN"_s, "sat_Deva_IN"_s },
        { "sat-Olck"_s, "sat_Olck"_s },
        { "sat-Olck_IN"_s, "sat_Olck_IN"_s },
        { "sd-Arab"_s, "sd_Arab"_s },
        { "sd-Arab_PK"_s, "sd_Arab_PK"_s },
        { "sd-Deva"_s, "sd_Deva"_s },
        { "sd-Deva_IN"_s, "sd_Deva_IN"_s },
        { "shi"_s, "shi_Latn"_s },
        { "shi-Tfng"_s, "shi_Tfng"_s },
        { "shi-Tfng_MA"_s, "shi_Tfng_MA"_s },
        { "shi_MA"_s, "shi_Latn_MA"_s },
        { "sr"_s, "sr_Cyrl"_s },
        { "sr-Latn"_s, "sr_Latn"_s },
        { "sr-Latn_BA"_s, "sr_Latn_BA"_s },
        { "sr-Latn_ME"_s, "sr_Latn_ME"_s },
        { "sr-Latn_RS"_s, "sr_Latn_RS"_s },
        { "sr-Latn_XK"_s, "sr_Latn_XK"_s },
        { "sr_BA"_s, "sr_Cyrl_BA"_s },
        { "sr_ME"_s, "sr_Cyrl_ME"_s },
        { "sr_RS"_s, "sr_Cyrl_RS"_s },
        { "sr_XK"_s, "sr_Cyrl_XK"_s },
        { "su-Latn"_s, "su_Latn"_s },
        { "su-Latn_ID"_s, "su_Latn_ID"_s },
        { "ur-Arab"_s, "ur_Arab"_s },
        { "ur-Arab_IN"_s, "ur_Arab_IN"_s },
        { "ur-Arab_PK"_s, "ur_Arab_PK"_s },
        { "ur_IN"_s, "ur_Aran_IN"_s },
        { "ur_PK"_s, "ur_Aran_PK"_s },
        { "uz"_s, "uz_Latn"_s },
        { "uz-Arab"_s, "uz_Arab"_s },
        { "uz-Arab_AF"_s, "uz_Arab_AF"_s },
        { "uz-Cyrl"_s, "uz_Cyrl"_s },
        { "uz-Cyrl_UZ"_s, "uz_Cyrl_UZ"_s },
        { "uz_UZ"_s, "uz_Latn_UZ"_s },
        { "vai"_s, "vai_Vaii"_s },
        { "vai-Latn"_s, "vai_Latn"_s },
        { "vai-Latn_LR"_s, "vai_Latn_LR"_s },
        { "vai_LR"_s, "vai_Vaii_LR"_s },
        { "yue-Hans"_s, "yue_Hans"_s },
        { "yue-Hant"_s, "yue_Hant"_s },
        { "yue_CN"_s, "yue_Hans_CN"_s },
        { "yue_HK"_s, "yue_Hant_HK"_s },
        { "zh-Hans"_s, "zh_Hans"_s },
        { "zh-Hans_HK"_s, "zh_Hans_HK"_s },
        { "zh-Hans_JP"_s, "zh_Hans_JP"_s },
        { "zh-Hans_MO"_s, "zh_Hans_MO"_s },
        { "zh-Hant"_s, "zh_Hant"_s },
        { "zh-Hant_CN"_s, "zh_Hant_CN"_s },
        { "zh-Hant_JP"_s, "zh_Hant_JP"_s },
        { "zh_CN"_s, "zh_Hans_CN"_s },
        { "zh_HK"_s, "zh_Hant_HK"_s },
        { "zh_MO"_s, "zh_Hant_MO"_s },
        { "zh_SG"_s, "zh_Hans_SG"_s },
        { "zh_TW"_s, "zh_Hant_TW"_s }
    };

    auto entry = replacements.get().find(identifier);
    if (entry == replacements.get().end())
        return std::nullopt;
    return entry->value;
}

}
