/*
 * MIT License
 * 
 * Copyright (c) 2023 YetAnotherFurryMan
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <vector>
#include <future>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace bro{

// Detect C++ compiler name
inline const std::string_view CXX_COMPILER_NAME = 
#if defined(__clang__)
	"clang++"
#elif defined(__GNUC__)
	"g++"
#else
	"c++"
#endif
;

// Detect C compiler name
inline const std::string_view C_COMPILER_NAME = 
#if defined(__clang__)
	"clang"
#elif defined(__GNUC__)
	"gcc"
#else
	"cc"
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
		bool exists = false;
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

		inline bool operator>(const File& f) const {
			return exists && f.exists && this->time > f.time;
		}

		inline bool operator<(const File& f) const {
			return exists && f.exists && this->time < f.time;
		}

		inline int copy(Log& log, std::filesystem::path to) const {
			if(!exists){
				log.error("File does not exist {}", path);
				return 1;
			}

			std::error_code ec;
			std::filesystem::copy(path, to, std::filesystem::copy_options::overwrite_existing, ec);
			
			if(ec){
				log.error("Failed to copy from {} to {}: {}", path, to, ec);
				return ec.value();
			}

			return 0;
		}

		inline int move(Log& log, std::filesystem::path to){
			if(!exists){
				log.error("File does not exist {}", path);
				return 1;
			}

			std::error_code ec;
			std::filesystem::rename(path, to, ec);
			
			if(ec){
				log.error("Failed to move from {} to {}: {}", path, to, ec);
				return ec.value();
			}

			path = to;

			return 0;
		}
	};

	struct Directory: public File{
		Directory() = default;
		Directory(std::filesystem::path p): File{p} {}

		inline int copyTree(Log& log, std::filesystem::path p) const {
			if(!exists){
				log.error("File does not exist {}", path);
				return 1;
			}
			
			std::error_code ec;

			std::filesystem::create_directories(p, ec);

			if(ec){
				log.error("Failed to create directory {}: {}", p, ec);
				return ec.value();
			}

			for(const auto& e: std::filesystem::recursive_directory_iterator(path)){
				if(std::filesystem::is_directory(e.status())){
					std::filesystem::path p2 = p / std::filesystem::relative(e.path(), path);
					std::filesystem::create_directory(p2, ec);

					if(ec){
						log.error("Failed to create directory {}: {}", p2, ec);
						return ec.value();
					}
				}
			}

			return 0;
		}

		inline std::vector<File> files() const {
			std::vector<File> files;
			if(!exists)
				return files;

			for(const auto& e: std::filesystem::recursive_directory_iterator(path)){
				if(std::filesystem::is_regular_file(e.status())){
					files.emplace_back(e.path());
				}
			}

			return files;
		}
	};

	struct Runnable{
		inline virtual int sync(Log& log) = 0;
		inline virtual std::future<int> async(Log& log) = 0;
		virtual ~Runnable() = default;
	};

	struct Cmd: public Runnable{
		std::vector<std::string> cmd;
		
		Cmd() = default;
		Cmd(const std::string* cmd, std::size_t cmd_size)
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

		inline int sync(Log& log) override {
			std::string line = str();
			log.cmd(line);
			return system(line.c_str());
		}

		inline std::future<int> async(Log& log) override {
			std::string line = str();
			log.cmd(line);
			return std::async([](std::string line){
				return system(line.c_str()); 
			}, line);
		}
	};

	struct CmdTmpl{
		std::string ext;
		std::vector<std::string> cmd;

		CmdTmpl() = default;
		
		CmdTmpl(const std::string* cmd, std::size_t cmd_size)
		{
			for(size_t i = 0; i < cmd_size; i++)
				this->cmd.push_back(cmd[i]);
		}

		CmdTmpl(std::string_view ext, const std::string* cmd, std::size_t cmd_size):
			ext{ext}
		{
			for(size_t i = 0; i < cmd_size; i++)
				this->cmd.push_back(cmd[i]);
		}

		inline Cmd compile(std::string_view out, const std::string* in, std::size_t in_size, const std::string* flags = nullptr, std::size_t flags_size = 0) const {
			Cmd cmd;

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
				} else if((pos = e.find("$flags")) != std::string::npos){
					for(std::size_t i = 0; i < flags_size; i++){
						std::string tmp = e;
						tmp.replace(pos, 6, flags[i]);
						cmd.cmd.push_back(tmp);
					}
				} else{
					cmd.cmd.push_back(e);
				}
			}

			return cmd;
		}

		inline Cmd compile(std::string_view out, const std::string& in) const {
			return compile(out, &in, 1);
		}

		inline Cmd compile(const std::string& in) const {
			return compile("", &in, 1);
		}
		
		inline int sync(Log& log, std::string_view out, const std::string* in, std::size_t in_size, const std::string* flags = nullptr, std::size_t flags_size = 0) const {
			return compile(out, in, in_size, flags, flags_size).sync(log);
		}

		inline int sync(Log& log, std::string_view out, const std::string& in) const {
			return compile(out, &in, 1).sync(log);
		}

		inline int sync(Log& log, const std::string& in) const {
			return compile("", &in, 1).sync(log);
		}

		inline std::future<int> async(Log& log, std::string_view out, const std::string* in, std::size_t in_size, const std::string* flags = nullptr, std::size_t flags_size = 0) const {
			return compile(out, in, in_size, flags, flags_size).async(log);
		}

		inline std::future<int> async(Log& log, std::string_view out, const std::string& in) const {
			return compile(out, &in, 1).async(log);
		}

		inline std::future<int> async(Log& log, const std::string& in) const {
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
		inline int sync(Log& log) override {
			int ret = 0;
			for(auto& cmd: *this){
				ret = cmd->sync(log);
				if(ret) return ret;
			}
			return 0;
		}

		inline std::future<int> async(Log& log) override {
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
		EXE,
		LIB,
		APP
	};

	struct Mod{
		ModType type;
		std::string name;
		std::unordered_set<std::string> cmds;
		std::unordered_set<std::string> deps;
		std::unordered_set<std::string> flags;
		Directory dir;

		bool needsLinkage = false;
		std::vector<std::string> objs;

		Mod() = default;

		Mod(ModType type, std::string_view name):
			type{type}, name{name}
		{
			dir = Directory("src/" + this->name);
		}
	};

	struct Bro{
		Log log;
		File src;
		File exe;
		std::vector<std::string_view> args;
		std::unordered_map<std::string, CmdTmpl> cmds;
		std::unordered_map<std::string, Mod> mods;
		std::unordered_map<std::string_view, std::string_view> flags;

		Bro(std::filesystem::path src = __builtin_FILE()):
			src{src}
		{
			flags["cc"] = C_COMPILER_NAME;
			flags["cxx"] = CXX_COMPILER_NAME;
		}
		
		Bro(std::filesystem::path p, std::filesystem::path src = __builtin_FILE()):
			src{src},
			exe{p}
		{
			flags["cc"] = C_COMPILER_NAME;
			flags["cxx"] = CXX_COMPILER_NAME;
		}
		
		Bro(int argc, const char** argv, std::filesystem::path src = __builtin_FILE()):
			src{src},
			exe{argv[0]},
			args{argv + 1, argv + argc}
		{
			flags["cc"] = C_COMPILER_NAME;
			flags["cxx"] = CXX_COMPILER_NAME;

			for(size_t i = 0; i < args.size();){
				auto eq = args[i].find('=');
				if(eq != std::string_view::npos){
					std::string_view name = args[i].substr(0, eq);
					std::string_view value = args[i].substr(eq + 1);
					flags[name] = value;
					args.erase(args.begin() + i);
				} else {
					i++;
				}
			}
		}

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
				std::string command[] = {std::string(flags["cxx"]), "-o", exe.path, src.path};
				Cmd cmd(command, 4);
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

		inline bool registerCmd(std::string_view name, const CmdTmpl& cmd){
			std::string n(name);

			if(cmds.find(n) != cmds.end())
				return true;

			cmds[n] = cmd;
			return false;
		}

		inline bool registerCmd(std::string_view name, std::string_view ext, const std::string* cmd, std::size_t cmd_size){
			return registerCmd(name, CmdTmpl{ext, cmd, cmd_size});
		}

		inline bool registerModule(ModType type, std::string_view name){
			std::string n(name);

			if(mods.find(n) != mods.end())
				return true;

			mods.emplace(n, Mod{type, name});
			return false;
		}

		inline bool use(std::string_view mod, std::string_view cmd){
			std::string m(mod);
			std::string c(cmd);

			if(mods.find(m) == mods.end() || cmds.find(c) == cmds.end())
				return true;

			mods[m].cmds.insert(c);
			return false;
		}

		inline bool dep(std::string_view mod, std::string_view lib){
			std::string m(mod);
			std::string l(lib);

			if(mods.find(m) == mods.end() || mods.find(l) == mods.end() || mods[l].type != ModType::LIB)
				return true;

			mods[m].deps.insert(l);
			return false;
		}

		inline bool link(std::string_view mod, std::string_view flag){
			std::string m(mod);
			std::string f(flag);

			if(mods.find(m) == mods.end())
				return true;

			mods[m].flags.insert(f);

			return false;
		}

		inline int build(){
			// Get rid of g++ for linkage, use gcc and if someone is willing to use C++ will use link function.
			std::string lib_cmd[] = {"ar", "rcs", "$out", "$in"};
			std::string dll_cmd[] = {"g++", "$in", "-o", "$out", "-shared"};
			std::string exe_cmd[] = {"g++", "$flags", "$in", "-o", "$out"};

			CmdTmpl lib(lib_cmd, 4);
			CmdTmpl dll(dll_cmd, 5);
			CmdTmpl exe(exe_cmd, 5);

			std::filesystem::create_directory("build");
			std::filesystem::create_directory("build/bin");
			std::filesystem::create_directory("build/lib");
			std::filesystem::create_directory("build/app");

			int ret = 0;

			// TODO: Make CmdPool Runnable and q it with linkage
			CmdPool pool;
			for(auto& [name, mod]: mods){
				log.info("Module {}", name);

				if((ret = mod.dir.copyTree(log, "build/obj/" + name)))
					return ret;

				mod.needsLinkage = false;
				mod.objs.clear();
				for(const auto& file: mod.dir.files()){
					std::string ext = file.path.extension();
					for(const auto& cmd: mod.cmds){
						if(cmds[cmd].ext == ext){
							std::string out = "build/obj" + file.path.string().substr(3) + ".o";
							mod.objs.push_back(out);
							if(!(File(out) > file)){
								mod.needsLinkage = true;
								pool.push(cmds[cmd].compile(out, file.path.string()));
							}
						}
					}
				}
			}

			if((ret = pool.async(log).wait()))
				return ret;

			pool.clear();

			// Link libs
			for(const auto& [name, mod]: mods){
				if(mod.type != ModType::LIB)
					continue;

				if(mod.needsLinkage){
					pool.push(lib.compile("build/lib/lib" + name + ".a", mod.objs.data(), mod.objs.size()));
					pool.push(dll.compile("build/lib/" + name + ".so", mod.objs.data(), mod.objs.size()));
				}
			}

			if((ret = pool.async(log).wait()))
				return ret;

			pool.clear();

			// Link exes
			for(auto& [name, mod]: mods){
				if(mod.type != ModType::EXE)
					continue;

				for(const auto& dep: mod.deps)
					mod.objs.push_back("build/lib/lib" + dep + ".a");

				std::vector<std::string> flags(mod.flags.begin(), mod.flags.end());

				if(mod.needsLinkage)
					pool.push(exe.compile("build/bin/" + name, mod.objs.data(), mod.objs.size(), flags.data(), flags.size()));
			}

			if((ret = pool.async(log).wait()))
				return ret;

			pool.clear();

			// Link apps
			for(const auto& [name, mod]: mods){
				if(mod.type != ModType::APP)
					continue;

				if(mod.needsLinkage)
					pool.push(dll.compile("build/app/" + name + ".so", mod.objs.data(), mod.objs.size()));
			}

			if((ret = pool.async(log).wait()))
				return ret;

			return 0;
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
