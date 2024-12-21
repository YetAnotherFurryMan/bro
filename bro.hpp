#pragma once

#include <iostream>

namespace bro{
	struct Bro{
		inline void logError(const char* msg){
			std::cerr << "BRO ERROR: " << msg << std::endl;
		}
	};
}
