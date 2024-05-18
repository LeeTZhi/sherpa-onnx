include_directories(${PROJECT_SOURCE_DIR}/external/mbedtls/include)

add_library(get_cpu_info STATIC get_cpu_info.cc)
target_compile_options(get_cpu_info PRIVATE -fPIC)
target_include_directories(get_cpu_info PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(get_cpu_info_test test_main.cc)
target_link_libraries(get_cpu_info_test get_cpu_info mbedtls mbedcrypto mbedx509)

add_executable(generate_sn_tool generate_sn_tool.cc)
target_link_libraries(generate_sn_tool get_cpu_info mbedtls mbedcrypto mbedx509)

add_executable(sign_sn_tool sign_sn_tool.cc)
target_link_libraries(sign_sn_tool get_cpu_info mbedtls mbedcrypto mbedx509)