cmake_minimum_required(VERSION 3.25)

function(add_golem_bench name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE golem)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(${name} PRIVATE
        $<IF:$<CXX_COMPILER_ID:MSVC>,/O2,-O2>
    )
endfunction()

add_golem_bench(bench_vector)
add_golem_bench(bench_optional)
add_golem_bench(bench_variant)
add_golem_bench(bench_unordered_map)
