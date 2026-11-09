# This folder is only added when BUILD_TESTING is ON, so builds with
# -DBUILD_TESTING=OFF (such as the Docker image) never download Catch2.
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
    GIT_SHALLOW TRUE
    SYSTEM
)
FetchContent_MakeAvailable(Catch2)

# CI's -DCMAKE_COMPILE_WARNING_AS_ERROR=ON applies to every target. Third-party
# warnings are not ours to fix, so they must not be able to fail our build.
set_target_properties(Catch2 Catch2WithMain PROPERTIES COMPILE_WARNING_AS_ERROR OFF)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

# Builds one test program and registers each of its test cases with ctest.
# Catch2 tags become CTest labels, so a test tagged [perft] gets the "perft"
# label that the CI sanitizer jobs skip with `ctest -LE perft`.
function(chess_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE chess::core Catch2::Catch2WithMain)
    chess_set_warnings(${name})
    catch_discover_tests(${name} ADD_TAGS_AS_LABELS TEST_PREFIX "${name}: ")
endfunction()

add_subdirectory(core)
