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

#include "Document.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"

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
            return Exception { TypeError, "The specified element is not a submit button."_s };
        if (control->form() != form)
            return Exception { NotFoundError, "The specified element is not owned by this form element."_s };
    }
    auto result = form->constructEntryList(control.get(), WTFMove(formData), nullptr);
    
    if (!result)
        return Exception { InvalidStateError, "Already constructing Form entry list."_s };
    
    return result.releaseNonNull();
}

Ref<DOMFormData> DOMFormData::create(ScriptExecutionContext* context, const PAL::TextEncoding& encoding)
{
    return adoptRef(*new DOMFormData(context, encoding));
}

Ref<DOMFormData> DOMFormData::clone() const
{
    auto newFormData = adoptRef(*new DOMFormData(scriptExecutionContext(), this->encoding()));
    newFormData->m_items = m_items;
    
    return newFormData;
}

// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#create-an-entry
static auto createStringEntry(const String& name, const String& value) -> DOMFormData::Item
{
    return {
        replaceUnpairedSurrogatesWithReplacementCharacter(String(name)),
        replaceUnpairedSurrogatesWithReplacementCharacter(String(value)),
    };
}

// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#create-an-entry
static auto createFileEntry(const String& name, Blob& blob, const String& filename) -> DOMFormData::Item
{
    auto usvName = replaceUnpairedSurrogatesWithReplacementCharacter(String(name));

    if (!blob.isFile())
        return { usvName, File::create(blob.scriptExecutionContext(), blob, filename.isNull() ? "blob"_s : filename) };

    if (!filename.isNull())
        return { usvName, File::create(blob.scriptExecutionContext(), downcast<File>(blob), filename) };

    return { usvName, RefPtr<File> { &downcast<File>(blob) } };
}

void DOMFormData::append(const String& name, const String& value)
{
    m_items.append(createStringEntry(name, value));
}

void DOMFormData::append(const String& name, Blob& blob, const String& filename)
{
    m_items.append(createFileEntry(name, blob, filename));
}

void DOMFormData::remove(const String& name)
{
    m_items.removeAllMatching([&name] (const auto& item) {
        return item.name == name;
    });
}

auto DOMFormData::get(const String& name) -> std::optional<FormDataEntryValue>
{
    for (auto& item : m_items) {
        if (item.name == name)
            return item.data;
    }

    return std::nullopt;
}

auto DOMFormData::getAll(const String& name) -> Vector<FormDataEntryValue>
{
    Vector<FormDataEntryValue> result;

    for (auto& item : m_items) {
        if (item.name == name)
            result.append(item.data);
    }

    return result;
}

bool DOMFormData::has(const String& name)
{
    for (auto& item : m_items) {
        if (item.name == name)
            return true;
    }
    
    return false;
}

void DOMFormData::set(const String& name, const String& value)
{
    set(name, { name, value });
}

void DOMFormData::set(const String& name, Blob& blob, const String& filename)
{
    set(name, createFileEntry(name, blob, filename));
}

void DOMFormData::set(const String& name, Item&& item)
{
    std::optional<size_t> initialMatchLocation;

    // Find location of the first item with a matching name.
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (name == m_items[i].name) {
            initialMatchLocation = i;
            break;
        }
    }

    if (initialMatchLocation) {
        m_items[*initialMatchLocation] = WTFMove(item);

        m_items.removeAllMatching([&name] (const auto& item) {
            return item.name == name;
        }, *initialMatchLocation + 1);
        return;
    }

    m_items.append(WTFMove(item));
}

DOMFormData::Iterator::Iterator(DOMFormData& target)
    : m_target(target)
{
}

std::optional<KeyValuePair<String, DOMFormData::FormDataEntryValue>> DOMFormData::Iterator::next()
{
    auto& items = m_target->items();
    if (m_index >= items.size())
        return std::nullopt;

    auto& item = items[m_index++];
    return makeKeyValuePair(item.name, item.data);
}

} // namespace WebCore
