# --- Library ---

set(TARGET "vk_renderer")
add_library(${TARGET} STATIC)

# --- Sources ---

target_include_directories(${TARGET}
    SYSTEM PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

file(GLOB_RECURSE CXX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE CXX_PUBLIC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

target_sources(${TARGET} PRIVATE
    ${CXX_SOURCES}
    ${CXX_PUBLIC_HEADERS}
)

if(UNIX OR APPLE)
    target_compile_options(${TARGET} PUBLIC -fPIC)
    target_compile_definitions(${TARGET} PUBLIC 
        VK_ENABLE_BETA_EXTENSIONS=1
        VK_USE_PLATFORM_METAL_EXT=1
    )
endif()

if(APPLE AND NOT IOS)
    find_package(Vulkan REQUIRED)
    target_link_libraries(${TARGET} PUBLIC
        imgui
        glfw
        Vulkan::Vulkan
    )
    target_compile_definitions(${TARGET} PUBLIC
        VK_RENDERER_USE_GLFW=1
    )
endif()


# --- Vulkan headers + MoltenVK XCFramework ---

set(VULKAN_SDK_ROOT "$ENV{VULKAN_SDK}")
set(VULKAN_SDK_ROOT "$ENV{HOME}/VulkanSDK/1.4.335.1")
message(STATUS "VULKAN_SDK is set to: ${VULKAN_SDK_ROOT}")

set(VULKAN_HEADERS_DIR "${VULKAN_SDK_ROOT}/macOS/include")
if(NOT EXISTS "${VULKAN_HEADERS_DIR}/vulkan/vulkan.h")
    message(FATAL_ERROR "Vulkan headers not found at: ${VULKAN_HEADERS_DIR}")
endif()

target_include_directories(${TARGET} PUBLIC "${VULKAN_HEADERS_DIR}")
message(STATUS "Vulkan headers found at: ${VULKAN_HEADERS_DIR}")

if(NOT DEFINED MOLTENVK_XCFRAMEWORK)
    set(MOLTENVK_XCFRAMEWORK "${VULKAN_SDK_ROOT}/macOS/lib/MoltenVK.xcframework")
endif()

if(NOT EXISTS "${MOLTENVK_XCFRAMEWORK}")
    message(FATAL_ERROR "MoltenVK.xcframework not found at: ${MOLTENVK_XCFRAMEWORK}")
endif()

# Pick correct slice for iOS simulator/device or macOS
set(_mvk_candidates "")
if(IOS)
    if(CMAKE_OSX_SYSROOT MATCHES "iphonesimulator")
        file(GLOB _mvk_candidates "${MOLTENVK_XCFRAMEWORK}/ios-*-simulator*/libMoltenVK.a")
    else()
        file(GLOB _mvk_candidates "${MOLTENVK_XCFRAMEWORK}/ios-arm64/libMoltenVK.a")
    endif()
elseif(APPLE)
    # macOS slice names vary slightly by SDK; prefer static if present, otherwise dylib.
    file(GLOB _mvk_candidates
        "${MOLTENVK_XCFRAMEWORK}/macos-*/libMoltenVK.a"
        "${MOLTENVK_XCFRAMEWORK}/macos-*/libMoltenVK.dylib"
    )
endif()

list(LENGTH _mvk_candidates _mvk_count)
if(_mvk_count EQUAL 0)
    message(FATAL_ERROR "Could not locate libMoltenVK.* inside: ${MOLTENVK_XCFRAMEWORK}")
endif()

list(GET _mvk_candidates 0 MOLTENVK_LIB)
message(STATUS "Using MoltenVK library: ${MOLTENVK_LIB}")

add_library(MoltenVK::MoltenVK UNKNOWN IMPORTED GLOBAL)
set_target_properties(MoltenVK::MoltenVK PROPERTIES IMPORTED_LOCATION "${MOLTENVK_LIB}")

# Link MoltenVK + common Apple frameworks MoltenVK relies on
target_link_libraries(${TARGET} PUBLIC
    # MoltenVK::MoltenVK # THIS CAUSES ERROR????? idk man
    "-framework Foundation"
    "-framework QuartzCore"
    "-framework Metal"
    "-framework CoreGraphics"
    "-framework IOSurface"
)
