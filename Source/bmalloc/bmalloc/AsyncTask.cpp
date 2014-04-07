#include "AsyncTask.h"
#include "NoInline.h"

namespace bmalloc {

template<typename Function>
NO_INLINE void AsyncTask<Function>::runSlowCase()
{
    State oldState = m_state.exchange(Signaled);
    if (oldState == Signaled || oldState == Running)
        return;

    if (oldState == Sleeping) {
        m_condition.notify_one();
        return;
    }

    ASSERT(oldState == Exited);
    pthread_create(&m_thread, nullptr, &pthreadEntryPoint, this);
    pthread_detach(m_thread);
}

template<typename Function>
void* AsyncTask<Function>::pthreadEntryPoint(void* asyncTask)
{
    static_cast<AsyncTask*>(asyncTask)->entryPoint();
    return nullptr;
}

template<typename Function>
void AsyncTask<Function>::entryPoint()
{
    State expectedState;
    while (1) {
        expectedState = Signaled;
        if (m_state.compare_exchange_weak(expectedState, Running)) {
            m_function();
            continue;
        }

        expectedState = Running;
        if (m_state.compare_exchange_weak(expectedState, Sleeping)) {
            std::mutex dummy; // No need for a real mutex because there's only one waiting thread.
            std::unique_lock<std::mutex> lock(dummy);
            m_condition.wait_for(lock, std::chrono::milliseconds(2), [=]() { return this->m_state != Sleeping; });
            continue;
        }

        expectedState = Sleeping;
        if (m_state.compare_exchange_weak(expectedState, Exited))
            break;
    }
}

} // namespace bmalloc
