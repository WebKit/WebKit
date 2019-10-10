/*
* Copyright (C) 2019 Apple Inc. All Rights Reserved.
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

#include "config.h"

#include <WebCore/StorageQuotaManager.h>
#include <WebCore/StorageQuotaUser.h>

using namespace WebCore;

namespace TestWebKitAPI {

static const int defaultQuota = 1 * KB;

class MockQuotaUser final : public WebCore::StorageQuotaUser {
public:
    explicit MockQuotaUser(StorageQuotaManager& manager)
        : m_manager(&manager)
    {
        manager.addUser(*this);
    }

    void setSpaceUsed(uint64_t spaceUsed) { m_spaceUsed = spaceUsed; }

private:
    uint64_t spaceUsed() const final
    {
        return m_spaceUsed;
    }

    void whenInitialized(CompletionHandler<void()>&& callback) final
    {
        if (!m_spaceUsed)
            ASSERT_EQ(m_manager->state(), StorageQuotaManager::State::Uninitialized);
        else
            ASSERT_EQ(m_manager->state(), StorageQuotaManager::State::ComputingSpaceUsed);

        // Reset usage to mock deletion.
        m_spaceUsed = 0;
        callback();
    }

    uint64_t m_spaceUsed { 0 };
    StorageQuotaManager* m_manager;
};
    
TEST(StorageQuotaManagerTest, Basic)
{
    StorageQuotaManager* storageQuotaManagerPtr = nullptr;
    StorageQuotaManager storageQuotaManager(defaultQuota, [&storageQuotaManagerPtr](uint64_t quota, uint64_t currentSpace, uint64_t spaceIncrease, CompletionHandler<void(Optional<uint64_t>)>&& completionHanlder) {
        ASSERT_TRUE(storageQuotaManagerPtr);
        ASSERT_EQ(storageQuotaManagerPtr->state(), StorageQuotaManager::State::WaitingForSpaceIncreaseResponse);

        // Allow all space increase requests.
        completionHanlder(spaceIncrease + currentSpace);
    });
    storageQuotaManagerPtr = &storageQuotaManager;


    MockQuotaUser mockUser(storageQuotaManager);

    // Grant directly.
    storageQuotaManager.requestSpace(0.1 * KB, [&](WebCore::StorageQuotaManager::Decision decision) {
        ASSERT_EQ(decision, WebCore::StorageQuotaManager::Decision::Grant);
        mockUser.setSpaceUsed(0.1 * KB);
    });

    // Grant after computing accurate usage.
    storageQuotaManager.requestSpace(1.0 * KB, [&](WebCore::StorageQuotaManager::Decision decision) {
        ASSERT_EQ(decision, WebCore::StorageQuotaManager::Decision::Grant);
        mockUser.setSpaceUsed(1.0 * KB);
    });

    // Grant after computing accurate usage and requesting space increase.
    storageQuotaManager.requestSpace(10 * KB, [&](WebCore::StorageQuotaManager::Decision decision) {
        ASSERT_EQ(storageQuotaManagerPtr->state(), StorageQuotaManager::State::MakingDecisionForRequest);
        ASSERT_EQ(decision, WebCore::StorageQuotaManager::Decision::Grant);
    });
}

} // namespace TestWebKitAPI
