#pragma once

#include <vector>
#include <future>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <string_view>
#include <source_location>

namespace bro{

// Detect C++ compiler name
inline const std::string_view CXX_COMPILER_NAME = 
#if defined(__clang__)
	"clang++"
#elif defined(__GNUC__)
	"g++"
#elif defined(_MSC_VER)
	"msvc"
#elif defined(__BORLANDC__) || defined(__CODEGEARC__)
	"bcc32"
#else
	"c++"
#endif
;

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

		inline void cmd(std::string_view cmd){
			log("CMD", "{}", cmd);
		}
	};

	struct File{
		bool exists;
		std::filesystem::path path;
		std::filesystem::file_time_type time;

		File() = default;

		File(std::filesystem::path p):
			path{p}
		{
			exists = std::filesystem::exists(p);

			if(exists){
				time = std::filesystem::last_write_time(p);
			}
		}

		inline bool operator>(const File& f){
			return exists && f.exists && this->time > f.time;
		}

		inline bool operator<(const File& f){
			return exists && f.exists && this->time < f.time;
		}

		inline int copy(Log& log, std::filesystem::path to){
			std::error_code ec;
			std::filesystem::copy(path, to, std::filesystem::copy_options::overwrite_existing, ec);
			
			if(ec){
				log.error("Failed to copy from {} to {}: {}", path, to, ec);
				return ec.value();
			}

			return 0;
		}

		inline int move(Log& log, std::filesystem::path to){
			std::error_code ec;
			std::filesystem::rename(path, to, ec);
			
			if(ec){
				log.error("Failed to move from {} to {}: {}", path, to, ec);
				return ec.value();
			}

			return 0;
		}
	};

	struct Runnable{
		inline virtual int sync(Log& log) = 0;
		inline virtual std::future<int> async(Log& log) = 0;
	};

	struct Cmd: public Runnable{
		std::string name;
		std::vector<std::string> cmd;
		
		Cmd(std::string_view name):
			name{name}
		{}

		Cmd(std::string_view name, std::string* cmd, std::size_t cmd_size):
			name{name}
		{
			for(size_t i = 0; i < cmd_size; i++)
				this->cmd.push_back(cmd[i]);
		}

		inline std::string str(){
			std::stringstream ss;
			for(const auto& e: cmd){
				ss << " " << std::quoted(e);
			}

			return ss.str().substr(1);
		}

		inline virtual int sync(Log& log){
			std::string line = str();
			log.cmd(line);
			return system(line.c_str());
		}

		inline virtual std::future<int> async(Log& log){
			std::string line = str();
			log.cmd(line);
			return std::async([](std::string line){
				return system(line.c_str()); 
			}, line);
		}
	};

	struct CmdTmpl{
		std::string name;
		std::vector<std::string> cmd;
		
		CmdTmpl(std::string_view name, std::string* cmd, std::size_t cmd_size):
			name{name}
		{
			for(size_t i = 0; i < cmd_size; i++)
				this->cmd.push_back(cmd[i]);
		}

		inline Cmd compile(std::string_view out, std::string* in, std::size_t in_size){
			Cmd cmd(name);

			for(const auto& e: this->cmd){
				std::size_t pos = 0;
				if((pos = e.find("$in")) != std::string::npos){
					for(std::size_t i = 0; i < in_size; i++){
						std::string tmp = e;
						tmp.replace(pos, 3, in[i]);
						cmd.cmd.push_back(tmp);
					}
				} else if((pos = e.find("$out")) != std::string::npos){
					std::string tmp = e;
					tmp.replace(pos, 4, out);
					cmd.cmd.push_back(tmp);
				} else{
					cmd.cmd.push_back(e);
				}
			}

			return cmd;
		}

		inline Cmd compile(std::string_view out, std::string in){
			return compile(out, &in, 1);
		}

		inline Cmd compile(std::string in){
			return compile("", &in, 1);
		}
		
		inline int sync(Log& log, std::string_view out, std::string* in, std::size_t in_size){
			return compile(out, in, in_size).sync(log);
		}

		inline int sync(Log& log, std::string_view out, std::string in){
			return compile(out, &in, 1).sync(log);
		}

		inline int sync(Log& log, std::string in){
			return compile("", &in, 1).sync(log);
		}

		inline std::future<int> async(Log& log, std::string_view out, std::string* in, std::size_t in_size){
			return compile(out, in, in_size).async(log);
		}

		inline std::future<int> async(Log& log, std::string_view out, std::string in){
			return compile(out, &in, 1).async(log);
		}

		inline std::future<int> async(Log& log, std::string in){
			return compile("", &in, 1).async(log);
		}
	};

	struct CmdPoolAsync: public std::vector<std::future<int>> {
		inline int wait(){
			int ret = 0;
			for(auto& cmd: *this){
				ret += cmd.get();
			}
			return ret;
		}
	};

	struct CmdPool: public std::vector<std::unique_ptr<Runnable>> {
		inline int sync(Log& log){
			int ret = 0;
			for(auto& cmd: *this){
				ret = cmd->sync(log);
				if(ret) return ret;
			}
			return 0;
		}

		inline CmdPoolAsync async(Log& log){
			CmdPoolAsync pool;
			for(auto& cmd: *this){
				pool.push_back(cmd->async(log));
			}
			return pool;
		}

		template<typename T>
		inline void push(const T& obj){
			std::vector<std::unique_ptr<Runnable>>::push_back(std::make_unique<T>(obj));
		}
	};

	struct CmdQueue: public std::vector<std::unique_ptr<Runnable>>, public Runnable {
		inline virtual int sync(Log& log){
			int ret = 0;
			for(auto& cmd: *this){
				ret = cmd->sync(log);
				if(ret) return ret;
			}
			return 0;
		}

		inline virtual std::future<int> async(Log& log){
			return std::async([](Log* log, CmdQueue* q){
				return q->sync(*log);
			}, &log, this);
		}

		template<typename T>
		inline void push(const T& obj){
			std::vector<std::unique_ptr<Runnable>>::push_back(std::make_unique<T>(obj));
		}
	};

	enum class ModType{
		LIB,
		DLL,
		EXE
	};

	struct Mod{
		ModType type;
		std::string_view name;

		Mod(ModType type, std::string_view name):
			type{type},
			name{name}
		{}
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

		inline void fresh(){
			if(!isFresh()){
				int ret = 0;

				// Save exe to .old
				ret = exe.copy(log, exe.path.string() + ".old");
				if(ret) std::exit(ret);
				
				// Recompile
				std::string command[] = {std::string(CXX_COMPILER_NAME), "-o", exe.path, src.path};
				Cmd cmd("fresh", command, 4);
				if((ret = cmd.sync(log))){
					log.error("Failed to recompile source: {}", src);
					std::exit(ret);
				}

				// Run
				cmd.cmd.clear();
				cmd.cmd.push_back(exe.path);
				std::exit(cmd.sync(log));
			}
		}
	};

	template<typename CharT, typename Traits>
	inline std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& out, const File& file){
		auto time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(file.time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
		std::time_t tt = std::chrono::system_clock::to_time_t(time);
		std::tm* tm = std::localtime(&tt);
		return out << "bro::File{'exists': " << file.exists << ", 'path': " << file.path << ", 'time': '" << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << "'}";
	}
}
