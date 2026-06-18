cmake_minimum_required(VERSION 3.10)

# Set 32-bit flags BEFORE project() so CMake detects the correct ABI.
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -m32")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS}    -m32")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")

project(CormanLisp LANGUAGES C CXX)

# Target: 32-bit x86 Linux
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -fno-omit-frame-pointer -fno-strict-aliasing")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fno-strict-aliasing -fpermissive -ffixed-esi -ffixed-edi")

# ---- Include directories ----
set(CORMAN_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/CormanLispServer/include
)

# ---- zlib (static library) ----
set(SKIP_EXAMPLES ON CACHE BOOL "Skip building zlib example binaries")
add_subdirectory(zlib)
set_target_properties(zlibstatic PROPERTIES OUTPUT_NAME z)

# ---- distorm (static library) ----
set(DISTORM_DIR ${CMAKE_SOURCE_DIR}/CormanLispServer/distorm)
set(DISTORM_SRCS
    ${DISTORM_DIR}/src/decoder.c
    ${DISTORM_DIR}/src/distorm.c
    ${DISTORM_DIR}/src/instructions.c
    ${DISTORM_DIR}/src/insts.c
    ${DISTORM_DIR}/src/mnemonics.c
    ${DISTORM_DIR}/src/operands.c
    ${DISTORM_DIR}/src/prefix.c
    ${DISTORM_DIR}/src/textdefs.c
    ${DISTORM_DIR}/src/wstring.c
)
add_library(distorm STATIC ${DISTORM_SRCS})
target_include_directories(distorm PUBLIC
    ${DISTORM_DIR}/include
    ${DISTORM_DIR}/src
)
target_compile_options(distorm PRIVATE -m32 -w)

# ---- CormanLisp shared library ----
set(CORMAN_SERVER_SRCS
    CormanLispServer/src/CharBuf.cpp
    CormanLispServer/src/CoCormanLispStubs.cpp
    CormanLispServer/src/Compx86.cpp
    CormanLispServer/src/CormanLispServer.cpp
    CormanLispServer/src/Gc.cpp
    CormanLispServer/src/Lisp.cpp
    CormanLispServer/src/Lispfunc.cpp
    CormanLispServer/src/LispMath.cpp
    CormanLispServer/src/LispThreadQueue.cpp
    CormanLispServer/src/MemoryReport.cpp
    CormanLispServer/src/Read.cpp
    CormanLispServer/src/Stdafx.cpp
    CormanLispServer/src/ThreadClasses.cpp
    CormanLispServer/src/Unassemble.cpp
    CormanLispServer/src/UserInfo.cpp
    CormanLispServer/src/Write.cpp
)

add_library(cormanlisp SHARED ${CORMAN_SERVER_SRCS})
target_include_directories(cormanlisp PRIVATE
    ${CORMAN_INCLUDE_DIRS}
    ${DISTORM_DIR}/include
    ${DISTORM_DIR}/src
    ${CMAKE_BINARY_DIR}/zlib
)
target_compile_definitions(cormanlisp PRIVATE LINUX _CormanLispServer)
target_link_libraries(cormanlisp PRIVATE zlibstatic distorm pthread dl m)
set_target_properties(cormanlisp PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    LINK_FLAGS "-Wl,--defsym=ThreadQV=_Z8ThreadQVv -Wl,--defsym=EnterGCCriticalSection=_Z22EnterGCCriticalSectionv -Wl,--defsym=LeaveGCCriticalSection=_Z22LeaveGCCriticalSectionv -Wl,--defsym=AllocLocalCons=_Z14AllocLocalConsv -Wl,--defsym=LoadLocalHeap=_Z13LoadLocalHeapv -Wl,--defsym=garbageCollect=_Z14garbageCollectl -Wl,--defsym=ThrowUserException=_Z18ThrowUserExceptionv -Wl,--defsym=Plus=_Z4Plusmz -Wl,--defsym=Minus=_Z5Minusmz -Wl,--defsym=checkFunction=_Z13checkFunctionm"
)

# ---- clconsole (console REPL) ----
add_executable(clconsole clconsole/clconsole.cpp)
target_include_directories(clconsole PRIVATE ${CORMAN_INCLUDE_DIRS})
target_link_libraries(clconsole PRIVATE cormanlisp)

# ---- clboot (minimal bootstrap) ----
add_executable(clboot clboot/clboot.cpp)
target_include_directories(clboot PRIVATE ${CORMAN_INCLUDE_DIRS})
target_compile_definitions(clboot PRIVATE LINUX)
target_link_libraries(clboot PRIVATE cormanlisp)
