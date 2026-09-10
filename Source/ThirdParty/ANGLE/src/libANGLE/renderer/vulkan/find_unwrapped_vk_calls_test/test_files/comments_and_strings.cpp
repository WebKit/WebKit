// Test file: VK calls inside comments and strings should NOT be detected
// vkQueueSubmit(queue, 1, &submitInfo, fence);
// vkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);

void testCommentsAndStrings()
{
    /* Multi-line comment with vkCreateDevice(physicalDevice, &createInfo, nullptr, &device); */

    // This real VK call IS detected as unwrapped:
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    // The same call inside a string is NOT detected:
    const char *errorMsg = "vkQueueSubmit(queue, 1, &submitInfo, fence) failed with error";
    // Likewise, this real VK call IS detected as unwrapped:
    vkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
    // The same call inside a string is NOT detected:
    const char *debugStr = "Calling vkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0)";

    // Raw string literal containing VK call - should be ignored
    const char *shaderCode = R"glsl(
        // vkCmdDraw should not be detected here
        layout(location = 0) in vec4 position;
    )glsl";

    // This is a real unwrapped call that SHOULD be detected
    vkDestroyDevice(device, nullptr);
}
