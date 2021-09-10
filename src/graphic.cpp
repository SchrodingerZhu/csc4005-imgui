//
// Created by schrodinger on 9/9/21.
//
#include <graphic/graphic.hpp>
#include <vector>

namespace {
    inline void check_vulkan_result(VkResult error) {
        if (__builtin_expect(error, 0) != 0) {
            throw graphic::VulkanException("vulkan internal error with" + std::to_string(error));
        }
    }
}

graphic::VulkanContext::VulkanContext(const char **extensions, uint32_t extensions_count) {
    VkResult err;

    // Create Vulkan Instance
    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.enabledExtensionCount = extensions_count;
        create_info.ppEnabledExtensionNames = extensions;
        err = vkCreateInstance(&create_info, allocator, &instance);
        check_vulkan_result(err);
    }

    // Select GPU
    {
        uint32_t gpu_count;
        err = vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr);
        check_vulkan_result(err);
        IM_ASSERT(gpu_count > 0);

        std::vector<VkPhysicalDevice> gpus(gpu_count);
        err = vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.data());
        check_vulkan_result(err);

        // If a number >1 of GPUs got reported, find discrete GPU if present, or use first one available. This covers
        // most common cases (multi-gpu/integrated+dedicated graphics). Handling more complicated setups (multiple
        // dedicated GPUs) is out of scope of this sample.
        int use_gpu = 0;
        for (int i = 0; i < (int) gpu_count; i++) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(gpus[i], &properties);
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                use_gpu = i;
                break;
            }
        }

        physical_device = gpus[use_gpu];
    }

    // Select graphics queue family
    {
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);
        std::vector<VkQueueFamilyProperties> queues(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queues.data());
        for (uint32_t i = 0; i < count; i++)
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                queue_family = i;
                break;
            }
        IM_ASSERT(queue_family != (uint32_t) -1);
    }

    // Create Logical Device (with 1 queue)
    {
        int device_extension_count = 1;
        const char *device_extensions[] = {"VK_KHR_swapchain"};
        const float queue_priority[] = {1.0f};
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = queue_family;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = device_extension_count;
        create_info.ppEnabledExtensionNames = device_extensions;
        err = vkCreateDevice(physical_device, &create_info, allocator, &device);
        check_vulkan_result(err);
        vkGetDeviceQueue(device, queue_family, 0, &queue);
    }

    // Create Descriptor Pool
    {
        VkDescriptorPoolSize pool_sizes[] =
                {
                        {VK_DESCRIPTOR_TYPE_SAMPLER,                1000},
                        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000},
                        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000},
                        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000},
                        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000},
                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000},
                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000},
                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000}
                };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        pool_info.poolSizeCount = (uint32_t) IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(device, &pool_info, allocator, &descriptor_pool);
        check_vulkan_result(err);
    }
}

graphic::VulkanContext::~VulkanContext() {
    vkDestroyDescriptorPool(device, descriptor_pool, allocator);
    vkDestroyDevice(device, allocator);
    vkDestroyInstance(instance, allocator);
}

graphic::Window::Window(graphic::VulkanContext &context, VkSurfaceKHR surface, int width, int height,
                        bool unlimited_refresh_rate)
        : context(context) {
    VkBool32 res;
    window_impl.Surface = surface;
    vkGetPhysicalDeviceSurfaceSupportKHR(context.physical_device, context.queue_family, window_impl.Surface,
                                         &res);
    if (res != VK_TRUE) {
        throw VulkanException("Error no WSI support on physical device");
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
                                                  VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    window_impl.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(context.physical_device,
                                                                      window_impl.Surface,
                                                                      requestSurfaceImageFormat,
                                                                      (size_t) IM_ARRAYSIZE(
                                                                              requestSurfaceImageFormat),
                                                                      requestSurfaceColorSpace);

    // Select Present Mode
    VkPresentModeKHR present_modes_unlimited[] = {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR,
                                                  VK_PRESENT_MODE_FIFO_KHR};
    VkPresentModeKHR present_modes_limited[] = {VK_PRESENT_MODE_FIFO_KHR};

    auto *present_modes = unlimited_refresh_rate ? present_modes_unlimited : present_modes_limited;
    auto array_size = unlimited_refresh_rate ? IM_ARRAYSIZE(present_modes_unlimited) : IM_ARRAYSIZE(
            present_modes_limited);
    window_impl.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(context.physical_device, window_impl.Surface,
                                                                  &present_modes[0],
                                                                  array_size);
    IM_ASSERT(context.min_image_count >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(context.instance, context.physical_device, context.device,
                                           &window_impl, context.queue_family,
                                           context.allocator, width, height, context.min_image_count);
}

void graphic::Window::render(ImDrawData &draw_data) {
    VkResult err;

    VkSemaphore image_acquired_semaphore = window_impl.FrameSemaphores[window_impl.SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = window_impl.FrameSemaphores[window_impl.SemaphoreIndex].RenderCompleteSemaphore;
    err = vkAcquireNextImageKHR(context.device, window_impl.Swapchain, UINT64_MAX, image_acquired_semaphore,
                                VK_NULL_HANDLE,
                                &window_impl.FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        context.swap_chain_rebuild = true;
        return;
    }
    check_vulkan_result(err);

    ImGui_ImplVulkanH_Frame *fd = &window_impl.Frames[window_impl.FrameIndex];
    {
        err = vkWaitForFences(context.device, 1, &fd->Fence, VK_TRUE,
                              UINT64_MAX);    // wait indefinitely instead of periodically checking
        check_vulkan_result(err);

        err = vkResetFences(context.device, 1, &fd->Fence);
        check_vulkan_result(err);
    }
    {
        err = vkResetCommandPool(context.device, fd->CommandPool, 0);
        check_vulkan_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        check_vulkan_result(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = window_impl.RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = window_impl.Width;
        info.renderArea.extent.height = window_impl.Height;
        info.clearValueCount = 1;
        info.pClearValues = &window_impl.ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(&draw_data, fd->CommandBuffer);

    // Submit command buffer
    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        check_vulkan_result(err);
        err = vkQueueSubmit(context.queue, 1, &info, fd->Fence);
        check_vulkan_result(err);
    }
}

void graphic::Window::present() {
    if (context.swap_chain_rebuild)
        return;
    VkSemaphore render_complete_semaphore = window_impl.FrameSemaphores[window_impl.SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &window_impl.Swapchain;
    info.pImageIndices = &window_impl.FrameIndex;
    VkResult err = vkQueuePresentKHR(context.queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        context.swap_chain_rebuild = true;
        return;
    }
    check_vulkan_result(err);
    window_impl.SemaphoreIndex =
            (window_impl.SemaphoreIndex + 1) % window_impl.ImageCount; // Now we can use the next set of semaphores
}


graphic::GraphicContext::GraphicContext(int width, int height, std::string title, ImVec4 clear_color,
                                        bool unlimited_refresh_rate)
        : title(std::move(title)), width(width), height(height), clear_color(clear_color), finished(false) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        throw SDLException(SDL_GetError());
    }
    sdl_window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                                  window_flags);
    uint32_t extensions_count = 0;
    SDL_Vulkan_GetInstanceExtensions(sdl_window, &extensions_count, nullptr);
    const char **extensions = new const char *[extensions_count];
    SDL_Vulkan_GetInstanceExtensions(sdl_window, &extensions_count, extensions);
    vk_context = std::make_unique<VulkanContext>(extensions, extensions_count);
    delete[] extensions;

    if (SDL_Vulkan_CreateSurface(sdl_window, vk_context->instance, &surface) == 0) {
        throw SDLException("Failed to create Vulkan surface.\n");
    }

    {
        int w, h;
        SDL_GetWindowSize(sdl_window, &w, &h);
        window = std::make_unique<Window>(*vk_context, surface, w, h, unlimited_refresh_rate);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForVulkan(sdl_window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vk_context->instance;
    init_info.PhysicalDevice = vk_context->physical_device;
    init_info.Device = vk_context->device;
    init_info.QueueFamily = vk_context->queue_family;
    init_info.Queue = vk_context->queue;
    init_info.PipelineCache = vk_context->pipeline_cache;
    init_info.DescriptorPool = vk_context->descriptor_pool;
    init_info.Allocator = vk_context->allocator;
    init_info.MinImageCount = vk_context->min_image_count;
    init_info.ImageCount = window->window_impl.ImageCount;
    init_info.CheckVkResultFn = check_vulkan_result;
    ImGui_ImplVulkan_Init(&init_info, window->window_impl.RenderPass);

    {
        // Use any command queue
        VkCommandPool command_pool = window->window_impl.Frames[window->window_impl.FrameIndex].CommandPool;
        VkCommandBuffer command_buffer = window->window_impl.Frames[window->window_impl.FrameIndex].CommandBuffer;
        VkResult err;
        err = vkResetCommandPool(vk_context->device, command_pool, 0);
        check_vulkan_result(err);
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        check_vulkan_result(err);

        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &command_buffer;
        err = vkEndCommandBuffer(command_buffer);
        check_vulkan_result(err);
        err = vkQueueSubmit(vk_context->queue, 1, &end_info, VK_NULL_HANDLE);
        check_vulkan_result(err);

        err = vkDeviceWaitIdle(vk_context->device);
        check_vulkan_result(err);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

}


graphic::GraphicContext::~GraphicContext() {
    auto err = vkDeviceWaitIdle(vk_context->device);
    check_vulkan_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    window.reset();
    vk_context.reset();

    SDL_DestroyWindow(sdl_window);
    SDL_Quit();
}

void graphic::GraphicContext::render() {
    ImGui::Render();
    ImDrawData *draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (!is_minimized) {
        window->window_impl.ClearValue.color.float32[0] = clear_color.x * clear_color.w;
        window->window_impl.ClearValue.color.float32[1] = clear_color.y * clear_color.w;
        window->window_impl.ClearValue.color.float32[2] = clear_color.z * clear_color.w;
        window->window_impl.ClearValue.color.float32[3] = clear_color.w;
        window->render(*draw_data);
        window->present();
    }
}
