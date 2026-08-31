cmake_minimum_required(VERSION 3.15)
project(CalcOS
    VERSION 0.1.0
    LANGUAGES C CXX
)

# C11 for _Static_assert and _Thread_local. C++17 for the plugin loader.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# keep binaries and libs separate from source tree
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# SIMD options. AVX2+SSE2 on by default. NEON off by default (ARM cross-compile).
# CALC_TINY_MEM: size-optimize for <64KB targets (Cortex-M, DOS, retro hardware).
option(CALC_ENABLE_AVX2 "Compile with AVX2 instruction set support" ON)
option(CALC_ENABLE_SSE2 "Compile with SSE2 instruction set support" ON)
option(CALC_ENABLE_NEON "Compile with ARM NEON instruction set support" OFF)
option(CALC_TINY_MEM "Compile for ultra-low-memory environments (16KB/24KB/32KB)" OFF)

# glob source and headers for the calc library. explicit lists would be better but
# this codebase has too many files to enumerate. at least it's consistent.
file(GLOB_RECURSE KERNEL_SOURCES
    "Kernel/*.c"
    "Kernel/Adapters/*.c"
    "Kernel/Core/*.c"
    "Kernel/Operations/*.c"
    "Kernel/State/*.c"
)
file(GLOB_RECURSE INFRASTRUCTURE_SOURCES
    "Infrastructure/Utils/*.c"
    "Infrastructure/Config/*.c"
    "Infrastructure/Logging/*.c"
)
file(GLOB_RECURSE APPLICATION_SOURCES
    "Application/Input/*.c"
    "Application/Output/*.c"
    "Application/Interface/*.c"
    "Application/UI/*.c"
)
file(GLOB SRC_SOURCES
    "src/*.c"
)
file(GLOB_RECURSE PLATFORM_SOURCES
    "Platform/x86_64/*.c"
)

set(CALC_SOURCES
    ${KERNEL_SOURCES}
    ${INFRASTRUCTURE_SOURCES}
    ${APPLICATION_SOURCES}
    ${SRC_SOURCES}
    ${PLATFORM_SOURCES}
    src/kernel/wal.c
    src/apps/CalculatorApp.c
    src/kernel/db.c
    src/kernel/disk_marshal.c
    src/kernel/app_registry.c
    src/apps/TetrisApp.c
)

file(GLOB_RECURSE KERNEL_HEADERS
    "Kernel/*.h"
    "Kernel/Adapters/*.h"
    "Kernel/API/*.h"
    "Kernel/Core/*.h"
    "Kernel/Operations/*.h"
    "Kernel/State/*.h"
)
file(GLOB_RECURSE INFRASTRUCTURE_HEADERS
    "Infrastructure/Utils/*.h"
    "Infrastructure/Config/*.h"
)
file(GLOB_RECURSE APPLICATION_HEADERS
    "Application/Input/*.h"
    "Application/Output/*.h"
    "Application/Interface/*.h"
    "Application/UI/*.h"  # was *.c here -- that's a bug, headers only
)
file(GLOB SRC_HEADERS
    "src/*.h"
    "include/calc/*.h"
    "include/calc/*.hpp"
)

set(CALC_HEADERS
    ${KERNEL_HEADERS}
    ${INFRASTRUCTURE_HEADERS}
    ${APPLICATION_HEADERS}
    ${SRC_HEADERS}
)

# ---------------------------------------------------------
# Static Library
# ---------------------------------------------------------
add_library(calc_static STATIC ${CALC_SOURCES} ${CALC_HEADERS})
set_target_properties(calc_static PROPERTIES
    OUTPUT_NAME "calc_static"
    CLEAN_DIRECT_OUTPUT 1
)


# ---------------------------------------------------------
# Shared Library
# ---------------------------------------------------------
add_library(calc_shared SHARED ${CALC_SOURCES} ${CALC_HEADERS})
set_target_properties(calc_shared PROPERTIES
    OUTPUT_NAME "calc"
    CLEAN_DIRECT_OUTPUT 1
)

# ---------------------------------------------------------
# Optional: Pascal Edition Tetris (TetrisAppPascal.pas)
# ---------------------------------------------------------
# OFF by default — Free Pascal isn't a hard dependency of this project.
# Turn it on with `-DCALCOS_ENABLE_PASCAL_TETRIS=ON` to compile the
# Turbo Pascal 7 port of Tetris and link it through the C bridge into
# the same .appreg registry the C apps use.
option(CALCOS_ENABLE_PASCAL_TETRIS "Build TetrisAppPascal.pas (Turbo Pascal edition) via Free Pascal" OFF)
if(CALCOS_ENABLE_PASCAL_TETRIS)
    find_program(FPC_EXECUTABLE fpc)
    if(NOT FPC_EXECUTABLE)
        message(FATAL_ERROR "CALCOS_ENABLE_PASCAL_TETRIS=ON but fpc was not found in PATH")
    endif()
    # -Mtp = Turbo Pascal 7 dialect, -Cg = PIC (links into PIE), -Fu = obj dir
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/TetrisAppPascal.o
        COMMAND ${FPC_EXECUTABLE} -Mtp -Cg
                -Fu${CMAKE_CURRENT_SOURCE_DIR}/src/apps
                -o${CMAKE_CURRENT_BINARY_DIR}/TetrisAppPascal.o
                ${CMAKE_CURRENT_SOURCE_DIR}/src/apps/TetrisAppPascal.pas
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/apps/TetrisAppPascal.pas
        COMMENT "Compiling TetrisAppPascal.pas (Turbo Pascal 7 mode)"
    )
    set(PASCAL_TETRIS_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/TetrisAppPascal.o)
    foreach(target calc_static calc_shared)
        target_sources(${target} PRIVATE
            src/apps/TetrisAppPascalBridge.c
            ${PASCAL_TETRIS_OBJECT}
        )
    endforeach()
endif()

# Include directories configuration
foreach(target calc_static calc_shared)
    # include root so #include paths resolve from project root
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    )

    # This CMakeLists is the host/simulator build (Target A). PLATFORM_HOST
    # keeps kernel_log from falling into bare-metal COM1 port I/O (privileged
    # inb/outb) and selects the host branches in disk_marshal. Embedded targets
    # (ESP32 via idf.py, RPi, WASM) use their own build systems.
    target_compile_definitions(${target} PRIVATE PLATFORM_HOST=1)

    # warnings: W3 on MSVC (W4 breaks third-party headers). Wall+Wextra+Werror on GCC/Clang.
    # -O3: let the compiler do its job. the SIMD dispatch is the bottleneck anyway.
    if(MSVC)
        target_compile_options(${target} PRIVATE /W3 /wd4100 /wd4210)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -O3)
    endif()

    # Apply AVX2 configurations
    # NOTE: On GCC/Clang we deliberately do NOT pass -mavx2 globally. The AVX2
    # kernels use per-function __attribute__((target("avx2"))) and are selected
    # at runtime via CPUID dispatch. A global -mavx2 would let -O3 auto-vectorize
    # the whole binary with AVX2 and SIGILL on older CPUs that lack the feature.
    if(CALC_ENABLE_AVX2)
        target_compile_definitions(${target} PRIVATE USE_AVX2=1)
        if(MSVC)
            target_compile_options(${target} PRIVATE /arch:AVX2)
        endif()
    endif()

    # Apply SSE2 configurations
    if(CALC_ENABLE_SSE2)
        target_compile_definitions(${target} PRIVATE USE_SSE2=1)
        if(MSVC)
            if(CMAKE_SIZEOF_VOID_P EQUAL 4)
                target_compile_options(${target} PRIVATE /arch:SSE2)
            endif()
        endif()
    endif()

    # Apply NEON configurations
    if(CALC_ENABLE_NEON)
        target_compile_definitions(${target} PRIVATE USE_NEON=1)
        if(NOT MSVC)
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "ARM")
                target_compile_options(${target} PRIVATE -mfpu=neon)
            elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "AARCH64")
                target_compile_options(${target} PRIVATE -march=armv8-a+simd)
            endif()
        endif()
    endif()
endforeach()


# ---------------------------------------------------------
# Test Executable
# ---------------------------------------------------------
add_executable(test_stats_rng Tests/Unit/TestStatsRNG.c)
target_link_libraries(test_stats_rng PRIVATE calc_static)
target_include_directories(test_stats_rng PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

add_executable(test_parse Tests/Unit/temp_test_parse.c)
target_link_libraries(test_parse PRIVATE calc_static)
target_include_directories(test_parse PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

add_executable(calc_terminal
    Platform/Host/main_host.c
    Platform/Host/host_term.c
    Platform/Host/host_sdl.c
    Platform/Host/host_input.c
)
target_link_libraries(calc_terminal PRIVATE calc_static)
target_include_directories(calc_terminal PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
# The host platform driver must define PLATFORM_HOST or the kernel's log
# fallback tries bare-metal COM1 port I/O (privileged inb/outb) and SIGSEGVs
# in user space. HOST_APP makes CPUID detection safe for non-bare-metal.
target_compile_definitions(calc_terminal PRIVATE PLATFORM_HOST=1 HOST_APP=1)


add_executable(test_complex_calc Tests/Unit/test_complex_calc.c)
target_link_libraries(test_complex_calc PRIVATE calc_static)
target_include_directories(test_complex_calc PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_definitions(test_complex_calc PRIVATE HOST_APP=1)

add_executable(test_cpp_wrapper Tests/Unit/TestCppWrapper.cpp)
target_link_libraries(test_cpp_wrapper PRIVATE calc_static)
target_include_directories(test_cpp_wrapper PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_definitions(test_cpp_wrapper PRIVATE HOST_APP=1)

add_executable(test_plugin_system Tests/Unit/TestPluginSystem.cpp)
target_link_libraries(test_plugin_system PRIVATE calc_static)
target_include_directories(test_plugin_system PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_definitions(test_plugin_system PRIVATE HOST_APP=1)

# --- Additional unit tests (were in Tests/Unit but never wired into the build) ---
add_executable(test_addition Tests/Unit/TestAddition.c)
target_link_libraries(test_addition PRIVATE calc_static)
target_include_directories(test_addition PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_definitions(test_addition PRIVATE HOST_APP=1)

add_executable(test_subtraction Tests/Unit/TestSubtraction.c)
target_link_libraries(test_subtraction PRIVATE calc_static)
target_include_directories(test_subtraction PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_definitions(test_subtraction PRIVATE HOST_APP=1)

add_executable(test_simd Tests/Unit/TestSIMD.c)
target_link_libraries(test_simd PRIVATE calc_static)
target_include_directories(test_simd PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_definitions(test_simd PRIVATE HOST_APP=1)

add_executable(test_tokenizer Tests/Unit/TestTokenizer.c)
target_link_libraries(test_tokenizer PRIVATE calc_static)
target_include_directories(test_tokenizer PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

add_executable(test_complex Tests/Unit/TestComplex.c)
target_link_libraries(test_complex PRIVATE calc_static)
target_include_directories(test_complex PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_definitions(test_complex PRIVATE HOST_APP=1)

add_executable(test_complex_divzero Tests/Unit/TestComplexDivZero.c)
target_link_libraries(test_complex_divzero PRIVATE calc_static)
target_include_directories(test_complex_divzero PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

add_executable(test_ist_canary Tests/Unit/TestISTCanary.c)
target_link_libraries(test_ist_canary PRIVATE calc_static)
target_include_directories(test_ist_canary PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

add_executable(test_thread_local Tests/Unit/TestThreadLocalSymbols.c)
target_link_libraries(test_thread_local PRIVATE calc_static)
target_include_directories(test_thread_local PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

# Dynamic Plugin Compilation & Staging Target
add_library(sample_plugin SHARED Tests/Unit/SamplePlugin.cpp)
target_link_libraries(sample_plugin PRIVATE calc_static)
target_include_directories(sample_plugin PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
add_custom_command(TARGET sample_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:sample_plugin>/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sample_plugin> $<TARGET_FILE_DIR:sample_plugin>/plugins/
)
add_dependencies(test_plugin_system sample_plugin)

add_executable(test_benchmark Tests/Performance/test_benchmark.c)
target_link_libraries(test_benchmark PRIVATE calc_static)
target_include_directories(test_benchmark PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_definitions(test_benchmark PRIVATE HOST_APP=1)
if(MSVC)
    target_compile_options(test_benchmark PRIVATE /W3 /wd4100 /wd4210)
endif()

# Apply CALC_TINY_MEM size optimizations globally to all targets
if(CALC_TINY_MEM)
    foreach(tgt calc_static calc_shared calc_terminal test_stats_rng test_parse test_complex_calc test_benchmark test_cpp_wrapper test_addition test_subtraction test_simd test_tokenizer test_complex test_complex_divzero test_ist_canary test_thread_local)
        if(TARGET ${tgt})
            target_compile_definitions(${tgt} PRIVATE CALC_TINY_MEM=1)
            if(MSVC)
                target_compile_options(${tgt} PRIVATE /O1 /Os /Gy /Gw)
                target_link_options(${tgt} PRIVATE /OPT:REF)
            else()
                target_compile_options(${tgt} PRIVATE -Os -ffunction-sections -fdata-sections)
                target_link_options(${tgt} PRIVATE -Wl,--gc-sections)
                set_target_properties(${tgt} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
            endif()
        endif()
    endforeach()
endif()
