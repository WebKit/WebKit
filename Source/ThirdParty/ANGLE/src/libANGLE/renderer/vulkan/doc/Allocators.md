# Allocators

Allocator helpers are used in the command buffer objects as a means to allocate memory for the
latter. Currently, only a pool allocator is implemented.

### Common interface

In `SecondaryCommandBuffer.h`, the helper classes related to the selected allocator type are
aliased as the following:

* `SecondaryCommandMemoryAllocator`

  * This is the main allocator object used in the allocator helper class. It is used as a type
	for some of the allocator helper's public functions.

* `SecondaryCommandBlockPool`

  * This allocator is used in `SecondaryCommandBuffer`.

* `SecondaryCommandBlockAllocator`

  * This allocator is defined in `CommandBufferHelperCommon`, and by extension, is used in its
	derived helper classes for render pass and outside render pass command buffer helpers.


### Pool allocator helpers

_Files: `AllocatorHelperPool.cpp` and `AllocatorHelperPool.h`_

- `SecondaryCommandMemoryAllocator` -> `DedicatedCommandMemoryAllocator` -> `angle::PoolAllocator`

- `SecondaryCommandBlockPool` -> `DedicatedCommandBlockPool`

- `SecondaryCommandBlockAllocator` -> `DedicatedCommandBlockAllocator`

