
##
## cardinal_bench
##
set(CARDINAL_BENCH_SOURCE_FILES
    ${CMAKE_CURRENT_LIST_DIR}/cardinal_bench/cardinal_bench.cpp
)

add_executable(cardinal_bench ${CARDINAL_BENCH_SOURCE_FILES})

if (NOT MSVC)
    # For gcc or clang warning setting
    target_compile_options(cardinal_bench
        PUBLIC
            -std=c++17 -march=native -mtune=native -O3 -DNDEBUG -finput-charset=utf-8 -fPIC -fpic -Wall -Wno-unused-function -Wno-deprecated-declarations -Wno-unused-variable -Wno-deprecated
    )
else()
    # Warning level 3 and all warnings as errors
    target_compile_options(cardinal_bench PUBLIC -O2 -DNDEBUG /W3 /WX)
endif()

target_link_libraries(cardinal_bench
PUBLIC
    ${EXTRA_LIBS}
    ${JSTD_HASHMAP_LIBNAME}
)

target_include_directories(cardinal_bench
PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/cardinal_bench"
    "${CMAKE_CURRENT_LIST_DIR}/../src"
    ${EXTRA_INCLUDES}
)
