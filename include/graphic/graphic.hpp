//
// Created by schrodinger on 9/9/21.
//
#pragma once

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_vulkan.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <exception>
#include <string>
#include <memory>

namespace graphic {

    using WindowImpl = ImGui_ImplVulkanH_Window;

    class StringBaseException : public std::exception {
        std::string msg;
    public:
        explicit StringBaseException(std::string msg) : msg(std::move(msg)) {}

        [[nodiscard]] const char *what() const noexcept override {
            return msg.c_str();
        }
    };

    class VulkanException : public StringBaseException {
    public:
        explicit VulkanException(std::string msg) : StringBaseException(std::move(msg)) {}
    };

    class SDLException : public StringBaseException {
    public:
        explicit SDLException(std::string msg) : StringBaseException(std::move(msg)) {}
    };

    class VulkanContext {
        VkAllocationCallbacks *allocator = nullptr;
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t queue_family = (uint32_t) -1;
        VkQueue queue = VK_NULL_HANDLE;
        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
        VkPipelineCache pipeline_cache{};
        uint32_t min_image_count = 2;
        bool swap_chain_rebuild = true;

    public:
        VulkanContext(const char **extensions, uint32_t extensions_count);

        ~VulkanContext();

        friend class Window;

        friend class GraphicContext;
    };

    class Window {
        VulkanContext &context;
        ImGui_ImplVulkanH_Window window_impl{};

    public:
        ~Window() {
            ImGui_ImplVulkanH_DestroyWindow(context.instance, context.device, &window_impl, context.allocator);
        }

        void render(ImDrawData &draw_data);

        void present();

        friend class GraphicContext;

        Window(VulkanContext &context, VkSurfaceKHR surface, int width, int height, bool unlimited_refresh_rate = true);
    };


    class GraphicContext {
        std::string title;
        SDL_Window *sdl_window{};
        std::unique_ptr<VulkanContext> vk_context{};
        std::unique_ptr<Window> window{};
        VkSurfaceKHR surface{};
        SDL_WindowFlags window_flags = static_cast<SDL_WindowFlags> (SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                                                                     SDL_WINDOW_ALLOW_HIGHDPI);
        int width;
        int height;
        ImGuiIO *io = nullptr;
        ImVec4 clear_color;
        bool finished;

        void render();

    public:
        explicit GraphicContext(int width = 1280, int height = 720, std::string title = "untitled",
                                ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f),
                                bool unlimited_refresh_rate = true);

        template<class Event>
        void run(Event event) {
            while (!finished) {
                SDL_Event sdl_event;
                while (SDL_PollEvent(&sdl_event)) {
                    ImGui_ImplSDL2_ProcessEvent(&sdl_event);
                    if (sdl_event.type == SDL_QUIT)
                        quit();
                    if (sdl_event.type == SDL_WINDOWEVENT && sdl_event.window.event == SDL_WINDOWEVENT_CLOSE &&
                        sdl_event.window.windowID == SDL_GetWindowID(sdl_window))
                        quit();
                }

                // Resize swap chain?
                if (vk_context->swap_chain_rebuild) {
                    SDL_GetWindowSize(sdl_window, &width, &height);
                    if (width > 0 && height > 0) {
                        ImGui_ImplVulkan_SetMinImageCount(vk_context->min_image_count);
                        ImGui_ImplVulkanH_CreateOrResizeWindow(vk_context->instance, vk_context->physical_device,
                                                               vk_context->device,
                                                               &window->window_impl, vk_context->queue_family,
                                                               vk_context->allocator, width,
                                                               height, vk_context->min_image_count);
                        window->window_impl.FrameIndex = 0;
                        vk_context->swap_chain_rebuild = false;
                    }
                }

                // Start the Dear ImGui frame
                ImGui_ImplVulkan_NewFrame();
                ImGui_ImplSDL2_NewFrame();
                ImGui::GetIO().FontGlobalScale = std::fmaxf(floorf(ImGui::GetPlatformIO().Viewports[0]->DpiScale), 1);
                ImGui::NewFrame();
                event(this, &window->window_impl, sdl_window);
                render();
            }
        }

        void quit() {
            finished = true;
        }

        ImVec4 &get_clear_color() {
            return clear_color;
        }

        ~GraphicContext();
    };
}
