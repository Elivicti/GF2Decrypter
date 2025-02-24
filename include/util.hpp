#pragma once

#include <iostream>
#include <format>

namespace util {

template<typename... Args>
void print(std::ostream& os, std::format_string<Args...> fmt, Args&&... args)
{
	std::format_to(std::ostreambuf_iterator{ os }, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void print(std::format_string<Args...> fmt, Args&&... args)
{
	print(std::cout, fmt, std::forward<Args>(args)...);
}

}

