#pragma once

#include <vector>
#include <iostream>
#include <filesystem>
#include <string_view>
#include <source_location>

namespace bro{
	struct Log{
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
		inline void log(std::string_view prefix, std::string_view fmt, Args... args){
			std::cerr << prefix << ": ";
			format(std::cerr, fmt, args...);
			std::cerr << std::endl;
		}

		template<typename... Args>
		inline auto error(std::string_view fmt, Args&&... args) -> decltype(log<Args...>("ERROR", fmt, std::forward<Args>(args)...)){
			return log<Args...>("ERROR", fmt, std::forward<Args>(args)...);
		}

		template<typename... Args>
		inline auto warning(std::string_view fmt, Args&&... args) -> decltype(log<Args...>("WARNING", fmt, std::forward<Args>(args)...)){
			return log<Args...>("WARNING", fmt, std::forward<Args>(args)...);
		}

		template<typename... Args>
		inline auto info(std::string_view fmt, Args&&... args) -> decltype(log<Args...>("INFO", fmt, std::forward<Args>(args)...)){
			return log<Args...>("INFO", fmt, std::forward<Args>(args)...);
		}
	};

	struct File{
		std::filesystem::path path;
		std::filesystem::file_time_type time;

		File() = default;

		File(std::filesystem::path p):
			path{p}, time{std::filesystem::last_write_time(p)}
		{}

		inline bool operator>(const File& f){
			return this->time > f.time;
		}

		inline bool operator<(const File& f){
			return this->time < f.time;
		}
	};

	struct Bro{
		Log log;
		File src;
		File exe;
		std::vector<std::string_view> args;

		Bro(std::filesystem::path src = __builtin_FILE()):
			src{src}
		{}
		
		Bro(std::filesystem::path p, std::filesystem::path src = __builtin_FILE()):
			src{src},
			exe{p}
		{}
		
		Bro(int argc, const char** argv, std::filesystem::path src = __builtin_FILE()):
			src{src},
			exe{argv[0]},
			args{argv + 1, argv + argc}
		{}

		inline bool isFresh(){
			return !(src > exe);
		}
	};

	template<typename CharT, typename Traits>
	inline std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& out, const File& file){
		auto time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(file.time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
		std::time_t tt = std::chrono::system_clock::to_time_t(time);
		std::tm* tm = std::localtime(&tt);
		return out << "bro::File{'path': " << file.path << ", 'time': '" << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << "'}";
	}
}
