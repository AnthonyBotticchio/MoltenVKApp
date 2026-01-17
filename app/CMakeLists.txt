# --- Executable ---

set(TARGET "app")
add_executable(${TARGET} MACOSX_BUNDLE)

target_link_libraries(${TARGET} PRIVATE
    vk_renderer
)

if(APPLE AND NOT IOS)
    include(FetchContent)

    # GLFW
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.10
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)

    # Dear ImGui (no official CMakeLists in the repo; we build a target ourselves)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.90.9
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    )

    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )

    target_link_libraries(imgui PUBLIC 
        glfw 
        vk_renderer
    )

    add_executable(macos_imgui
        src/macos_imgui/main.cpp
    )

    target_link_libraries(macos_imgui PRIVATE
        vk_renderer
        imgui
        glfw
    )

    target_compile_features(macos_imgui PRIVATE cxx_std_20)
endif()


# --- Sources ---

# Pick platform entrypoint
if(IOS)
    file(GLOB_RECURSE OBJCXX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/ios/*.mm")
    file(GLOB_RECURSE CXX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/ios/*.cpp")
    file(GLOB_RECURSE CXX_PRIVATE_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/macos/*.hpp")
elseif(APPLE)
    file(GLOB_RECURSE OBJCXX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/macos/*.mm")
    file(GLOB_RECURSE CXX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/macos/*.cpp")
    file(GLOB_RECURSE CXX_PRIVATE_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/macos/*.hpp")
endif()

target_sources(${TARGET} PRIVATE
    ${OBJCXX_SOURCES}
    ${CXX_SOURCES}
    ${CXX_PRIVATE_HEADERS}
)

# --- Bundle plist ---

if(IOS)
    set(APP_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist")
elseif(APPLE)
    set(APP_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist")
endif()

if(EXISTS "${APP_PLIST}")
    set_target_properties(${TARGET} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${APP_PLIST}")
endif()

# --- Platform frameworks and Xcode settings ---

if(IOS)
    target_link_libraries(${TARGET} PRIVATE
        "-framework UIKit"
    )

    set_target_properties(${TARGET} PROPERTIES
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.example.VulkanMoltenVKSample"
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
    )
elseif(APPLE)
    target_link_libraries(${TARGET} PRIVATE
        "-framework Cocoa"
    )

    set_target_properties(${TARGET} PROPERTIES
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.example.VulkanMoltenVKSampleMac"
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
    )
endif()
