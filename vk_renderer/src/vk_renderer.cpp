#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <vk_renderer/vk_renderer.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vulkan/vulkan_metal.h>

#if defined( VK_RENDERER_USE_GLFW )
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

#define VK_CHECK( expr )                                                                                                                   \
    do                                                                                                                                     \
    {                                                                                                                                      \
        VkResult _vk_result = ( expr );                                                                                                    \
        if( _vk_result != VK_SUCCESS )                                                                                                     \
        {                                                                                                                                  \
            std::fprintf( stderr, "Vulkan error %d at %s:%d\n", (int)_vk_result, __FILE__, __LINE__ );                                     \
            std::abort();                                                                                                                  \
        }                                                                                                                                  \
    } while( 0 )

static bool hasInstanceExtension( const char* name )
{
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties( nullptr, &count, nullptr );
    std::vector<VkExtensionProperties> exts( count );
    vkEnumerateInstanceExtensionProperties( nullptr, &count, exts.data() );

    for( const auto& e : exts )
    {
        if( std::strcmp( e.extensionName, name ) == 0 )
            return true;
    }
    return false;
}

static bool hasDeviceExtension( VkPhysicalDevice pd, const char* name )
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties( pd, nullptr, &count, nullptr );
    std::vector<VkExtensionProperties> exts( count );
    vkEnumerateDeviceExtensionProperties( pd, nullptr, &count, exts.data() );

    for( const auto& e : exts )
    {
        if( std::strcmp( e.extensionName, name ) == 0 )
            return true;
    }
    return false;
}

VulkanRenderer::~VulkanRenderer()
{
    shutdown();
}

bool VulkanRenderer::init( void* nativeLayer, uint32_t width, uint32_t height )
{
    if( initialized_ )
        return true;

    width_  = std::max( 1u, width );
    height_ = std::max( 1u, height );

    createInstanceForMetalSurface();
    createSurfaceFromMetalLayer( nativeLayer );
    pickPhysicalDevice();
    createDeviceAndQueues();
    createSwapchain( width_, height_ );
    createCommandResources();
    createSyncObjects();

    initialized_ = true;
    return true;
}

bool VulkanRenderer::initGlfw( void* glfwWindow )
{
#if !defined( VK_RENDERER_USE_GLFW )
    (void)glfwWindow;
    std::fprintf( stderr, "initGlfw called but VK_RENDERER_USE_GLFW is not enabled.\n" );
    return false;
#else
    if( initialized_ )
        return true;

    GLFWwindow* window = reinterpret_cast<GLFWwindow*>( glfwWindow );
    int fbw            = 0;
    int fbh            = 0;
    glfwGetFramebufferSize( window, &fbw, &fbh );
    width_  = std::max( 1, fbw );
    height_ = std::max( 1, fbh );

    createInstanceForGlfw( glfwWindow );
    createSurfaceFromGlfw( glfwWindow );
    pickPhysicalDevice();
    createDeviceAndQueues();
    createSwapchain( width_, height_ );
    createCommandResources();
    createSyncObjects();

    initialized_ = true;
    return true;
#endif
}

void VulkanRenderer::setRecordCallback( RecordCallback cb )
{
    recordCallback_ = std::move( cb );
}

void VulkanRenderer::resize( uint32_t width, uint32_t height )
{
    width_          = std::max( 1u, width );
    height_         = std::max( 1u, height );
    swapchainDirty_ = true;
}

void VulkanRenderer::shutdown()
{
    if( !initialized_ )
        return;

    vkDeviceWaitIdle( device_ );

    destroySyncObjects();
    destroyCommandResources();
    destroySwapchain();

    if( device_ != VK_NULL_HANDLE )
    {
        vkDestroyDevice( device_, nullptr );
        device_ = VK_NULL_HANDLE;
    }

    if( surface_ != VK_NULL_HANDLE )
    {
        vkDestroySurfaceKHR( instance_, surface_, nullptr );
        surface_ = VK_NULL_HANDLE;
    }

    if( instance_ != VK_NULL_HANDLE )
    {
        vkDestroyInstance( instance_, nullptr );
        instance_ = VK_NULL_HANDLE;
    }

    initialized_ = false;
}

void VulkanRenderer::createInstanceForMetalSurface()
{
    std::vector<const char*> extensions;
    extensions.push_back( VK_KHR_SURFACE_EXTENSION_NAME );
    extensions.push_back( VK_EXT_METAL_SURFACE_EXTENSION_NAME );

    if( hasInstanceExtension( VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) )
    {
        extensions.push_back( VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME );
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "VulkanMoltenVKSample";
    appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
    appInfo.pEngineName        = "None";
    appInfo.engineVersion      = VK_MAKE_VERSION( 1, 0, 0 );
    appInfo.apiVersion         = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = static_cast<uint32_t>( extensions.size() );
    ci.ppEnabledExtensionNames = extensions.data();

    if( std::find( extensions.begin(), extensions.end(), VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) != extensions.end() )
    {
        ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    VK_CHECK( vkCreateInstance( &ci, nullptr, &instance_ ) );
}

void VulkanRenderer::createInstanceForGlfw( void* glfwWindow )
{
#if defined( VK_RENDERER_USE_GLFW )
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>( glfwWindow );
    (void)window;

    if (!glfwVulkanSupported())
    {
        std::fprintf(stderr, "GLFW says Vulkan is NOT supported (loader not found).\n");
        std::abort();
    }

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions( &glfwExtCount );
    if( !glfwExts || glfwExtCount == 0 )
    {
        std::fprintf( stderr, "glfwGetRequiredInstanceExtensions returned none.\n" );
        std::abort();
    }

    std::vector<const char*> extensions;
    extensions.reserve( glfwExtCount + 2 );
    for( uint32_t i = 0; i < glfwExtCount; ++i )
    {
        extensions.push_back( glfwExts[i] );
    }

    if( hasInstanceExtension( VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) )
    {
        extensions.push_back( VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME );
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "VulkanMoltenVKSample";
    appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
    appInfo.pEngineName        = "None";
    appInfo.engineVersion      = VK_MAKE_VERSION( 1, 0, 0 );
    appInfo.apiVersion         = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = static_cast<uint32_t>( extensions.size() );
    ci.ppEnabledExtensionNames = extensions.data();

    if( std::find( extensions.begin(), extensions.end(), VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) != extensions.end() )
    {
        ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    VK_CHECK( vkCreateInstance( &ci, nullptr, &instance_ ) );
#else
    (void)glfwWindow;
#endif
}

void VulkanRenderer::createSurfaceFromMetalLayer( void* nativeLayer )
{
    VkMetalSurfaceCreateInfoEXT sci{};
    sci.sType  = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    sci.pLayer = reinterpret_cast<CAMetalLayer*>( nativeLayer );

    VK_CHECK( vkCreateMetalSurfaceEXT( instance_, &sci, nullptr, &surface_ ) );
}

void VulkanRenderer::createSurfaceFromGlfw( void* glfwWindow )
{
#if defined( VK_RENDERER_USE_GLFW )
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>( glfwWindow );
    VK_CHECK( glfwCreateWindowSurface( instance_, window, nullptr, &surface_ ) );
#else
    (void)glfwWindow;
#endif
}

void VulkanRenderer::pickPhysicalDevice()
{
    uint32_t count = 0;
    VK_CHECK( vkEnumeratePhysicalDevices( instance_, &count, nullptr ) );
    if( count == 0 )
    {
        std::fprintf( stderr, "No Vulkan physical devices found.\n" );
        std::abort();
    }

    std::vector<VkPhysicalDevice> devices( count );
    VK_CHECK( vkEnumeratePhysicalDevices( instance_, &count, devices.data() ) );

    for( auto pd : devices )
    {
        if( !hasDeviceExtension( pd, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) )
            continue;

        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties( pd, &qCount, nullptr );
        std::vector<VkQueueFamilyProperties> qProps( qCount );
        vkGetPhysicalDeviceQueueFamilyProperties( pd, &qCount, qProps.data() );

        for( uint32_t i = 0; i < qCount; ++i )
        {
            if( ( qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) == 0 )
                continue;

            VkBool32 presentSupported = VK_FALSE;
            VK_CHECK( vkGetPhysicalDeviceSurfaceSupportKHR( pd, i, surface_, &presentSupported ) );
            if( presentSupported )
            {
                physicalDevice_   = pd;
                queueFamilyIndex_ = i;
                return;
            }
        }
    }

    std::fprintf( stderr, "No suitable Vulkan physical device found.\n" );
    std::abort();
}

void VulkanRenderer::createDeviceAndQueues()
{
    float prio = 1.0f;

    VkDeviceQueueCreateInfo qci{};
    qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = queueFamilyIndex_;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &prio;

    std::vector<const char*> devExts;
    devExts.push_back( VK_KHR_SWAPCHAIN_EXTENSION_NAME );

    // Use literal to avoid header/version pitfalls
    static constexpr const char* kPortabilitySubset = "VK_KHR_portability_subset";
    if( hasDeviceExtension( physicalDevice_, kPortabilitySubset ) )
    {
        devExts.push_back( kPortabilitySubset );
    }

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount    = 1;
    dci.pQueueCreateInfos       = &qci;
    dci.enabledExtensionCount   = static_cast<uint32_t>( devExts.size() );
    dci.ppEnabledExtensionNames = devExts.data();

    VK_CHECK( vkCreateDevice( physicalDevice_, &dci, nullptr, &device_ ) );
    vkGetDeviceQueue( device_, queueFamilyIndex_, 0, &queue_ );
}

void VulkanRenderer::createSwapchain( uint32_t width, uint32_t height )
{
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physicalDevice_, surface_, &caps ) );

    uint32_t formatCount = 0;
    VK_CHECK( vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice_, surface_, &formatCount, nullptr ) );
    std::vector<VkSurfaceFormatKHR> formats( formatCount );
    VK_CHECK( vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice_, surface_, &formatCount, formats.data() ) );

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for( const auto& f : formats )
    {
        if( f.format == VK_FORMAT_B8G8R8A8_UNORM )
        {
            chosenFormat = f;
            break;
        }
    }

    VkExtent2D extent{};
    if( caps.currentExtent.width != 0xFFFFFFFFu )
    {
        extent = caps.currentExtent;
    }
    else
    {
        extent.width  = std::clamp( width, caps.minImageExtent.width, caps.maxImageExtent.width );
        extent.height = std::clamp( height, caps.minImageExtent.height, caps.maxImageExtent.height );
    }

    uint32_t minImages = std::max( 2u, caps.minImageCount );
    if( caps.maxImageCount > 0 )
    {
        minImages = std::min( minImages, caps.maxImageCount );
    }

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagsKHR supportedAlpha    = caps.supportedCompositeAlpha;
    if( ( supportedAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR ) == 0 )
    {
        const VkCompositeAlphaFlagBitsKHR candidates[] = { VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                                           VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR };
        for( auto c : candidates )
        {
            if( supportedAlpha & c )
            {
                compositeAlpha = c;
                break;
            }
        }
    }

    VkSwapchainCreateInfoKHR sci{};
    sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface          = surface_;
    sci.minImageCount    = minImages;
    sci.imageFormat      = chosenFormat.format;
    sci.imageColorSpace  = chosenFormat.colorSpace;
    sci.imageExtent      = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform     = ( caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
                                                                                                : caps.currentTransform;
    sci.compositeAlpha   = compositeAlpha;
    sci.presentMode      = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped          = VK_TRUE;
    sci.oldSwapchain     = VK_NULL_HANDLE;

    VK_CHECK( vkCreateSwapchainKHR( device_, &sci, nullptr, &swapchain_ ) );

    swapchainFormat_        = chosenFormat.format;
    swapchainExtent_        = extent;
    swapchainMinImageCount_ = minImages;

    uint32_t imageCount = 0;
    VK_CHECK( vkGetSwapchainImagesKHR( device_, swapchain_, &imageCount, nullptr ) );
    swapchainImages_.resize( imageCount );
    VK_CHECK( vkGetSwapchainImagesKHR( device_, swapchain_, &imageCount, swapchainImages_.data() ) );

    swapchainImageViews_.resize( imageCount, VK_NULL_HANDLE );
    for( uint32_t i = 0; i < imageCount; ++i )
    {
        VkImageViewCreateInfo ivci{};
        ivci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image                           = swapchainImages_[i];
        ivci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format                          = swapchainFormat_;
        ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel   = 0;
        ivci.subresourceRange.levelCount     = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount     = 1;

        VK_CHECK( vkCreateImageView( device_, &ivci, nullptr, &swapchainImageViews_[i] ) );
    }

    createRenderPass();
    createFramebuffers();
}

void VulkanRenderer::destroySwapchain()
{
    destroyFramebuffers();
    destroyRenderPass();

    for( auto view : swapchainImageViews_ )
    {
        if( view != VK_NULL_HANDLE )
        {
            vkDestroyImageView( device_, view, nullptr );
        }
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();

    if( swapchain_ != VK_NULL_HANDLE )
    {
        vkDestroySwapchainKHR( device_, swapchain_, nullptr );
        swapchain_ = VK_NULL_HANDLE;
    }

    swapchainFormat_        = VK_FORMAT_UNDEFINED;
    swapchainExtent_        = {};
    swapchainMinImageCount_ = 2;
}

void VulkanRenderer::createRenderPass()
{
    if( renderPass_ != VK_NULL_HANDLE )
        return;

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = swapchainFormat_;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments    = &colorAttachment;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    VK_CHECK( vkCreateRenderPass( device_, &rpci, nullptr, &renderPass_ ) );
}

void VulkanRenderer::destroyRenderPass()
{
    if( renderPass_ != VK_NULL_HANDLE )
    {
        vkDestroyRenderPass( device_, renderPass_, nullptr );
        renderPass_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::createFramebuffers()
{
    framebuffers_.resize( swapchainImageViews_.size(), VK_NULL_HANDLE );

    for( size_t i = 0; i < swapchainImageViews_.size(); ++i )
    {
        VkImageView attachments[] = { swapchainImageViews_[i] };

        VkFramebufferCreateInfo fbci{};
        fbci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass      = renderPass_;
        fbci.attachmentCount = 1;
        fbci.pAttachments    = attachments;
        fbci.width           = swapchainExtent_.width;
        fbci.height          = swapchainExtent_.height;
        fbci.layers          = 1;

        VK_CHECK( vkCreateFramebuffer( device_, &fbci, nullptr, &framebuffers_[i] ) );
    }
}

void VulkanRenderer::destroyFramebuffers()
{
    for( auto fb : framebuffers_ )
    {
        if( fb != VK_NULL_HANDLE )
        {
            vkDestroyFramebuffer( device_, fb, nullptr );
        }
    }
    framebuffers_.clear();
}

void VulkanRenderer::createCommandResources()
{
    if( commandPool_ == VK_NULL_HANDLE )
    {
        VkCommandPoolCreateInfo cpci{};
        cpci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.queueFamilyIndex = queueFamilyIndex_;
        cpci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK( vkCreateCommandPool( device_, &cpci, nullptr, &commandPool_ ) );
    }

    commandBuffers_.resize( swapchainImages_.size(), VK_NULL_HANDLE );

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool        = commandPool_;
    cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = static_cast<uint32_t>( commandBuffers_.size() );
    VK_CHECK( vkAllocateCommandBuffers( device_, &cbai, commandBuffers_.data() ) );
}

void VulkanRenderer::destroyCommandResources()
{
    if( !commandBuffers_.empty() && commandPool_ != VK_NULL_HANDLE )
    {
        vkFreeCommandBuffers( device_, commandPool_, static_cast<uint32_t>( commandBuffers_.size() ), commandBuffers_.data() );
        commandBuffers_.clear();
    }

    if( commandPool_ != VK_NULL_HANDLE )
    {
        vkDestroyCommandPool( device_, commandPool_, nullptr );
        commandPool_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::createSyncObjects()
{
    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK( vkCreateSemaphore( device_, &sci, nullptr, &imageAvailable_ ) );
    VK_CHECK( vkCreateSemaphore( device_, &sci, nullptr, &renderFinished_ ) );

    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK( vkCreateFence( device_, &fci, nullptr, &inFlight_ ) );
}

void VulkanRenderer::destroySyncObjects()
{
    if( inFlight_ != VK_NULL_HANDLE )
    {
        vkDestroyFence( device_, inFlight_, nullptr );
        inFlight_ = VK_NULL_HANDLE;
    }
    if( renderFinished_ != VK_NULL_HANDLE )
    {
        vkDestroySemaphore( device_, renderFinished_, nullptr );
        renderFinished_ = VK_NULL_HANDLE;
    }
    if( imageAvailable_ != VK_NULL_HANDLE )
    {
        vkDestroySemaphore( device_, imageAvailable_, nullptr );
        imageAvailable_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::recordCommandBuffer( uint32_t imageIndex )
{
    VkCommandBuffer cmd = commandBuffers_[imageIndex];

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK( vkBeginCommandBuffer( cmd, &bi ) );

    VkClearValue clear{};
    clear.color.float32[0] = 0.08f;
    clear.color.float32[1] = 0.10f;
    clear.color.float32[2] = 0.18f;
    clear.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass        = renderPass_;
    rpBegin.framebuffer       = framebuffers_[imageIndex];
    rpBegin.renderArea.offset = { 0, 0 };
    rpBegin.renderArea.extent = swapchainExtent_;
    rpBegin.clearValueCount   = 1;
    rpBegin.pClearValues      = &clear;

    vkCmdBeginRenderPass( cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE );

    if( recordCallback_ )
    {
        recordCallback_( cmd );
    }

    vkCmdEndRenderPass( cmd );

    VK_CHECK( vkEndCommandBuffer( cmd ) );
}

void VulkanRenderer::drawFrame()
{
    if( !initialized_ )
        return;

    if( swapchainDirty_ )
    {
        vkDeviceWaitIdle( device_ );

        if( !commandBuffers_.empty() )
        {
            vkFreeCommandBuffers( device_, commandPool_, static_cast<uint32_t>( commandBuffers_.size() ), commandBuffers_.data() );
            commandBuffers_.clear();
        }

        destroySwapchain();
        createSwapchain( width_, height_ );
        createCommandResources();

        swapchainDirty_ = false;
    }

    VK_CHECK( vkWaitForFences( device_, 1, &inFlight_, VK_TRUE, UINT64_MAX ) );
    VK_CHECK( vkResetFences( device_, 1, &inFlight_ ) );

    uint32_t imageIndex = 0;
    VkResult acq        = vkAcquireNextImageKHR( device_, swapchain_, UINT64_MAX, imageAvailable_, VK_NULL_HANDLE, &imageIndex );
    if( acq == VK_ERROR_OUT_OF_DATE_KHR )
    {
        swapchainDirty_ = true;
        return;
    }
    if( acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR )
    {
        VK_CHECK( acq );
    }

    VK_CHECK( vkResetCommandBuffer( commandBuffers_[imageIndex], 0 ) );
    recordCommandBuffer( imageIndex );

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &imageAvailable_;
    si.pWaitDstStageMask    = &waitStage;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &commandBuffers_[imageIndex];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &renderFinished_;

    VK_CHECK( vkQueueSubmit( queue_, 1, &si, inFlight_ ) );

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &renderFinished_;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &swapchain_;
    pi.pImageIndices      = &imageIndex;

    VkResult pres = vkQueuePresentKHR( queue_, &pi );
    if( pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR )
    {
        swapchainDirty_ = true;
        return;
    }
    VK_CHECK( pres );
}
