// Test file: Contains properly wrapped Vulkan API calls that should NOT be detected

void testWrappedCalls()
{
    // Wrapped with VK_CALL_WITH_GROUP macro
    VK_CALL_WITH_GROUP(vkQueueSubmit(queue, 1, &submitInfo, fence));
    VK_CALL_WITH_GROUP(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));

    // Wrapped with VK_SECONDARY_CMD_CALL macro
    VK_SECONDARY_CMD_CALL(vkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0));

    // Wrapped with VK_CALL macro
    VkResult result = VK_CALL(vkGetPhysicalDeviceSurfaceFormats2KHR, physicalDevice, &surfaceInfo2,
                              &surfaceFormatCount, nullptr);

    // Wrapped with timer scope
    {
        ScopedVulkanApiPerfTimer timer;
        vkQueueSubmit(queue, 1, &submitInfo, fence);
        vkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
    }
}
