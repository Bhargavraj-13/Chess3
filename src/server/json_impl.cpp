// Boost.JSON is used header-only, so its compiled code has to live in exactly
// one .cpp file of the program. This is that file. Never include
// <boost/json/src.hpp> anywhere else, or the linker will see duplicates.
#include <boost/json/src.hpp>
