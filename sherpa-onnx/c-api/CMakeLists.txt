include_directories(${CMAKE_SOURCE_DIR})
add_library(sherpa-onnx-c-api c-api.cc)
target_link_libraries(sherpa-onnx-c-api sherpa-onnx-core)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(sherpa-onnx-c-api PRIVATE SHERPA_ONNX_BUILD_SHARED_LIBS=1)
  target_compile_definitions(sherpa-onnx-c-api PRIVATE SHERPA_ONNX_BUILD_MAIN_LIB=1)
endif()

install(TARGETS sherpa-onnx-c-api DESTINATION lib)

install(FILES c-api.h
  DESTINATION include/sherpa-onnx/c-api
)

# ASR_API
include_directories(${CMAKE_SOURCE_DIR}/external/mbedtls/include)
include_directories(${CMAKE_SOURCE_DIR}/cpu_info)

add_library(kws-api SHARED get_hardware_info.cc asr_api_impl.cc  )
target_link_libraries(kws-api sherpa-onnx-core get_cpu_info mbedtls mbedcrypto mbedx509  
      -Wl,-Bstatic -lc -lm  -Wl,-Bdynamic onnxruntime)
target_include_directories(kws-api PRIVATE ${CMAKE_BINARY_DIR}/sherpa-onnx/c-api)

install(TARGETS kws-api DESTINATION lib)
install(FILES kws_api.h DESTINATION include/sherpa-onnx/c-api)