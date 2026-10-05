# ============================================================
#  电梯调度仿真 —— CMake 构建脚本
#
#  Windows / MSVC（Visual Studio 生成器）：
#      cmake -S . -B build -A x64
#      cmake --build build --config Release
#      build\Release\elevator.exe              # 不带参数 → 内置演示数据
#      build\Release\elevator.exe data.txt     # 带参数   → 读数据文件
#
#  Windows / Ninja 或 MinGW，Linux / macOS（gcc、clang）：
#      cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#      cmake --build build
#      ./build/elevator
#
#  排查内存问题时（裸指针链表，ASan 很有用）：
#      cmake -S . -B build -DELEVATOR_SANITIZE=ON && cmake --build build
#
#  跑一遍冒烟测试：
#      ctest --test-dir build --output-on-failure
# ============================================================

cmake_minimum_required(VERSION 3.16)

project(elevator LANGUAGES CXX)

# ---------------- C++20 ----------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)     # 用 /std:c++20、-std=c++20，不用 gnu++20

# 单配置生成器（Makefile / Ninja）如果没指定构建类型，默认 Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------- 源文件 ----------------
# 头文件也列进来，纯粹是为了让 IDE 的文件列表完整
set(ELEVATOR_SOURCES
    main.cpp
    Building.cpp
    storey.cpp
    elevator.cpp
)

set(ELEVATOR_HEADERS
    people.h
    enumdata.h
    storey.h
    elevator.h
    Building.h
)

add_executable(elevator ${ELEVATOR_SOURCES} ${ELEVATOR_HEADERS})

# 头文件都在项目根目录，显式声明一下，以后挪文件也不用改 include
target_include_directories(elevator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------- 编译选项 ----------------
if(MSVC)
    # 源文件是 UTF-8（无 BOM），必须告诉编译器：否则中文注释会触发 C4819，
    # 某些字节序列甚至会"吃掉换行"，把后面的代码并进注释里。
    #
    # 这里**只**声明源文件编码，不要用 /utf-8 —— 因为
    #     /utf-8 == /source-charset:utf-8 + /execution-charset:utf-8
    # 后半句会把中文字面量编成 UTF-8 字节，而 Windows 控制台按系统代码页
    # （中文系统是 CP936）去解释，输出就全成乱码（"铓埣壕绮"那种）。
    target_compile_options(elevator PRIVATE
        /source-charset:utf-8
        /W3
        /permissive-          # 严格模式，对应 vcxproj 里的 ConformanceMode
    )
    # 如果哪天要跑在执行字符集不是 CP936 的 Windows 上（中文字面量会被转成 '?'），
    # 就把上面这条换成 /utf-8，并在 main() 开头加 SetConsoleOutputCP(CP_UTF8)。
else()
    # gcc / clang：源文件和执行字符集默认都是 UTF-8，不用额外设置
    target_compile_options(elevator PRIVATE -Wall -Wextra)
endif()

# ---------------- 可选：ASan + UBSan ----------------
option(ELEVATOR_SANITIZE "用地址/未定义行为检查器编译（gcc / clang）" OFF)
if(ELEVATOR_SANITIZE)
    if(MSVC)
        message(STATUS "MSVC: pass /fsanitize=address manually if you need ASan")
    else()
        target_compile_options(elevator PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer -g)
        target_link_options(elevator PRIVATE -fsanitize=address,undefined)
    endif()
endif()

# ---------------- 在 VS 里直接 F5 调试 ----------------
set_target_properties(elevator PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    # 想调试自己的数据文件就打开下面这行（把文件名换成你的）：
    # VS_DEBUGGER_COMMAND_ARGUMENTS "data.txt"
)

# ---------------- 冒烟测试 ----------------
# 不带参数运行会用内置演示数据跑完整轮仿真，退出码 0 即视为通过。
# 它能挡住"崩了""卡死""忘了 return"这类问题。
include(CTest)
if(BUILD_TESTING)
    add_test(NAME smoke COMMAND elevator)
endif()

# ---------------- 以后加单元测试的位置 ----------------
# 想把 c1~c7 那些边界用例固化成文件的话，在这里加：
#     add_executable(smoke_tests tests/smoke_tests.cpp Building.cpp storey.cpp elevator.cpp)
#     add_test(NAME cases COMMAND smoke_tests)
