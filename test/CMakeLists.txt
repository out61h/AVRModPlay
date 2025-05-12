cmake_minimum_required(VERSION 3.19)

include(CTest)

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

file(GLOB mod_files "${CMAKE_SOURCE_DIR}/extras/songs/mod/*.mod")
foreach(mod_file ${mod_files})
    # file download <URL> + check md5
    get_filename_component (mod_name ${mod_file} NAME)
    file(MD5 ${mod_file} mod_md5)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/hash/${mod_name}.md5" precalc_mod_md5)
    if(NOT ${mod_md5} STREQUAL ${precalc_mod_md5})
        message(FATAL_ERROR "The MD5 hash of the MOD file doesn't match the reference value.")
    endif()

    set(precalc_wav_md5_file "${CMAKE_CURRENT_SOURCE_DIR}/hash/${mod_name}.wav.md5")
    if(EXISTS ${precalc_wav_md5_file})
        file(READ ${precalc_wav_md5_file} precalc_wav_md5)
    endif()
    
    add_test(NAME "PLAY: ${mod_name}"
            COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/run-test.py"
                            -c ${precalc_wav_md5}
                            -e $<TARGET_FILE:${PROJECT_NAME}>
                            -i ${mod_file})
endforeach()
