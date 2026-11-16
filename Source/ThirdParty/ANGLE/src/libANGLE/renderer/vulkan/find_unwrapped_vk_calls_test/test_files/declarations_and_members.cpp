// Test file: Function declarations, member calls, namespace calls should NOT be detected

// Function declarations - should be ignored
void vkQueueSubmit(VkQueue queue, uint32_t count, const VkSubmitInfo *pSubmits, VkFence fence);
VkResult vkCreateDevice(VkPhysicalDevice physicalDevice,
                        const VkDeviceCreateInfo *pCreateInfo,
                        const VkAllocationCallbacks *pAllocator,
                        VkDevice *pDevice);
PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice device, const char *pName);

class VulkanHelper
{
  public:
    // Member function - should be ignored
    void vkQueueSubmit(VkQueue queue) {}

    void test()
    {
        // Member call via dot - should be ignored
        helper.vkQueueSubmit(queue);

        // Member call via arrow - should be ignored
        helperPtr->vkQueueSubmit(queue);

        // Namespace qualified call - should be ignored
        vkns::vkQueueSubmit(queue, 1, &info, fence);
    }
};

// Real unwrapped call that SHOULD be detected
void realCall()
{
    vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);
}
