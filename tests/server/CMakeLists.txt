chess_add_test(server_test
    config_test.cpp
    http_routes_test.cpp
    listener_test.cpp
)
target_link_libraries(server_test PRIVATE chess_server_lib)
