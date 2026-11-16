// Test file: Contains unwrapped Vulkan API calls that SHOULD be detected

void testUnwrappedCalls()
{
    // Unwrapped direct vk calls
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
    vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);

    // Unwrapped using lower camelCase variations
    queueSubmit(queue, 1, &submitInfo, fence);
    p_queueSubmit(queue, 1, &submitInfo, fence);
    p_vkQueueSubmit(queue, 1, &submitInfo, fence);
    pfn_queueSubmit(queue, 1, &submitInfo, fence);
    pfn_vkQueueSubmit(queue, 1, &submitInfo, fence);

    // Unwrapped using capital camelCase variations
    pCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
    p_CmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
    pfnCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
    pfn_CmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
    pVkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
    p_VkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
    pfnVkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
    pfn_VkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);

    // Unwrapped using snake_case variations
    cmd_draw(cmdBuffer, vertexCount, 1, 0, 0);
    p_cmd_draw(cmdBuffer, vertexCount, 1, 0, 0);
    pfn_cmd_draw(cmdBuffer, vertexCount, 1, 0, 0);
    cmd_set_viewport_w_scaling_nv(cmdBuffer, 0, 1, &viewportWScalings);
    create_direct_fb_surface_ext(instance, &createInfo, nullptr, &surface);
    create_ios_surface_mvk(instance, &createInfo, nullptr, &surface);
}
