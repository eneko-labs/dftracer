include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/dftracer-utils.cmake)

install_external_project(cpp-logger 1.0.0 "CPP_LOGGER" https://github.com/hariharan-devarajan/cpp-logger.git v0.0.4 ${CMAKE_INSTALL_PREFIX} "")
install_external_project(gotcha 2.3.2 "gotcha" https://github.com/LLNL/GOTCHA.git 1.0.7 ${CMAKE_INSTALL_PREFIX} "-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
install_external_project(brahma 2.1.0 "BRAHMA" https://github.com/hariharan-devarajan/brahma.git v0.0.10 ${CMAKE_INSTALL_PREFIX} "")
add_dependencies(brahma cpp-logger)
add_dependencies(brahma gotcha)

find_package(yaml-cpp QUIET)

if(NOT ${yaml-cpp_FOUND})
  install_external_project(yaml-cpp "" "YAML_CPP" https://github.com/jbeder/yaml-cpp.git yaml-cpp-0.6.3 ${CMAKE_INSTALL_PREFIX} "-DYAML_CPP_BUILD_TESTS=OFF;-DYAML_BUILD_SHARED_LIBS=ON;-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
  add_dependencies(yaml-cpp brahma)
  add_dependencies(yaml-cpp gotcha)
  add_dependencies(yaml-cpp cpp-logger)
endif()

install_external_project(pybind11 2.12.0 "pybind11" https://github.com/pybind/pybind11.git v2.12.0 ${CMAKE_INSTALL_PREFIX} "-DPYBIND11_TEST=OFF")

if(DFTRACER_WRITER_TYPE MATCHES ".*PROTO.*")
  install_external_project(protobuf 31.1 "protobuf" https://github.com/protocolbuffers/protobuf.git v31.1 ${CMAKE_INSTALL_PREFIX} "-Dprotobuf_BUILD_TESTS=OFF;-Dprotobuf_BUILD_EXAMPLES=OFF;-Dprotobuf_BUILD_LIBPROTOC=ON;-Dprotobuf_BUILD_SHARED_LIBS=ON;-Dprotobuf_WITH_ZLIB=OFF;-DCMAKE_POLICY_VERSION_MINIMUM=3.5")

  ExternalProject_Add(
    perfetto
    PREFIX ${CMAKE_BINARY_DIR}
    GIT_REPOSITORY https://github.com/google/perfetto.git
    GIT_TAG v51.0
    TIMEOUT 10
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON
  )

  set(PERFETTO_OUTPUT_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib64")
  set(PERFETTO_OUTPUT_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")

  set(PERFETTO_SDK_DIR "${CMAKE_BINARY_DIR}/src/perfetto/sdk")
  set(PERFETTO_SDK_HEADER_PATH "${PERFETTO_OUTPUT_INCLUDE_DIR}/perfetto.h")
  set(PERFETTO_SDK_LIB_PATH "${PERFETTO_OUTPUT_LIB_DIR}/libperfetto_sdk.a")

  add_custom_command(
    OUTPUT ${PERFETTO_SDK_LIB_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_OUTPUT_LIB_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_OUTPUT_INCLUDE_DIR}
    COMMAND ${CMAKE_CXX_COMPILER} -std=c++17 -fPIC -c ${PERFETTO_SDK_DIR}/perfetto.cc -I${PERFETTO_SDK_DIR} -o ${CMAKE_BINARY_DIR}/perfetto.o
    COMMAND ${CMAKE_AR} rcs ${PERFETTO_SDK_LIB_PATH} ${CMAKE_BINARY_DIR}/perfetto.o
    COMMAND ${CMAKE_COMMAND} -E copy ${PERFETTO_SDK_DIR}/perfetto.h ${PERFETTO_SDK_HEADER_PATH}
    DEPENDS perfetto
    BYPRODUCTS ${PERFETTO_SDK_HEADER_PATH}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building and placing Perfetto SDK into ${CMAKE_INSTALL_PREFIX}"
    VERBATIM
  )

  add_custom_target(perfetto_sdk ALL DEPENDS ${PERFETTO_SDK_LIB_PATH})
endif()

if(DFTRACER_WRITER_TYPE STREQUAL "PERFETTO_PROTO_ZMQ")
  install_external_project(zeromq 4.3.5 "zeromq" https://github.com/zeromq/libzmq.git v4.3.5 ${CMAKE_INSTALL_PREFIX} "-DBUILD_SHARED=ON;-DENABLE_CURVE=OFF;-DZMQ_BUILD_TESTS=OFF")
  install_external_project(cppzmq 4.10.0 "cppzmq" https://github.com/zeromq/cppzmq.git v4.9.0 ${CMAKE_INSTALL_PREFIX} "-DCPPZMQ_BUILD_TESTS=OFF")
  add_dependencies(cppzmq zeromq)
  add_dependencies(zeromq yaml-cpp)
endif()
