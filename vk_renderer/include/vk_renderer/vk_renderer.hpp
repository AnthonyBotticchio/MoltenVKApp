#pragma once

#include <cstdint>
#include <functional>
#include <vulkan/vulkan.h>

class VulkanRenderer
{
  public:
    using RecordCallback = std::function<void( VkCommandBuffer )>;

    VulkanRenderer() = default;
    ~VulkanRenderer();

    // nativeLayer is expected to be a CAMetalLayer* (iOS/macOS) but passed as void*
    // to avoid ObjC/ObjC++ types in the header.
    bool init( void* nativeLayer, uint32_t width, uint32_t height );

    // Optional: initialize using a GLFWwindow* (macOS only; GLFW does not exist on iOS).
    // This is compiled only if VK_RENDERER_USE_GLFW is defined.
    bool initGlfw( void* glfwWindow );

    void resize( uint32_t width, uint32_t height );
    void drawFrame();
    void shutdown();

    void setRecordCallback( RecordCallback cb );

    // Getters (useful for ImGui init)
    VkInstance instance() const { return instance_; }

    VkPhysicalDevice physicalDevice() const { return physicalDevice_; }

    VkDevice device() const { return device_; }

    VkQueue graphicsQueue() const { return queue_; }

    uint32_t graphicsQueueFamilyIndex() const { return queueFamilyIndex_; }

    VkRenderPass renderPass() const { return renderPass_; }

    VkCommandPool commandPool() const { return commandPool_; }

    uint32_t imageCount() const { return static_cast<uint32_t>( swapchainImages_.size() ); }

    uint32_t minImageCount() const { return swapchainMinImageCount_; }

  private:
    void createInstanceForMetalSurface();
    void createInstanceForGlfw( void* glfwWindow );

    void createSurfaceFromMetalLayer( void* nativeLayer );
    void createSurfaceFromGlfw( void* glfwWindow );

    void pickPhysicalDevice();
    void createDeviceAndQueues();

    void createSwapchain( uint32_t width, uint32_t height );
    void destroySwapchain();

    void createRenderPass();
    void destroyRenderPass();

    void createFramebuffers();
    void destroyFramebuffers();

    void createCommandResources();
    void destroyCommandResources();

    void createSyncObjects();
    void destroySyncObjects();

    void recordCommandBuffer( uint32_t imageIndex );

  private:
    bool initialized_    = false;
    bool swapchainDirty_ = false;

    uint32_t width_  = 1;
    uint32_t height_ = 1;

    RecordCallback recordCallback_;

    // Vulkan core
    VkInstance instance_             = VK_NULL_HANDLE;
    VkSurfaceKHR surface_            = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_                 = VK_NULL_HANDLE;
    VkQueue queue_                   = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex_       = 0;

    // Swapchain + views
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    uint32_t swapchainMinImageCount_ = 2;

    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;

    // Render pass + framebuffers (needed for ImGui)
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    // Commands
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    // Sync
    VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
    VkSemaphore renderFinished_ = VK_NULL_HANDLE;
    VkFence inFlight_           = VK_NULL_HANDLE;
};
