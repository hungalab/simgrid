if(enable_smpi AND enable_smpi_MPICH3_testsuite)
  set(CMAKE_C_COMPILER "${CMAKE_BINARY_DIR}/smpi_script/bin/smpicc")
  set(CMAKE_Fortran_COMPILER "${CMAKE_BINARY_DIR}/smpi_script/bin/smpiff")

  include_directories(BEFORE "${CMAKE_HOME_DIRECTORY}/include/smpi")
  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../include/")
  foreach(file exitst1 exitst2 finalized initstat library_version version)
    # not compiled files: attrself exitst3 timeout
    add_executable(${file} EXCLUDE_FROM_ALL ${file}.c)
    add_dependencies(tests ${file})
    target_link_libraries(${file} simgrid mtest_c)
  endforeach()
endif()

if (enable_smpi_MPICH3_testsuite AND HAVE_RAW_CONTEXTS)
  ADD_TEST(test-smpi-mpich3-init-raw       ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/teshsuite/smpi/mpich3-test/init ${PERL_EXECUTABLE} ${CMAKE_HOME_DIRECTORY}/teshsuite/smpi/mpich3-test/runtests "-wrapper=${VALGRIND_WRAPPER}" -mpiexec=${CMAKE_BINARY_DIR}/smpi_script/bin/smpirun -srcdir=${CMAKE_HOME_DIRECTORY}/teshsuite/smpi/mpich3-test/init -tests=testlist -execarg=--cfg=contexts/factory:raw)
  SET_TESTS_PROPERTIES(test-smpi-mpich3-init-raw PROPERTIES PASS_REGULAR_EXPRESSION "tests passed!")
endif()

foreach(file attrself exitst1 exitst2 exitst3 finalized initstat library_version timeout version)
  set(examples_src ${examples_src}  ${CMAKE_CURRENT_SOURCE_DIR}/${file}.c)
endforeach()

set(examples_src ${examples_src}                                         PARENT_SCOPE)
set(txt_files    ${txt_files}     ${CMAKE_CURRENT_SOURCE_DIR}/testlist   PARENT_SCOPE)
