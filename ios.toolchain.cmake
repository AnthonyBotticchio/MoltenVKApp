# This toolchain file is meant for iOS builds.

set(CMAKE_SYSTEM_NAME iOS) # Automatically sets ${IOS} to 1. 

# Pick a conservative default; adjust if you know your MoltenVK/runtime baseline differs.
set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "iOS deployment target")

# IOS_PLATFORM can be: SIMULATOR or OS (device)
if(NOT DEFINED IOS_PLATFORM)
    set(IOS_PLATFORM "OS" CACHE STRING "SIMULATOR or OS")
endif()

if(IOS_PLATFORM STREQUAL "SIMULATOR")
    set(CMAKE_OSX_SYSROOT "iphonesimulator" CACHE STRING "" FORCE)
    # On Apple Silicon you can do arm64 only; universal sim builds are also possible.
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
    endif()
else()
    set(CMAKE_OSX_SYSROOT "iphoneos" CACHE STRING "" FORCE)
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
    endif()
endif()

# Help Xcode generator behave nicely
set(CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "YES")
