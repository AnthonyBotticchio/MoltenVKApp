#define GLFW_INCLUDE_NONE
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include <GLFW/glfw3.h>


// Include your renderer header
#include "vk_renderer/vk_renderer.hpp" // adjust to your real path/name

static VkDescriptorPool CreateImGuiDescriptorPool( VkDevice device )
{
    // Big-ish pool for ImGui. This is the standard approach in many samples.
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
    };

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    ci.maxSets       = 1000 * (uint32_t)( sizeof( poolSizes ) / sizeof( poolSizes[0] ) );
    ci.poolSizeCount = (uint32_t)( sizeof( poolSizes ) / sizeof( poolSizes[0] ) );
    ci.pPoolSizes    = poolSizes;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult r            = vkCreateDescriptorPool( device, &ci, nullptr, &pool );
    if( r != VK_SUCCESS )
    {
        std::fprintf( stderr, "Failed to create ImGui descriptor pool: %d\n", (int)r );
    }
    return pool;
}

int main()
{
    if( !glfwInit() )
    {
        std::fprintf( stderr, "glfwInit failed\n" );
        return 1;
    }

    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    GLFWwindow* window = glfwCreateWindow( 1280, 720, "ImGui + Vulkan (MoltenVK) macOS", nullptr, nullptr );
    if( !window )
    {
        std::fprintf( stderr, "glfwCreateWindow failed\n" );
        return 1;
    }

    VulkanRenderer renderer;
    if( !renderer.initGlfw( window ) )
    {
        std::fprintf( stderr, "renderer.initGlfw failed\n" );
        return 1;
    }

    // --- ImGui setup ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    ImGui_ImplGlfw_InitForVulkan( window, true );

    VkDescriptorPool imguiPool = CreateImGuiDescriptorPool( renderer.device() );

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance       = renderer.instance();
    initInfo.PhysicalDevice = renderer.physicalDevice();
    initInfo.Device         = renderer.device();
    initInfo.QueueFamily    = renderer.graphicsQueueFamilyIndex();
    initInfo.Queue          = renderer.graphicsQueue();
    initInfo.DescriptorPool = imguiPool;
    initInfo.MinImageCount  = renderer.minImageCount();
    initInfo.ImageCount     = renderer.imageCount();
    initInfo.RenderPass     = renderer.renderPass();
    ImGui_ImplVulkan_Init( &initInfo );

    ImGui_ImplVulkan_CreateFontsTexture();
    vkDeviceWaitIdle( renderer.device() );
    ImGui_ImplVulkan_DestroyFontsTexture();

    // Tell renderer to render ImGui during its render pass
    renderer.setRecordCallback( []( VkCommandBuffer cmd ) { ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), cmd ); } );

    while( !glfwWindowShouldClose( window ) )
    {
        glfwPollEvents();

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Simple UI
        ImGui::Begin( "Hello" );
        ImGui::Text( "ImGui + Vulkan + MoltenVK on macOS" );
        ImGui::End();

        ImGui::Render();

        renderer.drawFrame();
    }

    vkDeviceWaitIdle( renderer.device() );
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool( renderer.device(), imguiPool, nullptr );

    renderer.shutdown();
    glfwDestroyWindow( window );
    glfwTerminate();
    return 0;
}
