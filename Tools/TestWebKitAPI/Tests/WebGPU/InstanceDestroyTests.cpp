/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// The iOS simulator does not give these tests a WebGPU adapter.
#if !PLATFORM(IOS_FAMILY_SIMULATOR)

#include <WebGPU/WebGPUCpp.h>
#include <array>
#include <wtf/CompletionHandler.h>
#include <wtf/Condition.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/text/MakeString.h>

// WebGPU::Instance::destroy() of the WebGPU C++ API, which waits for the asynchronous compiles and
// the command buffers in flight, whose Metal completion handlers schedule work with the instance.
namespace TestWebKitAPI {

// The test's thread is the thread that the instance calls back on: the work that the instance
// schedules, on any thread, runs when the test runs it.
class ScheduledWork final : public ThreadSafeRefCounted<ScheduledWork> {
public:
    static Ref<ScheduledWork> create() { return adoptRef(*new ScheduledWork); }

    void append(Function<void()>&& work)
    {
        Locker locker { m_lock };
        m_work.append(WTF::move(work));
        m_condition.notifyAll();
    }

    // Runs the work scheduled so far, without waiting for more. Returns how much work ran.
    size_t runScheduledWork()
    {
        Deque<Function<void()>> work;
        {
            Locker locker { m_lock };
            work = std::exchange(m_work, { });
        }
        size_t count = work.size();
        while (!work.isEmpty())
            work.takeFirst()();
        return count;
    }

    // Runs the scheduled work until isDone() holds. Fails the test, rather than hangs it, when no
    // work is scheduled for too long.
    template<typename Predicate>
    void runWorkUntil(const Predicate& isDone)
    {
        while (!isDone()) {
            {
                Locker locker { m_lock };
                bool hasWork = m_condition.waitFor(m_lock, 10_s, [&] {
                    assertIsHeld(m_lock);
                    return !m_work.isEmpty();
                });
                if (!hasWork) {
                    ADD_FAILURE() << "No work was scheduled";
                    return;
                }
            }
            runScheduledWork();
        }
    }

private:
    ScheduledWork() = default;

    Lock m_lock;
    Condition m_condition;
    Deque<Function<void()>> m_work WTF_GUARDED_BY_LOCK(m_lock);
};

// A different shader every time, so that Metal compiles it rather than finds it in its cache.
static String uniqueValue()
{
    static unsigned counter;
    return makeString(cryptographicallyRandomNumber<uint32_t>(), "u + "_s, ++counter, 'u');
}

static String computeShader()
{
    return makeString("@group(0) @binding(0) var<storage, read_write> output: u32;\n"
        "@compute @workgroup_size(1) fn main() { output = "_s, uniqueValue(), "; }\n"_s);
}

static String renderShader()
{
    return makeString("@vertex fn vs() -> @builtin(position) vec4f { return vec4f(0, 0, 0, 1); }\n"
        "@fragment fn fs() -> @location(0) vec4f { return vec4f(f32("_s, uniqueValue(), "), 0, 0, 1); }\n"_s);
}

class WebGPUInstanceDestroy : public testing::Test {
public:
    void SetUp() override
    {
        m_instance = WebGPU::createInstance(WebGPU::InstanceDescriptor {
            .scheduleWork = [scheduledWork = m_scheduledWork](Function<void()>&& work) {
                scheduledWork->append(WTF::move(work));
            },
        });
        ASSERT_TRUE(m_instance);

        bool done = false;
        m_instance->requestAdapter({ }, [&](RefPtr<WebGPU::Adapter>&& adapter) {
            m_adapter = WTF::move(adapter);
            done = true;
        });
        m_scheduledWork->runWorkUntil([&] {
            return done;
        });
        ASSERT_TRUE(m_adapter);

        done = false;
        m_adapter->requestDevice({ }, [&](std::expected<Ref<WebGPU::Device>, String>&& device) {
            if (device)
                m_device = WTF::move(*device);
            done = true;
        });
        m_scheduledWork->runWorkUntil([&] {
            return done;
        });
        ASSERT_TRUE(m_device);
    }

    void TearDown() override
    {
        m_device = nullptr;
        m_adapter = nullptr;
        if (m_instance) {
            destroyAndRunScheduledWork();
            m_instance = nullptr;
        }
    }

protected:
    // Destroys the instance, and runs the work that the compiles and the command buffers in flight
    // scheduled. A pipeline is compiled in steps, the next of which the work of the previous one
    // starts, so this destroys the instance again for the compiles that the work started, until no
    // work is left. It never waits for work: when destroy() returns, the work it waited for has
    // been scheduled.
    void destroyAndRunScheduledWork()
    {
        m_instance->destroy();
        while (m_scheduledWork->runScheduledWork())
            m_instance->destroy();
    }

    // A compute pipeline needs a layout. This one is generated from the pipeline's shaders.
    RefPtr<WebGPU::PipelineLayout> autoLayout()
    {
        return m_device->createPipelineLayout({ .label = { }, .bindGroupLayouts = { }, .isAutogenerated = true });
    }

    const Ref<ScheduledWork> m_scheduledWork { ScheduledWork::create() };
    RefPtr<WebGPU::Instance> m_instance;
    RefPtr<WebGPU::Adapter> m_adapter;
    RefPtr<WebGPU::Device> m_device;
};

static constexpr unsigned compileCount = 8;

TEST_F(WebGPUInstanceDestroy, WaitsForPendingComputePipelineCompiles)
{
    unsigned completed = 0;
    for (unsigned i = 0; i < compileCount; ++i) {
        RefPtr module = m_device->createShaderModule({ .code = computeShader() });
        ASSERT_TRUE(module);
        m_device->createComputePipelineAsync({ .layout = autoLayout(), .compute = { .module = *module } }, [&](auto&& result) {
            EXPECT_TRUE(result.has_value());
            ++completed;
        });
    }

    // When destroy() returns, every compile has scheduled the work that completes it.
    destroyAndRunScheduledWork();
    EXPECT_EQ(completed, compileCount);
}

TEST_F(WebGPUInstanceDestroy, WaitsForPendingRenderPipelineCompiles)
{
    unsigned completed = 0;
    std::array<std::optional<WebGPU::ColorTargetState>, 1> targets { WebGPU::ColorTargetState { .format = WebGPU::TextureFormat::Rgba8unorm } };
    for (unsigned i = 0; i < compileCount; ++i) {
        RefPtr module = m_device->createShaderModule({ .code = renderShader() });
        ASSERT_TRUE(module);
        m_device->createRenderPipelineAsync({
            .vertex = { .stage = { .module = *module, .entryPoint = "vs"_s } },
            .fragment = WebGPU::FragmentState { .stage = { .module = *module, .entryPoint = "fs"_s }, .targets = targets },
        }, [&](auto&& result) {
            EXPECT_TRUE(result.has_value());
            ++completed;
        });
    }

    destroyAndRunScheduledWork();
    EXPECT_EQ(completed, compileCount);
}

TEST_F(WebGPUInstanceDestroy, WaitsForPendingCompilesOfADestroyedDevice)
{
    // The GPU process destroys the devices before the instance.
    bool computeCompleted = false;
    RefPtr computeModule = m_device->createShaderModule({ .code = computeShader() });
    ASSERT_TRUE(computeModule);
    m_device->createComputePipelineAsync({ .layout = autoLayout(), .compute = { .module = *computeModule } }, [&](auto&&) {
        computeCompleted = true;
    });

    bool renderCompleted = false;
    RefPtr renderModule = m_device->createShaderModule({ .code = renderShader() });
    ASSERT_TRUE(renderModule);
    std::array<std::optional<WebGPU::ColorTargetState>, 1> targets { WebGPU::ColorTargetState { .format = WebGPU::TextureFormat::Rgba8unorm } };
    m_device->createRenderPipelineAsync({
        .vertex = { .stage = { .module = *renderModule, .entryPoint = "vs"_s } },
        .fragment = WebGPU::FragmentState { .stage = { .module = *renderModule, .entryPoint = "fs"_s }, .targets = targets },
    }, [&](auto&&) {
        renderCompleted = true;
    });

    m_device->destroy();

    destroyAndRunScheduledWork();
    EXPECT_TRUE(computeCompleted);
    EXPECT_TRUE(renderCompleted);
}

TEST_F(WebGPUInstanceDestroy, WaitsForSubmittedWork)
{
    RefPtr encoder = m_device->createCommandEncoder({ });
    ASSERT_TRUE(encoder);
    RefPtr commandBuffer = encoder->finish({ });
    ASSERT_TRUE(commandBuffer);
    Ref queue = m_device->queue();
    Vector<Ref<WebGPU::CommandBuffer>> commandBuffers;
    commandBuffers.append(commandBuffer.releaseNonNull());
    queue->submit(WTF::move(commandBuffers));
    bool done = false;
    queue->onSubmittedWorkDone([&](bool) {
        done = true;
    });

    // When destroy() returns, the command buffer has completed, and the work that calls back has
    // been scheduled.
    destroyAndRunScheduledWork();
    EXPECT_TRUE(done);
}

TEST_F(WebGPUInstanceDestroy, WaitsForPendingCompilesAndSubmittedWork)
{
    unsigned completed = 0;
    RefPtr computeModule = m_device->createShaderModule({ .code = computeShader() });
    ASSERT_TRUE(computeModule);
    m_device->createComputePipelineAsync({ .layout = autoLayout(), .compute = { .module = *computeModule } }, [&](auto&& result) {
        EXPECT_TRUE(result.has_value());
        ++completed;
    });

    RefPtr encoder = m_device->createCommandEncoder({ });
    ASSERT_TRUE(encoder);
    RefPtr commandBuffer = encoder->finish({ });
    ASSERT_TRUE(commandBuffer);
    Ref queue = m_device->queue();
    Vector<Ref<WebGPU::CommandBuffer>> commandBuffers;
    commandBuffers.append(commandBuffer.releaseNonNull());
    queue->submit(WTF::move(commandBuffers));
    bool done = false;
    queue->onSubmittedWorkDone([&](bool) {
        done = true;
    });

    RefPtr renderModule = m_device->createShaderModule({ .code = renderShader() });
    ASSERT_TRUE(renderModule);
    std::array<std::optional<WebGPU::ColorTargetState>, 1> targets { WebGPU::ColorTargetState { .format = WebGPU::TextureFormat::Rgba8unorm } };
    m_device->createRenderPipelineAsync({
        .vertex = { .stage = { .module = *renderModule, .entryPoint = "vs"_s } },
        .fragment = WebGPU::FragmentState { .stage = { .module = *renderModule, .entryPoint = "fs"_s }, .targets = targets },
    }, [&](auto&& result) {
        EXPECT_TRUE(result.has_value());
        ++completed;
    });

    destroyAndRunScheduledWork();
    EXPECT_EQ(completed, 2u);
    EXPECT_TRUE(done);
}

TEST_F(WebGPUInstanceDestroy, ReturnsWhenTheCompilesHaveAlreadyCompleted)
{
    unsigned completed = 0;
    for (unsigned i = 0; i < compileCount; ++i) {
        RefPtr module = m_device->createShaderModule({ .code = computeShader() });
        ASSERT_TRUE(module);
        m_device->createComputePipelineAsync({ .layout = autoLayout(), .compute = { .module = *module } }, [&](auto&& result) {
            EXPECT_TRUE(result.has_value());
            ++completed;
        });
    }
    m_scheduledWork->runWorkUntil([&] {
        return completed == compileCount;
    });

    destroyAndRunScheduledWork();
    EXPECT_EQ(completed, compileCount);
}

TEST_F(WebGPUInstanceDestroy, WaitsForTheCompilesStartedSinceTheLastDestroy)
{
    m_instance->destroy();
    m_instance->destroy();

    bool completed = false;
    RefPtr module = m_device->createShaderModule({ .code = computeShader() });
    ASSERT_TRUE(module);
    m_device->createComputePipelineAsync({ .layout = autoLayout(), .compute = { .module = *module } }, [&](auto&& result) {
        EXPECT_TRUE(result.has_value());
        completed = true;
    });
    destroyAndRunScheduledWork();
    EXPECT_TRUE(completed);
}

TEST_F(WebGPUInstanceDestroy, ReturnsWhenNothingIsPending)
{
    m_instance->destroy();
    EXPECT_EQ(m_scheduledWork->runScheduledWork(), 0u);
}

TEST_F(WebGPUInstanceDestroy, ReleasesTheInstanceOnTheThreadThatDestroysIt)
{
    unsigned completed = 0;
    for (unsigned i = 0; i < compileCount; ++i) {
        RefPtr module = m_device->createShaderModule({ .code = computeShader() });
        ASSERT_TRUE(module);
        m_device->createComputePipelineAsync({ .layout = autoLayout(), .compute = { .module = *module } }, [&](auto&& result) {
            EXPECT_TRUE(result.has_value());
            ++completed;
        });
    }

    RefPtr renderModule = m_device->createShaderModule({ .code = renderShader() });
    ASSERT_TRUE(renderModule);
    std::array<std::optional<WebGPU::ColorTargetState>, 1> targets { WebGPU::ColorTargetState { .format = WebGPU::TextureFormat::Rgba8unorm } };
    m_device->createRenderPipelineAsync({
        .vertex = { .stage = { .module = *renderModule, .entryPoint = "vs"_s } },
        .fragment = WebGPU::FragmentState { .stage = { .module = *renderModule, .entryPoint = "fs"_s }, .targets = targets },
    }, [&](auto&& result) {
        EXPECT_TRUE(result.has_value());
        ++completed;
    });

    RefPtr encoder = m_device->createCommandEncoder({ });
    ASSERT_TRUE(encoder);
    RefPtr commandBuffer = encoder->finish({ });
    ASSERT_TRUE(commandBuffer);
    Ref queue = m_device->queue();
    Vector<Ref<WebGPU::CommandBuffer>> commandBuffers;
    commandBuffers.append(commandBuffer.releaseNonNull());
    queue->submit(WTF::move(commandBuffers));
    bool done = false;
    queue->onSubmittedWorkDone([&](bool) {
        done = true;
    });

    // As the GPU process does: destroy the instance, release it, and then run the work it scheduled.
    // No Metal completion handler holds the instance anymore, so the reference that the test
    // releases is the last one, and the instance is destroyed here.
    m_instance->destroy();
    ThreadSafeWeakPtr<WebGPU::Instance> weakInstance { *m_instance };
    m_instance = nullptr;
    EXPECT_FALSE(weakInstance.get());

    // Without the instance, the steps of the compiles that the work starts compile synchronously,
    // so the work completes every pipeline without waiting.
    while (m_scheduledWork->runScheduledWork()) { }
    EXPECT_EQ(completed, compileCount + 1);
    EXPECT_TRUE(done);
}

} // namespace TestWebKitAPI

#endif // !PLATFORM(IOS_FAMILY_SIMULATOR)
