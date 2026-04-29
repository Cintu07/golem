cmake_minimum_required(VERSION 3.25)

set(GOLEM_TEST_INCLUDE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
)

function(add_golem_test name src)
    add_executable(${name} ${src})
    target_include_directories(${name} PRIVATE ${GOLEM_TEST_INCLUDE})
    target_link_libraries(${name} PRIVATE golem)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_golem_test(test_optional     unit/test_optional.cpp)
add_golem_test(test_vector       unit/test_vector.cpp)
add_golem_test(test_variant      unit/test_variant.cpp)
add_golem_test(test_unordered_map unit/test_unordered_map.cpp)
