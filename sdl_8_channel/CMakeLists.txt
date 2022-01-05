cmake_minimum_required(VERSION 3.0)
project(sdl_8ch_sample)

set(CMAKE_CXX_STANDARD 11)

set(TARGET sdl_8ch_sample)
include_directories(/usr/local/include)


#set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
#set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_definitions("-Wno-invalid-source-encoding")
    add_definitions("-O2")
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CMAKE_OSX_SYSROOT "/Library/Developer/CommandLineTools/SDKs/MacOSX11.1.sdk")
    set(CMAKE_CXX_FLAGS "--sysroot ${CMAKE_OSX_SYSROOT} ${CMAKE_CXX_FLAGS}")

    # https://stackoverflow.com/questions/69803659/what-is-the-proper-way-to-build-for-macos-x86-64-using-cmake-on-apple-m1-arm
    set(CMAKE_OSX_ARCHITECTURES "x86_64")   
    add_definitions("-Wno-invalid-source-encoding")
    add_definitions("-fdata-sections -ffunction-sections")
    #    add_definitions("-flto")  # 影响clion 打断点
    #    add_definitions("-Os")
else ()
    message(FATAL_ERROR "unknown CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")
endif ()

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

add_executable(${TARGET} ${SRC})

target_include_directories(${TARGET} PRIVATE /usr/local/include)

target_link_libraries(${TARGET} PRIVATE
        /usr/local/Cellar/sdl2/2.0.12_1/lib/libSDL2.dylib
        )