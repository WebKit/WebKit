/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DOMFormData.h"

#include "ContextDestructionObserverInlines.h"
#include "Document.h"
#include "ExceptionOr.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"

template class mpark::variant<WTF::Ref<WebCore::File>, RefPtr<StringImpl>>;

namespace WebCore {

DOMFormData::DOMFormData(ScriptExecutionContext* context, const PAL::TextEncoding& encoding)
    : ContextDestructionObserver(context)
    , m_encoding(encoding)
{
}

ExceptionOr<Ref<DOMFormData>> DOMFormData::create(ScriptExecutionContext& context, HTMLFormElement* form, HTMLElement* submitter)
{
    // https://xhr.spec.whatwg.org/#dom-formdata
    auto formData = adoptRef(*new DOMFormData(&context));
    if (!form)
        return formData;

    RefPtr<HTMLFormControlElement> control;
    if (submitter) {
        control = dynamicDowncast<HTMLFormControlElement>(*submitter);
        if (!control || !control->isSubmitButton())
            return Exception { ExceptionCode::TypeError, "The specified element is not a submit button."_si };
        if (control->form() != form)
            return Exception { ExceptionCode::NotFoundError, "The specified element is not owned by this form element."_si };
    }
    auto result = form->constructEntryList(control.get(), WTF::move(formData), nullptr);
    
    if (!result)
        return Exception { ExceptionCode::InvalidStateError, "Already constructing Form entry list."_si };
    
    return result.releaseNonNull();
}

Ref<DOMFormData> DOMFormData::create(ScriptExecutionContext* context, const PAL::TextEncoding& encoding)
{
    return adoptRef(*new DOMFormData(context, encoding));
}

Ref<DOMFormData> DOMFormData::clone() const
{
    auto newFormData = adoptRef(*new DOMFormData(protect(scriptExecutionContext()), this->encoding()));
    newFormData->m_items = m_items;
    
    return newFormData;
}

// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#create-an-entry
static auto createStringEntry(StringImpl* name, StringImpl* value) -> DOMFormData::Item
{
    return {
        replaceUnpairedSurrogatesWithReplacementCharacter(RefPtr<StringImpl>(name)),
        replaceUnpairedSurrogatesWithReplacementCharacter(RefPtr<StringImpl>(value)),
    };
}

// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#create-an-entry
static auto createFileEntry(StringImpl* name, Blob& blob, StringImpl* filename) -> DOMFormData::Item
{
    auto usvName = replaceUnpairedSurrogatesWithReplacementCharacter(RefPtr<StringImpl>(name));

    if (RefPtr file = dynamicDowncast<File>(blob)) {
        if (filename)
            return { usvName, File::create(protect(blob.scriptExecutionContext()).get(), *file, filename) };
        return { usvName, file.releaseNonNull() };
    }
    return { usvName, File::create(protect(blob.scriptExecutionContext()).get(), blob, !filename ? "blob"_si : RefPtr { filename }) };
}

void DOMFormData::append(StringImpl* name, StringImpl* value)
{
    m_items.append(createStringEntry(name, value));
}

void DOMFormData::append(StringImpl* name, Blob& blob, StringImpl* filename)
{
    m_items.append(createFileEntry(name, blob, filename));
}

void DOMFormData::remove(StringImpl* name)
{
    m_items.removeAllMatching([&name] (const auto& item) {
        return equal(item.name, name);
    });
}

auto DOMFormData::get(StringImpl* name) -> std::optional<FormDataEntryValue>
{
    for (auto& item : m_items) {
        if (equal(item.name, name))
            return item.data;
    }

    return std::nullopt;
}

auto DOMFormData::getAll(StringImpl* name) -> Vector<FormDataEntryValue>
{
    Vector<FormDataEntryValue> result;

    for (auto& item : m_items) {
        if (equal(item.name, name))
            result.append(item.data);
    }

    return result;
}

bool DOMFormData::has(StringImpl* name)
{
    for (auto& item : m_items) {
        if (equal(item.name, name))
            return true;
    }
    
    return false;
}

void DOMFormData::set(StringImpl* name, StringImpl* value)
{
    set(name, { name, value });
}

void DOMFormData::set(StringImpl* name, Blob& blob, StringImpl* filename)
{
    set(name, createFileEntry(name, blob, filename));
}

void DOMFormData::set(StringImpl* name, Item&& item)
{
    std::optional<size_t> initialMatchLocation;

    // Find location of the first item with a matching name.
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (equal(name, m_items[i].name)) {
            initialMatchLocation = i;
            break;
        }
    }

    if (initialMatchLocation) {
        m_items[*initialMatchLocation] = WTF::move(item);

        m_items.removeAllMatching([&name] (const auto& item) {
            return equal(item.name, name);
        }, *initialMatchLocation + 1);
        return;
    }

    m_items.append(WTF::move(item));
}

DOMFormData::Iterator::Iterator(DOMFormData& target)
    : m_target(target)
{
}

std::optional<KeyValuePair<RefPtr<StringImpl>, DOMFormData::FormDataEntryValue>> DOMFormData::Iterator::next()
{
    auto& items = m_target->items();
    if (m_index >= items.size())
        return std::nullopt;

    auto& item = items[m_index++];
    return makeKeyValuePair(item.name, item.data);
}

} // namespace WebCore
