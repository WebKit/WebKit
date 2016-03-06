/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Autofill.h"

#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

enum class AutofillCategory {
    Off,
    Automatic,
    Normal,
    Contact,
};

struct AutofillInfo {
    AutofillFieldName fieldName;
    AutofillCategory category;
};

static HashMap<AtomicString, AutofillInfo>& fieldNameMap()
{
    static NeverDestroyed<HashMap<AtomicString, AutofillInfo>> map;
    if (map.get().isEmpty()) {
        map.get().add(AtomicString("on", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::None, AutofillCategory::Off });
        map.get().add(AtomicString("off", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::None,  AutofillCategory::Automatic });
        map.get().add(AtomicString("name", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::Name, AutofillCategory::Normal });
        map.get().add(AtomicString("honorific-prefix", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::HonorificPrefix, AutofillCategory::Normal });
        map.get().add(AtomicString("given-name", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::GivenName, AutofillCategory::Normal });
        map.get().add(AtomicString("additional-name", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::AdditionalName, AutofillCategory::Normal });
        map.get().add(AtomicString("family-name", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::FamilyName, AutofillCategory::Normal });
        map.get().add(AtomicString("honorific-suffix", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::HonorificSuffix, AutofillCategory::Normal });
        map.get().add(AtomicString("nickname", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::Nickname, AutofillCategory::Normal });
        map.get().add(AtomicString("username", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::Username, AutofillCategory::Normal });
        map.get().add(AtomicString("new-password", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::NewPassword, AutofillCategory::Normal });
        map.get().add(AtomicString("current-password", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::CurrentPassword, AutofillCategory::Normal });
        map.get().add(AtomicString("organization-title", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::OrganizationTitle, AutofillCategory::Normal });
        map.get().add(AtomicString("organization", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::Organization, AutofillCategory::Normal });
        map.get().add(AtomicString("street-address", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::StreetAddress, AutofillCategory::Normal });
        map.get().add(AtomicString("address-line1", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::AddressLine1, AutofillCategory::Normal });
        map.get().add(AtomicString("address-line2", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::AddressLine2, AutofillCategory::Normal });
        map.get().add(AtomicString("address-line3", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::AddressLine3, AutofillCategory::Normal });
        map.get().add(AtomicString("address-level4", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::AddressLevel4, AutofillCategory::Normal });
        map.get().add(AtomicString("address-level3", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::AddressLevel3, AutofillCategory::Normal });
        map.get().add(AtomicString("address-level2", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::AddressLevel2, AutofillCategory::Normal });
        map.get().add(AtomicString("address-level1", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::AddressLevel1, AutofillCategory::Normal });
        map.get().add(AtomicString("country", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::Country, AutofillCategory::Normal });
        map.get().add(AtomicString("country-name", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::CountryName, AutofillCategory::Normal });
        map.get().add(AtomicString("postal-code", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::PostalCode, AutofillCategory::Normal });
        map.get().add(AtomicString("cc-name", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::CcName, AutofillCategory::Normal });
        map.get().add(AtomicString("cc-given-name", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::CcGivenName, AutofillCategory::Normal });
        map.get().add(AtomicString("cc-additional-name", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::CcAdditionalName, AutofillCategory::Normal });
        map.get().add(AtomicString("cc-family-name", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::CcFamilyName, AutofillCategory::Normal });
        map.get().add(AtomicString("cc-number", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::CcNumber, AutofillCategory::Normal });
        map.get().add(AtomicString("cc-exp", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::CcExp, AutofillCategory::Normal });
        map.get().add(AtomicString("cc-exp-month", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::CcExpMonth, AutofillCategory::Normal });
        map.get().add(AtomicString("cc-exp-year", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::CcExpYear, AutofillCategory::Normal });
        map.get().add(AtomicString("cc-csc", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::CcCsc, AutofillCategory::Normal });
        map.get().add(AtomicString("cc-type", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::CcType, AutofillCategory::Normal });
        map.get().add(AtomicString("transaction-currency", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::TransactionCurrency, AutofillCategory::Normal });
        map.get().add(AtomicString("transaction-amount", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::TransactionAmount, AutofillCategory::Normal });
        map.get().add(AtomicString("language", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::Language, AutofillCategory::Normal });
        map.get().add(AtomicString("bday", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::Bday, AutofillCategory::Normal });
        map.get().add(AtomicString("bday-day", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::BdayDay, AutofillCategory::Normal });
        map.get().add(AtomicString("bday-month", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::BdayMonth, AutofillCategory::Normal });
        map.get().add(AtomicString("bday-year", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::BdayYear, AutofillCategory::Normal });
        map.get().add(AtomicString("sex", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::Sex, AutofillCategory::Normal });
        map.get().add(AtomicString("url", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::URL, AutofillCategory::Normal });
        map.get().add(AtomicString("photo", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::Photo, AutofillCategory::Normal });

        map.get().add(AtomicString("tel", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::Tel, AutofillCategory::Contact });
        map.get().add(AtomicString("tel-country-code", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::TelCountryCode, AutofillCategory::Contact });
        map.get().add(AtomicString("tel-national", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::TelNational, AutofillCategory::Contact });
        map.get().add(AtomicString("tel-area-code", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::TelAreaCode, AutofillCategory::Contact });
        map.get().add(AtomicString("tel-local", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::TelLocal, AutofillCategory::Contact });
        map.get().add(AtomicString("tel-local-prefix", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::TelLocalPrefix, AutofillCategory::Contact });
        map.get().add(AtomicString("tel-local-suffix", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::TelLocalSuffix, AutofillCategory::Contact });
        map.get().add(AtomicString("tel-extension", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::TelExtension, AutofillCategory::Contact });
        map.get().add(AtomicString("email", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::Email, AutofillCategory::Contact });
        map.get().add(AtomicString("impp", AtomicString::ConstructFromLiteral), AutofillInfo{ AutofillFieldName::Impp, AutofillCategory::Contact });
    }

    return map;
}

AutofillFieldName toAutofillFieldName(const AtomicString& value)
{
    auto map = fieldNameMap();
    auto it = map.find(value);
    if (it == map.end())
        return AutofillFieldName::None;
    return it->value.fieldName;
}

static inline bool isContactToken(const AtomicString& token)
{
    static NeverDestroyed<AtomicString> home("home", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> work("work", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> mobile("mobile", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> fax("fax", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> pager("pager", AtomicString::ConstructFromLiteral);

    return token == home || token == work || token == mobile || token == fax || token == pager;
}

static unsigned maxTokensForAutofillFieldCategory(AutofillCategory category)
{
    switch (category) {
    case AutofillCategory::Automatic:
    case AutofillCategory::Off:
        return 1;

    case AutofillCategory::Normal:
        return 3;

    case AutofillCategory::Contact:
        return 4;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

// https://html.spec.whatwg.org/multipage/forms.html#processing-model-3
AutofillData AutofillData::createFromHTMLFormControlElement(const HTMLFormControlElement& element)
{
    static NeverDestroyed<AtomicString> on("on", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> off("off", AtomicString::ConstructFromLiteral);

    // Label: Default
    // 26. Let the element's IDL-exposed autofill value be the empty string, and its autofill hint set and autofill scope be empty.
    // 27. If the element's autocomplete attribute is wearing the autofill anchor mantle, then let the element's autofill field name be the empty string and abort these steps.
    // 28. Let form be the element's form owner, if any, or null otherwise.
    // 29. If form is not null and form's autocomplete attribute is in the off state, then let the element's autofill field name be "off". Otherwise, let the element's autofill field name be "on".
    auto defaultLabel = [&] () -> AutofillData {
        if (element.autofillMantle() == AutofillMantle::Anchor)
            return { emptyString(), emptyString() };
        
        auto form = element.form();
        if (form && form->autocomplete() == off)
            return { off, emptyString() };
        return { on, emptyString() };
    };


    const AtomicString& attributeValue = element.fastGetAttribute(HTMLNames::autocompleteAttr);

    // 1. If the element has no autocomplete attribute, then jump to the step labeled default.
    if (attributeValue == nullAtom)
        return defaultLabel();

    // 2. Let tokens be the result of splitting the attribute's value on spaces.
    SpaceSplitString tokens(attributeValue, true);

    // 3. If tokens is empty, then jump to the step labeled default.
    if (tokens.isEmpty())
        return defaultLabel();
    
    // 4. Let index be the index of the last token in tokens
    unsigned index = tokens.size() - 1;

    // 5. If the indexth token in tokens is not an ASCII case-insensitive match for one of the
    // tokens given in the first column of the following table, or if the number of tokens in
    // tokens is greater than the maximum number given in the cell in the second column of that
    // token's row, then jump to the step labeled default. Otherwise, let field be the string given
    // in the cell of the first column of the matching row, and let category be the value of the
    // cell in the third column of that same row.
    auto& map = fieldNameMap();

    auto it = map.find(tokens[index]);
    if (it == map.end())
        return defaultLabel();
    
    auto category = it->value.category;

    if (tokens.size() > maxTokensForAutofillFieldCategory(category))
        return defaultLabel();

    const auto& field = tokens[index];

    // 6. If category is Off or Automatic but the element's autocomplete attribute is wearing the
    // autofill anchor mantle, then jump to the step labeled default.
    auto mantle = element.autofillMantle();
    if ((category == AutofillCategory::Off || category == AutofillCategory::Automatic) && mantle == AutofillMantle::Anchor)
        return defaultLabel();

    // 7. If category is Off, let the element's autofill field name be the string "off", let its
    // autofill hint set be empty, and let its IDL-exposed autofill value be the string "off".
    // Then, abort these steps.
    if (category == AutofillCategory::Off)
        return { off, off.get().string() };

    // 8. If category is Automatic, let the element's autofill field name be the string "on",
    // let its autofill hint set be empty, and let its IDL-exposed autofill value be the string
    // "on". Then, abort these steps.
    if (category == AutofillCategory::Off)
        return { on, on.get().string() };

    // 9. Let scope tokens be an empty list.
    // 10. Let hint tokens be an empty set.

    // NOTE: We are ignoring these steps as we don't currently make use of scope tokens or hint tokens anywhere.

    // 11. Let IDL value have the same value as field.
    String idlValue = field;

    // 12, If the indexth token in tokens is the first entry, then skip to the step labeled done.
    if (index == 0)
        return { field, idlValue };

    // 13. Decrement index by one
    index--;

    // 14. If category is Contact and the indexth token in tokens is an ASCII case-insensitive match
    // for one of the strings in the following list, then run the substeps that follow:
    const auto& contactToken = tokens[index];
    if (category == AutofillCategory::Contact && isContactToken(contactToken)) {
        // 1. Let contact be the matching string from the list above.
        const auto& contact = contactToken;

        // 2. Insert contact at the start of scope tokens.
        // 3. Add contact to hint tokens.

        // NOTE: We are ignoring these steps as we don't currently make use of scope tokens or hint tokens anywhere.

        // 4. Let IDL value be the concatenation of contact, a U+0020 SPACE character, and the previous
        // value of IDL value (which at this point will always be field).
        idlValue = WTF::makeString(contact, " ", idlValue);

        // 5. If the indexth entry in tokens is the first entry, then skip to the step labeled done.
        if (index == 0)
            return { field, idlValue };

        // 6. Decrement index by one.
        index--;
    }

    // 15. If the indexth token in tokens is an ASCII case-insensitive match for one of the strings
    // in the following list, then run the substeps that follow:
    const auto& modeToken = tokens[index];
    if (equalIgnoringASCIICase(modeToken, "shipping") || equalIgnoringASCIICase(modeToken, "billing")) {
        // 1. Let mode be the matching string from the list above.
        const auto& mode = modeToken;

        // 2. Insert mode at the start of scope tokens.
        // 3. Add mode to hint tokens.

        // NOTE: We are ignoring these steps as we don't currently make use of scope tokens or hint tokens anywhere.

        // 4. Let IDL value be the concatenation of mode, a U+0020 SPACE character, and the previous
        // value of IDL value (which at this point will either be field or the concatenation of contact,
        // a space, and field).
        idlValue = WTF::makeString(mode, " ", idlValue);

        // 5. If the indexth entry in tokens is the first entry, then skip to the step labeled done.
        if (index == 0)
            return { field, idlValue };

        // 6. Decrement index by one.
        index--;
    }

    // 16. If the indexth entry in tokens is not the first entry, then jump to the step labeled default.
    if (index != 0)
        return defaultLabel();

    // 17. If the first eight characters of the indexth token in tokens are not an ASCII case-insensitive
    // match for the string "section-", then jump to the step labeled default.
    const auto& sectionToken = tokens[index];
    if (!sectionToken.startsWithIgnoringASCIICase("section-"))
        return defaultLabel();

    // 18. Let section be the indexth token in tokens, converted to ASCII lowercase.
    const auto& section = sectionToken;

    // 19. Insert section at the start of scope tokens.

    // NOTE: We are ignoring this step as we don't currently make use of scope tokens or hint tokens anywhere.

    // 20. Let IDL value be the concatenation of section, a U+0020 SPACE character, and the previous
    // value of IDL value.
    idlValue = WTF::makeString(section, " ", idlValue);

    return { field, idlValue };
}

} // namespace WebCore
