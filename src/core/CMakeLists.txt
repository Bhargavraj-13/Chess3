add_library(chess_core STATIC version.cpp)
add_library(chess::core ALIAS chess_core)

# PUBLIC so anything linking chess_core can write #include "core/version.hpp".
target_include_directories(chess_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
chess_set_warnings(chess_core)
