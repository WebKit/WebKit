# How to Add a New IPC MessageReceiver to WebKit

This guide covers adding a new IPC message receiver to the WebKit multi-process framework. All inter-process communication in WebKit goes through `.messages.in` files whose serialization/deserialization code is generated at build time.

## Background

Every object that receives IPC messages must:
1. Have a `.messages.in` file declaring its incoming messages
2. Inherit from one of the two base classes
3. Implement `didReceiveMessage(IPC::Connection&, IPC::Decoder&)`
4. Register itself with an `IPC::Connection` and deregister when destroyed
5. Be listed in the build system so code generation runs

There are two receiver base classes depending on which thread should handle the messages:

- **`IPC::MessageReceiver`** — messages dispatched on the **main thread** via a connection's `MessageReceiverMap`. Used when the object's state is main-thread-only.
- **`IPC::WorkQueueMessageReceiver<DestructionThread>`** — messages dispatched on a **background `WorkQueue`**. Used when you want to process messages off the main thread (e.g., to avoid blocking). This class is also `ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr`.

## Step 1: Define an Identifier Type (for multiplexed receivers)

If multiple instances of the same receiver can exist simultaneously (one per media player, one per source buffer, etc.), create a typed identifier. Copy the pattern from any existing `*Identifier.h`:

```cpp
// MyObjectIdentifier.h
#pragma once

#if ENABLE(GPU_PROCESS) && ENABLE(MY_FEATURE)

#include <wtf/ObjectIdentifier.h>

namespace WebKit {

struct MyObjectIdentifierType;
using MyObjectIdentifier = ObjectIdentifier<MyObjectIdentifierType>;
// For identifiers generated on multiple threads, use:
// using MyObjectIdentifier = AtomicObjectIdentifier<MyObjectIdentifierType>;

} // namespace WebKit

#endif
```

If there is only ever one instance (a singleton receiver), no identifier is needed and you register with `destinationID = 0`.

## Step 2: Write the `.messages.in` File

Create `MyObject.messages.in` alongside the `.h`/`.cpp` files. The attributes at the top control routing:

```
#if ENABLE(GPU_PROCESS) && ENABLE(MY_FEATURE)

[
    ExceptionForEnabledBy,      // omit if feature flags are the same on sender & receiver
    DispatchedFrom=WebContent,  // which process sends these messages
    DispatchedTo=GPU            // which process receives them
]
messages -> MyObject {
    // Fire-and-forget message:
    DoWork(uint64_t value)

    // Message with a synchronous reply (sends reply on same queue as the message):
    ComputeResult(uint64_t input) -> (uint64_t result)

    // Async reply (handler invoked later, Async suffix):
    FetchData(WebKit::MyObjectIdentifier id) -> () Async
}

#endif
```

Valid process values for `DispatchedFrom`/`DispatchedTo`: `WebContent`, `GPU`, `Networking`, `UI`.

Use `ExceptionForEnabledBy` when the sender and receiver are guarded by different feature-flag combinations.

## Step 3: Implement the Class

### Option A — Main-thread receiver (`IPC::MessageReceiver`)

Use when all your object's state is main-thread-only.

**Header:**
```cpp
#pragma once

#include "MessageReceiver.h"
#include "MyObjectIdentifier.h"
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace IPC { class Connection; class Decoder; }

namespace WebKit {

class MyObject final : public RefCounted<MyObject>, private IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(MyObject);
public:
    static Ref<MyObject> create(GPUConnectionToWebProcess&, MyObjectIdentifier);
    ~MyObject();

private:
    MyObject(GPUConnectionToWebProcess&, MyObjectIdentifier);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages (one method per entry in the .messages.in file)
    void doWork(uint64_t value);
    void computeResult(uint64_t input, CompletionHandler<void(uint64_t)>&&);
};

} // namespace WebKit
```

**Registration (constructor/destructor):**
```cpp
// In the factory or constructor:
connectionToWebProcess.messageReceiverMap().addMessageReceiver(
    Messages::MyObject::messageReceiverName(),
    m_identifier.toUInt64(),
    *this);

// In the destructor (or an explicit shutdown() method):
if (RefPtr connection = m_connection.get())
    connection->messageReceiverMap().removeMessageReceiver(
        Messages::MyObject::messageReceiverName(),
        m_identifier.toUInt64());
```

### Option B — Background work queue receiver (`IPC::WorkQueueMessageReceiver`)

Use when you want messages handled off the main thread.

**Header:**
```cpp
#pragma once

#include "MyObjectIdentifier.h"
#include "WorkQueueMessageReceiver.h"
#include <wtf/Identified.h>          // only when using Identified<>
#include <wtf/TZoneMalloc.h>
#include <wtf/WorkQueue.h>

namespace IPC { class Connection; class Decoder; }

namespace WebKit {

class MyObject final
    : public IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any>
    , public Identified<MyObjectIdentifier> {         // omit if singleton
    WTF_MAKE_TZONE_ALLOCATED(MyObject);
public:
    static Ref<MyObject> create(Ref<IPC::Connection>&&);
    ~MyObject();

    static Ref<WorkQueue> defaultQueue()
    {
        static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("com.apple.WebKit.MyObject"));
        return queue.get();
    }

    // Disambiguate ref/deref when inheriting from both WorkQueueMessageReceiver
    // and another ref-counted base (only needed in that specific case):
    void ref() const final { IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any>::ref(); }
    void deref() const final { IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any>::deref(); }

    // IPC::WorkQueueMessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    explicit MyObject(Ref<IPC::Connection>&&);
    void initializeConnection();

    // Messages
    void doWork(uint64_t value);
    void computeResult(uint64_t input, CompletionHandler<void(uint64_t)>&&);

    const Ref<IPC::Connection> m_connection;
};

} // namespace WebKit
```

**Registration (factory / destructor):**
```cpp
Ref<MyObject> MyObject::create(Ref<IPC::Connection>&& connection)
{
    auto object = adoptRef(*new MyObject(WTF::move(connection)));
    object->initializeConnection();
    return object;
}

void MyObject::initializeConnection()
{
    // Pass identifier().toUInt64() as the destination ID when multiplexed;
    // omit (defaults to 0) for singletons.
    m_connection->addWorkQueueMessageReceiver(
        Messages::MyObject::messageReceiverName(),
        defaultQueue(),
        *this,
        identifier().toUInt64());
}

MyObject::~MyObject()
{
    m_connection->removeWorkQueueMessageReceiver(
        Messages::MyObject::messageReceiverName(),
        identifier().toUInt64());
}
```

## Step 4: Implement `didReceiveMessage`

The generated code dispatches to the individual message-handler methods. The implementation file just calls the generated dispatcher:

```cpp
#include "config.h"
#include "MyObject.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MY_FEATURE)

#include "MyObjectMessages.h"   // generated header
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MyObject);

void MyObject::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    didReceiveMyObjectMessage(connection, decoder);   // generated dispatcher
}

// ... implement each message handler method ...

} // namespace WebKit

#endif
```

## Step 5: Sending Messages to the Counterpart

The generated `Messages::MyObject` namespace contains typed message structs. Send via the connection stored in the sender:

```cpp
// Fire and forget:
m_connection->send(Messages::MyObject::DoWork(42), destinationID);

// With sync or async reply:
m_connection->sendWithAsyncReply(
    Messages::MyObject::FetchData(id),
    [](auto result) { /* handle reply */ },
    destinationID);
```

## Step 6: Register with the Build System

Six files need updating whenever a new `.messages.in` file is added.

### `Source/WebKit/DerivedSources.make`

If the `.messages.in` file lives in a subdirectory of `Source/WebKit/` that does not already appear in the `VPATH` block at the top of the file, add it there first:

```make
VPATH = \
    ...
    $(WebKit2)/GPUProcess/cocoa \
    ...
```

Then add the receiver path (without extension) to the `MESSAGE_RECEIVERS` list. Entries are sorted **case-sensitively** (ASCII order): all uppercase-initial directory names (`GPU…`, `Remote…`, `Shape…`) sort before all lowercase-initial ones (`cocoa/`, `graphics/`, `media/`).

```make
MESSAGE_RECEIVERS = \
    ...
    GPUProcess/cocoa/MyObject \
    ...
```

### `Source/WebKit/DerivedSources-input.xcfilelist`

Add the `.messages.in` path. This list is also sorted **case-sensitively** by full path — uppercase-initial path components sort before lowercase ones.

```
$(PROJECT_DIR)/GPUProcess/cocoa/MyObject.messages.in
```

### `Source/WebKit/DerivedSources-output.xcfilelist`

Add the two generated output files. Note the `IPC/` subdirectory in the path:

```
$(BUILT_PRODUCTS_DIR)/DerivedSources/WebKit/IPC/MyObjectMessageReceiver.cpp
$(BUILT_PRODUCTS_DIR)/DerivedSources/WebKit/IPC/MyObjectMessages.h
```

### `Source/WebKit/Sources.txt` (cross-platform code)

Add the `.cpp` implementation using a path relative to `Source/WebKit/`:

```
<process>/<subdirectory>/MyObject.cpp
```

Add the generated `MessageReceiver.cpp` (filename only, no path prefix):

```
MyObjectMessageReceiver.cpp
```

### `Source/WebKit/SourcesCocoa.txt` (Cocoa-only code)

If the implementation is Cocoa-specific, add to `SourcesCocoa.txt` instead of `Sources.txt`. Paths are relative to `Source/WebKit/`. Objective-C++ files should include the `@nonARC` annotation:

```
<process>/<subdirectory>/MyObject.mm @nonARC
```

Add the generated `MessageReceiver.cpp` here for Cocoa-only receivers (filename only, no path prefix):

```
MyObjectMessageReceiver.cpp
```

### `Source/WebKit/WebKit.xcodeproj/project.pbxproj`

Add the new `.h`, `.cpp`/`.mm`, and `.messages.in` files to the Xcode project. Add them **unchecked** from any target membership (WebKit uses Unified Sources, so the Xcode target is not the primary build mechanism).

### `Source/WebKit/CMakeLists.txt` (non-Cocoa ports)

If the feature is also needed on GTK/WPE, add the `.cpp` and `.messages.in` to the CMake sources.

## Complete Checklist

- [ ] `MyObjectIdentifier.h` created (if multiplexed)
- [ ] `MyObject.messages.in` created with correct `DispatchedFrom`/`DispatchedTo`
- [ ] Class inherits `IPC::MessageReceiver` or `IPC::WorkQueueMessageReceiver<DestructionThread>`
- [ ] `didReceiveMessage` delegates to the generated dispatcher
- [ ] `initializeConnection` calls `addMessageReceiver` / `addWorkQueueMessageReceiver`
- [ ] Destructor calls `removeMessageReceiver` / `removeWorkQueueMessageReceiver`
- [ ] `WTF_MAKE_TZONE_ALLOCATED` in the header, `WTF_MAKE_TZONE_ALLOCATED_IMPL` in the `.cpp`
- [ ] `DerivedSources.make` updated (`VPATH` if new subdirectory; `MESSAGE_RECEIVERS` in case-sensitive sort order)
- [ ] `DerivedSources-input.xcfilelist` updated (case-sensitive sort order)
- [ ] `DerivedSources-output.xcfilelist` updated (paths include `IPC/` subdirectory)
- [ ] `Sources.txt` or `SourcesCocoa.txt` updated
- [ ] `WebKit.xcodeproj/project.pbxproj` updated (all new files added, unchecked from targets)
- [ ] `CMakeLists.txt` updated (if supporting non-Cocoa ports)

## Reference Implementations

- **WorkQueueMessageReceiver, multiplexed, GPU→WebContent pair:**
  - `Source/WebKit/GPUProcess/media/RemoteMediaResourceLoader.{h,cpp,messages.in}` (GPU side)
  - `Source/WebKit/WebProcess/GPU/media/RemoteMediaResourceLoaderProxy.{h,cpp,messages.in}` (WebContent side)

- **Main-thread MessageReceiver, multiplexed:**
  - `Source/WebKit/GPUProcess/media/RemoteMediaSourceProxy.{h,cpp,messages.in}`
  - `Source/WebKit/GPUProcess/media/RemoteSourceBufferProxy.{h,cpp,messages.in}`
