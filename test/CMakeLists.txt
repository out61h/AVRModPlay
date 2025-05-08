cmake_minimum_required(VERSION 3.5)

project(AvrModPlayTest VERSION 0.1 LANGUAGES CXX)

add_executable(${PROJECT_NAME} main.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PROJECT_NAME} PRIVATE AvrModPlay)

if( MSVC )
    set(MSVC_FLAGS 
        /GR-
        /MP
        /MT
        /Wall
        /WX
        /wd4505 # 'function' : unreferenced local function has been removed
        /wd4514 # 'function' : unreferenced inline function has been removed
        /wd4623 # 'derived class' : default constructor was implicitly defined as deleted because a base class default constructor is inaccessible or deleted
        /wd4625 # 'derived class' : copy constructor was implicitly defined as deleted because a base class copy constructor is inaccessible or deleted
        /wd4626 # 'derived class' : assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted
        /wd4710 # 'function' : function not inlined
        /wd4711 # function 'function' selected for inline expansion
        /wd4820 # 'bytes' bytes padding added after construct 'member_name'
        /wd5026 # 'type': move constructor was implicitly defined as deleted
        /wd5027 # 'type': move assignment operator was implicitly defined as deleted
        /wd5045 # Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
    )

    target_compile_options(${PROJECT_NAME} PRIVATE ${MSVC_FLAGS})
endif()
