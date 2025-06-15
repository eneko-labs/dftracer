set_property(GLOBAL PROPERTY COVERAGE_FILES "")
include_directories(${DFTRACER_PROJECT_DIR}/include)

function(df_add_test test_fixture_name) # First argument is the test name
    set(executable_path ${ARGV1}) # Second argument is the executable
    set(executable_args "")

    if(ARGC GREATER 2) # Remaining arguments are for the executable
        list(SUBLIST ARGV 2 -1 executable_args_list)
        set(executable_args ${executable_args_list})
    endif()

    add_test(NAME ${test_fixture_name} COMMAND ${executable_path} ${executable_args})

    set_tests_properties(${test_fixture_name} PROPERTIES # Use test_fixture_name
        ENVIRONMENT "LLVM_PROFILE_FILE=${test_fixture_name}.profraw") # Use test_fixture_name
    get_property(OLD_CF GLOBAL PROPERTY COVERAGE_FILES)

    # Use test_fixture_name for constructing the profraw path
    set(NEW_CF ${OLD_CF} ${CMAKE_CURRENT_BINARY_DIR}/${test_fixture_name}.profraw)
    set_property(GLOBAL PROPERTY COVERAGE_FILES ${NEW_CF})
endfunction()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_cpp cpp/test.cpp)
target_link_libraries(test_cpp ${PROJECT_NAME}_dbg)
add_dependencies(test_cpp ${PROJECT_NAME}_dbg)
add_dependencies(test_cpp ${PROJECT_NAME}_preload_dbg)

add_library(testlib SHARED c/testlib.c)
target_compile_options(testlib PRIVATE ${DFTRACER_FUNCTION_FLAGS})
add_executable(test_c c/test.c)
target_compile_options(test_c PRIVATE ${DFTRACER_FUNCTION_FLAGS})
target_link_libraries(test_c ${PROJECT_NAME}_dbg testlib)
add_dependencies(test_c ${PROJECT_NAME}_dbg)
add_dependencies(test_c ${PROJECT_NAME}_preload_dbg)

add_executable(test_chrome_writer writer/test_chrome_writer.cpp)
target_link_libraries(test_chrome_writer PRIVATE ${PROJECT_NAME}_dbg)

function(set_common_properties test_name)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_LOG_LEVEL=DEBUG)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_TRACE_COMPRESSION=0)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/${DFTRACER_LIBDIR}:${DFTRACER_TEST_LD_LIBRARY_PATH})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/data)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_LOG_FILE=${CMAKE_CURRENT_BINARY_DIR}/${test_name})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_ENABLE=1)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_INC_METADATA=1)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_BIND_SIGNALS=0)
endfunction()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data)

set(chrome_writer_test_name ChromeWriterPerformanceTest)
set(perf_output_file ${CMAKE_CURRENT_BINARY_DIR}/${chrome_writer_test_name}_output.jsonl)
df_add_test(${chrome_writer_test_name} $<TARGET_FILE:test_chrome_writer> ${perf_output_file})
set_common_properties(${chrome_writer_test_name})
set_property(TEST ${chrome_writer_test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_LOG_LEVEL=ERROR)

set(test_name test_cpp_basic_only)
df_add_test(${test_name} ${CMAKE_BINARY_DIR}/bin/test_cpp ${CMAKE_CURRENT_BINARY_DIR}/data)
set_common_properties(${test_name})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/${DFTRACER_LIBDIR}/libdftracer_preload_dbg.so)
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_INIT=PRELOAD)

find_program(BASH_PROGRAM bash)
df_add_test(check_file_exists_${test_name} ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/check_file_at_least.sh ${CMAKE_CURRENT_BINARY_DIR}/${test_name}* 29)
set_tests_properties(check_file_exists_${test_name} PROPERTIES DEPENDS ${test_name})

set(test_name test_cpp_basic_app_only)
df_add_test(${test_name} ${CMAKE_BINARY_DIR}/bin/test_cpp ${CMAKE_CURRENT_BINARY_DIR}/data 1)
set_common_properties(${test_name})

df_add_test(check_file_exists_${test_name} ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/check_file_at_least.sh ${CMAKE_CURRENT_BINARY_DIR}/${test_name}* 29)
set_tests_properties(check_file_exists_${test_name} PROPERTIES DEPENDS ${test_name})

set(test_name test_cpp_basic_app_only_yaml)
df_add_test(${test_name} ${CMAKE_BINARY_DIR}/bin/test_cpp ./data 1)
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_CONFIGURATION=${CMAKE_CURRENT_SOURCE_DIR}/yaml/conf.yaml)

set(test_name test_cpp_basic_app_compress_only)
df_add_test(${test_name} ${CMAKE_BINARY_DIR}/bin/test_cpp ${CMAKE_CURRENT_BINARY_DIR}/data 1)
set_common_properties(${test_name})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_TRACE_COMPRESSION=1)

set(test_name test_cpp_basic_app_disable_only)
df_add_test(${test_name} ${CMAKE_BINARY_DIR}/bin/test_cpp ${CMAKE_CURRENT_BINARY_DIR}/data 1)
set_common_properties(${test_name})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_ENABLE=0)

df_add_test(check_file_exists_${test_name} ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/check_file_not.sh ${CMAKE_CURRENT_BINARY_DIR}/${test_name}* 0)
set_tests_properties(check_file_exists_${test_name} PROPERTIES DEPENDS ${test_name})

set(test_name test_c_basic_only)
df_add_test(${test_name} ${CMAKE_BINARY_DIR}/bin/test_c ${CMAKE_CURRENT_BINARY_DIR}/data)
set_common_properties(${test_name})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/${DFTRACER_LIBDIR}/libdftracer_preload_dbg.so)
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_INIT=PRELOAD)

df_add_test(check_file_exists_${test_name} ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/check_file_at_least.sh ${CMAKE_CURRENT_BINARY_DIR}/${test_name}* 4)
set_tests_properties(check_file_exists_${test_name} PROPERTIES DEPENDS ${test_name})

set(test_name test_c_basic_app_only)
df_add_test(${test_name} ${CMAKE_BINARY_DIR}/bin/test_c ${CMAKE_CURRENT_BINARY_DIR}/data 1)
set_common_properties(${test_name})

df_add_test(check_file_exists_${test_name} ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/check_file_at_least.sh ${CMAKE_CURRENT_BINARY_DIR}/${test_name}* 4)
set_tests_properties(check_file_exists_${test_name} PROPERTIES DEPENDS ${test_name})

set(test_name test_c_basic_app_disable_only)
df_add_test(${test_name} ${CMAKE_BINARY_DIR}/bin/test_c ${CMAKE_CURRENT_BINARY_DIR}/data 1)
set_common_properties(${test_name})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_ENABLE=0)

df_add_test(check_file_exists_${test_name} ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/check_file_not.sh ${CMAKE_CURRENT_BINARY_DIR}/${test_name}* 0)
set_tests_properties(check_file_exists_${test_name} PROPERTIES DEPENDS ${test_name})

set(test_name test_cpp_basic_meta)
df_add_test(${test_name} ${CMAKE_BINARY_DIR}/bin/test_cpp ${CMAKE_CURRENT_BINARY_DIR}/data)
set_common_properties(${test_name})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/${DFTRACER_LIBDIR}/libdftracer_preload.so)
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_INIT=PRELOAD)
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_INC_METADATA=1)

df_add_test(check_file_exists_${test_name} ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/check_file_at_least.sh ${CMAKE_CURRENT_BINARY_DIR}/${test_name}* 29)
set_tests_properties(check_file_exists_${test_name} PROPERTIES DEPENDS ${test_name})

set(test_name test_cpp_basic_affinity)
df_add_test(${test_name} ${CMAKE_BINARY_DIR}/bin/test_cpp ${CMAKE_CURRENT_BINARY_DIR}/data)
set_common_properties(${test_name})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/${DFTRACER_LIBDIR}/libdftracer_preload.so)
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_INIT=PRELOAD)
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_INC_METADATA=1)
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_SET_CORE_AFFINITY=1)

df_add_test(check_file_exists_${test_name} ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/check_file_at_least.sh ${CMAKE_CURRENT_BINARY_DIR}/${test_name}* 29)
set_tests_properties(check_file_exists_${test_name} PROPERTIES DEPENDS ${test_name})

set(test_name test_py_disable_only)
df_add_test(${test_name} ${DFTRACER_PYTHON_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/py/test.py --format=npz --data_dir=${CMAKE_CURRENT_BINARY_DIR})
set_common_properties(${test_name})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT PYTHONPATH=$ENV{PYTHONPATH}:${CMAKE_SOURCE_DIR}/venv/${DFTRACER_LIBDIR})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_ENABLE=0)

df_add_test(check_file_exists_${test_name} ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/check_file_not.sh ${CMAKE_CURRENT_BINARY_DIR}/${test_name}* 0)
set_tests_properties(check_file_exists_${test_name} PROPERTIES DEPENDS ${test_name})

set(test_name test_py_both)
df_add_test(${test_name} ${DFTRACER_PYTHON_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/py/test.py --format=npz --data_dir=${CMAKE_CURRENT_BINARY_DIR}/data)
set_common_properties(${test_name})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT PYTHONPATH=$ENV{PYTHONPATH}:${CMAKE_SOURCE_DIR}/venv/${DFTRACER_LIBDIR})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/${DFTRACER_LIBDIR}:${CMAKE_SOURCE_DIR}/dependency/.spack-env/view/lib64:${DFTRACER_TEST_LD_LIBRARY_PATH})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_LOG_FILE=${CMAKE_CURRENT_BINARY_DIR}/${test_name}_app)
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT LD_PRELOAD=${CMAKE_BINARY_DIR}/${DFTRACER_LIBDIR}/libdftracer_preload.so)
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_DATA_DIR=${CMAKE_BINARY_DIR})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_LOG_FILE=${CMAKE_CURRENT_BINARY_DIR}/${test_name})
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_INIT=PRELOAD)
set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_INC_METADATA=1)

set(TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/util.h)

if(ENABLE_DLIO_BENCHMARK_TESTS)
    set(test_name python_test_py_disable_only)
    df_add_test(${test_name} ${DFTRACER_PYTHON_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/py/python_test.py --format=npz)
    set_common_properties(${test_name})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_ENABLE=0)

    set(test_name python_test_py_disable_io_only)
    df_add_test(${test_name} ${DFTRACER_PYTHON_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/py/python_test.py --format=npz --log_dir=${CMAKE_CURRENT_BINARY_DIR}/${test_name} --data_dir=${CMAKE_CURRENT_BINARY_DIR})
    set_common_properties(${test_name})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_ENABLE=1)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_DISABLE_IO=1)

    set(test_name python_test_py_io_all_only)
    df_add_test(${test_name} ${DFTRACER_PYTHON_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/py/python_test.py --format=npz --log_dir=${CMAKE_CURRENT_BINARY_DIR}/${test_name} --data_dir=${CMAKE_CURRENT_BINARY_DIR})
    set_common_properties(${test_name})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_ENABLE=1)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_DATA_DIR=all)

    set(test_name python_test_py_io_specific_only)
    df_add_test(${test_name} ${DFTRACER_PYTHON_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/py/python_test.py --format=npz --log_dir=${CMAKE_CURRENT_BINARY_DIR}/${test_name} --data_dir=${CMAKE_CURRENT_BINARY_DIR})
    set_common_properties(${test_name})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_ENABLE=1)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR})

    set(test_name python_test_py_io_specific_meta_only)
    df_add_test(${test_name} ${DFTRACER_PYTHON_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/py/python_test.py --format=npz --log_dir=${CMAKE_CURRENT_BINARY_DIR}/${test_name} --data_dir=${CMAKE_CURRENT_BINARY_DIR})
    set_common_properties(${test_name})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_ENABLE=1)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_INC_METADATA=1)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR})

    set(test_name python_test_py_io_all_meta_only)
    df_add_test(${test_name} ${DFTRACER_PYTHON_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/py/python_test.py --format=npz --log_dir=${CMAKE_CURRENT_BINARY_DIR}/${test_name} --data_dir=${CMAKE_CURRENT_BINARY_DIR})
    set_common_properties(${test_name})
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_ENABLE=1)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_INC_METADATA=1)
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT DFTRACER_DATA_DIR=all)
    add_subdirectory(dlio_benchmark)
endif()

if(ENABLE_PAPER_TESTS)
    add_subdirectory(paper)
endif()

add_subdirectory(dfanalyzer)
