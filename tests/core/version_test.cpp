#include "core/version.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("version is not empty", "[core]") { CHECK_FALSE(chess::core::version().empty()); }
