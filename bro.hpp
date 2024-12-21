#pragma once

#include <iostream>
#include <string_view>

namespace bro{
	struct Bro{
		inline void format(std::ostream& out, std::string_view fmt){
			out << fmt;
		}

		template<typename T, typename... Args>
		inline void format(std::ostream& out, std::string_view fmt, T first, Args... args){
			if(fmt.empty())
				return;

			std::size_t pos = fmt.find("{}");
			if(pos == std::string_view::npos){
				out << fmt;
			} else{
				out << fmt.substr(0, pos);
				out << first;
				format(out, fmt.substr(pos + 2), args...);
			}
		}

		template<typename... Args>
		inline void logError(std::string_view fmt, Args... args){
			std::cerr << "BRO ERROR: ";
			format(std::cerr, fmt, args...);
			std::cerr << std::endl;
		}
	};
}
