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

#include <array>
#include <vector>
#include <future>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
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

		Cmd(const std::string* cmd, std::size_t cmd_size):
			cmd{cmd, cmd + cmd_size}
		{}

		Cmd(const std::vector<std::string>& cmd):
			cmd{cmd}
		{}

		template<std::size_t N>
		Cmd(const std::array<std::string, N>& cmd):
			cmd{cmd.begin(), cmd.end()}
		{}

		inline std::string str(){
			std::stringstream ss;
			for(const auto& e: cmd){
				if(e.find('"') != std::string::npos ||
				   e.find(' ') != std::string::npos)
					ss << " " << std::quoted(e);
				else
					ss << " " << e;
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
		
		CmdTmpl(const std::string* cmd, std::size_t cmd_size):
			cmd{cmd, cmd + cmd_size}
		{}

		CmdTmpl(const std::vector<std::string>& cmd):
			cmd{cmd}
		{}

		template<std::size_t N>
		CmdTmpl(const std::array<std::string, N>& cmd):
			cmd{cmd.begin(), cmd.end()}
		{}

		CmdTmpl(std::string_view ext, const std::string* cmd, std::size_t cmd_size):
			ext{ext},
			cmd{cmd, cmd + cmd_size}
		{}

		CmdTmpl(std::string_view ext, const std::vector<std::string>& cmd):
			ext{ext},
			cmd{cmd}
		{}

		template<std::size_t N>
		CmdTmpl(std::string_view ext, std::array<std::string, N> cmd):
			ext{ext},
			cmd{cmd.begin(), cmd.end()}
		{}

		inline CmdTmpl resolve(std::string_view name, std::string_view value) const {
			CmdTmpl ret = *this;

			std::string v_name = "$";
			v_name += name;

			for(auto& e: ret.cmd){
				auto pos = e.find(v_name);
				while(pos != std::string::npos){
					e.replace(pos, v_name.length(), value);
					pos = e.find(v_name, pos + v_name.length());
				}
			}

			return ret;
		}

		inline CmdTmpl resolve(std::string_view name, const std::vector<std::string>& values) const {
			CmdTmpl ret;
			ret.ext = this->ext;

			std::string v_name = "$";
			v_name += name;

			for(const auto& e: this->cmd){
				auto pos = e.find(v_name);
				if(pos == std::string::npos){
					ret.cmd.push_back(e);
				} else{
					for(const auto& value: values){
						auto p = pos;
						std::string bit = e;
						while(p != std::string::npos){
							bit.replace(p, v_name.length(), value);
							p = bit.find(v_name, p + v_name.length());
						}
						ret.cmd.push_back(bit);
					}
				}
			}

			return ret;
		}

		inline Cmd compile() const {
			return Cmd(resolve("$dollar", "$").cmd);
		}

		inline Cmd compile(const std::unordered_map<std::string, std::vector<std::string>>& vars) const {
			std::vector<std::string> keys;
			for(const auto& [key, val]: vars){
				keys.emplace_back(key);
			}
			std::sort(keys.begin(), keys.end());
			
			CmdTmpl cmd = *this;
			for(const auto& key: keys){
				cmd = cmd.resolve(key, vars.at(key));
			}

			return cmd.compile();
		}

		inline int sync(Log& log) const {
			return compile().sync(log);
		}
		
		inline int sync(Log& log, const std::unordered_map<std::string, std::vector<std::string>>& vars) const {
			return compile(vars).sync(log);
		}

		inline std::future<int> async(Log& log) const {
			return compile().async(log);
		}
		
		inline std::future<int> async(Log& log, const std::unordered_map<std::string, std::vector<std::string>>& vars) const {
			return compile(vars).async(log);
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
		File header;
		File src;
		File exe;
		std::unordered_map<std::string, CmdTmpl> cmds;
		std::unordered_map<std::string, Mod> mods;
		std::unordered_map<std::string_view, std::string_view> flags;
		bool hasExe = false;
		bool hasLib = false;
		bool hasApp = false;

		inline void _setup_default(){
			std::string header_path = __FILE__;
			header = File(header_path);
			flags["cc"] = C_COMPILER_NAME;
			flags["cxx"] = CXX_COMPILER_NAME;
			flags["ld"] = C_COMPILER_NAME;
			flags["ar"] = "ar";
			flags["build"] = "build";

			CmdTmpl lib({std::string(flags["ar"]), "rcs", "$out", "$in"});
			CmdTmpl dll({std::string(flags["ld"]), "$in", "-o", "$out", "-shared"});
			CmdTmpl exe({std::string(flags["ld"]), "$flags", "$in", "-o", "$out"});

			registerCmd("lib", lib);
			registerCmd("dll", dll);
			registerCmd("exe", exe);
		}

		Bro(std::filesystem::path src = __builtin_FILE()):
			src{src}
		{
			_setup_default();
		}
		
		Bro(std::filesystem::path p, std::filesystem::path src = __builtin_FILE()):
			src{src},
			exe{p}
		{
			_setup_default();
		}
		
		Bro(int argc, const char** argv, std::filesystem::path src = __builtin_FILE()):
			src{src},
			exe{argv[0]}
		{
			_setup_default();

			for(size_t i = 1; i < argc; i++){
				std::string_view arg = argv[i];
				auto eq = arg.find('=');
				if(eq != std::string_view::npos){
					std::string_view name = arg.substr(0, eq);
					std::string_view value = arg.substr(eq + 1);
					flags[name] = value;
				} else if(arg[0] == '-'){
					std::string_view name = arg.substr(1, -1);
					flags[name] = "no";
				} else {
					flags[arg] = "yes";
				}
			}
		}

		inline bool isFresh(){
			return !(src > exe || header > exe);
		}

		inline void fresh(){
			if(!isFresh()){
				int ret = 0;

				// Save exe to .old
				ret = exe.copy(log, exe.path.string() + ".old");
				if(ret) std::exit(ret);
				
				// Recompile
				Cmd cmd({std::string(flags["cxx"]), "-o", exe.path, src.path});
				if((ret = cmd.sync(log))){
					log.error("Failed to recompile source: {}", src);
					std::exit(ret);
				}

				// Run
				cmd.cmd.clear();
				cmd.cmd.push_back(exe.path);
				for(const auto& [name, value]: flags)
					cmd.cmd.push_back(std::string(name) + "=" + std::string(value));
				std::exit(cmd.sync(log));
			}
		}

		inline bool hasFlag(std::string_view name){
			return flags.find(name) != flags.end();
		}

		inline std::string getFlag(std::string_view name, std::string_view dflt = ""){
			if(!hasFlag(name))
				return std::string(dflt);
			
			return std::string(flags[name]);
		}

		inline bool setFlag(std::string_view name, std::string_view value = "yes", bool force = true){
			if(!force && hasFlag(name))
				return false;

			flags[name] = value;
			return true;
		}

		inline bool isFlagSet(std::string_view name, bool dflt = false){
			if(!hasFlag(name))
				return dflt;

			return flags[name] != "no" || flags[name] != "0";
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

		inline bool registerCmd(std::string_view name, std::string_view ext, const std::vector<std::string>& cmd){
			return registerCmd(name, CmdTmpl{ext, cmd});
		}

		template<std::size_t N>
		inline bool registerCmd(std::string_view name, std::string_view ext, const std::array<std::string, N>& cmd){
			return registerCmd(name, CmdTmpl{ext, cmd});
		}

		inline bool registerModule(ModType type, std::string_view name){
			std::string n(name);

			if(mods.find(n) != mods.end())
				return true;

			switch(type){
				case ModType::EXE: hasExe = true; break;
				case ModType::LIB: hasLib = true; break;
				case ModType::APP: hasApp = true; break;
			}

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
			std::filesystem::create_directory(flags["build"]);
			if(hasExe) std::filesystem::create_directory(std::string(flags["build"]) + "/bin");
			if(hasLib) std::filesystem::create_directory(std::string(flags["build"]) + "/lib");
			if(hasApp) std::filesystem::create_directory(std::string(flags["build"]) + "/app");

			int ret = 0;

			// TODO: Make CmdPool Runnable and q it with linkage
			CmdPool pool;
			for(auto& [name, mod]: mods){
				log.info("Module {}", name);

				if((ret = mod.dir.copyTree(log, std::string(flags["build"]) + "/obj/" + name)))
					return ret;

				mod.needsLinkage = false;
				mod.objs.clear();
				for(const auto& file: mod.dir.files()){
					std::string ext = file.path.extension();
					if(!ext.empty()) for(const auto& cmd: mod.cmds){
						if(cmds[cmd].ext == ext){
							std::string out = std::string(flags["build"]) + "/obj" + file.path.string().substr(3) + ".o";
							mod.objs.push_back(out);
							if(!(File(out) > file)){
								mod.needsLinkage = true;
								pool.push(cmds[cmd].compile({
											{"out", {out}}, 
											{"in", {file.path.string()}}
										}));
							}

							break;
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
					pool.push(cmds["lib"].compile({
								{"out", {std::string(flags["build"]) + "/lib/lib" + name + ".a"}},
								{"in", mod.objs}
							}));
					pool.push(cmds["dll"].compile({
								{"out", {std::string(flags["build"]) + "/lib/" + name + ".so"}},
								{"in", mod.objs}
							}));
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
					mod.objs.push_back(std::string(flags["build"]) + "/lib/lib" + dep + ".a");

				std::vector<std::string> flags(mod.flags.begin(), mod.flags.end());

				if(mod.needsLinkage){
					pool.push(cmds["exe"].compile({
								{"out", {std::string(this->flags["build"]) + "/bin/" + name}},
								{"in", mod.objs},
								{"flags", flags}
							}));
				}
			}

			if((ret = pool.async(log).wait()))
				return ret;

			pool.clear();

			// Link apps
			for(const auto& [name, mod]: mods){
				if(mod.type != ModType::APP)
					continue;

				if(mod.needsLinkage){
					pool.push(cmds["dll"].compile({
								{"out", {std::string(flags["build"]) + "/app/" + name + ".so"}}, 
								{"in", mod.objs}
							}));
				}
			}

			if((ret = pool.async(log).wait()))
				return ret;

			return 0;
		}

		inline int ninja(std::ostream& out){
			for(const auto& [name, tmpl]: cmds){
				out << "rule " << name << std::endl;
				out << "  command = " << tmpl.compile().str() << std::endl;
				out << std::endl;
			}

			for(auto& [name, mod]: mods){
				mod.objs.clear();
				for(const auto& file: mod.dir.files()){
					std::string ext = file.path.extension();
					if(!ext.empty()) for(const auto& cmd: mod.cmds){
						if(cmds[cmd].ext == ext){
							std::string obj = std::string(flags["build"]) + "/obj" + file.path.string().substr(3) + ".o";
							mod.objs.push_back(obj);
							out << "build " << obj << ": " << cmd << " " << file.path.string() << std::endl;
							out << std::endl;
							break;
						}
					}
				}
			}

			for(const auto& [name, mod]: mods){
				switch(mod.type){
					case ModType::EXE:
					{
						out << "build " << flags["build"] << "/bin/" << name << ": exe";
						
						for(const auto& e: mod.objs) 
							out << " " << e;

						for(const auto& dep: mod.deps)
							out << " " << flags["build"] << "/lib/lib" << dep << ".a";
						
						out << std::endl;

						if(!mod.flags.empty()){
							out << "  flags = ";
							for(const auto& e: mod.flags)
								out << " " << e;
							out << std::endl;
						}
					} break;
					case ModType::LIB:
					{
						out << "build " << flags["build"] << "/lib/lib" << name << ".a: lib";
						for(const auto& e: mod.objs)
							out << " " << e;
						out << std::endl;

						out << "build " << flags["build"] << "/lib/" << name << ".so: dll";
						for(const auto& e: mod.objs)
							out << " " << e;
						out << std::endl;
					} break;
					case ModType::APP:
					{
						out << "build " << flags["build"] << "/app/" << name << ".so: dll";
						for(const auto& e: mod.objs)
							out << " " << e;
						out << std::endl;
					} break;
				}
			}
				
			return 0;
		}

		inline int ninja(){
			std::ofstream out("build.ninja");
			if(!out)
				return 1;

			int ret = ninja(out);

			out.close();

			return ret;
		}

		inline int makefile(std::ostream& out){
			std::unordered_set<std::string> dirs;
			if(hasExe) dirs.insert(std::string(flags["build"]) + "/bin");
			if(hasLib) dirs.insert(std::string(flags["build"]) + "/lib");
			if(hasApp) dirs.insert(std::string(flags["build"]) + "/app");

			out << ".DEFAULT_GOAL: all" << std::endl;
			out << ".MAIN: all" << std::endl;
			out << ".PHONY: default_goal" << std::endl;
			out << "default_goal: all" << std::endl;
			out << std::endl;

			for(auto& [name, mod]: mods){
				mod.objs.clear();
				for(const auto& file: mod.dir.files()){
					std::string ext = file.path.extension();
					if(!ext.empty()) for(const auto& cmd: mod.cmds){
						if(cmds[cmd].ext == ext){
							std::string obj = std::string(flags["build"]) + "/obj" + file.path.string().substr(3) + ".o";
							mod.objs.push_back(obj);
							dirs.insert(obj.substr(0, obj.find_last_of('/')));
							out << obj << ": " << file.path.string() << std::endl;
							out << "\t" << cmds[cmd].compile({{"in", {"$^"}}, {"out", {"$@"}}}).str() << std::endl;
							out << std::endl;
							break;
						}
					}
				}
			}

			for(const auto& [name, mod]: mods){
				out << ".PHONY: " << name << std::endl;
				switch(mod.type){
					case ModType::EXE:
					{
						out << name << ": " << flags["build"] << "/bin/" << name << std::endl;
						out << flags["build"] << "/bin/" << name << ":";
						
						for(const auto& e: mod.objs) 
							out << " " << e;

						for(const auto& dep: mod.deps)
							out << " " << flags["build"] << "/lib/lib" << dep << ".a";
						
						out << std::endl;

						out << "\t" << cmds["exe"].compile({
								{"in", {"$^"}},
								{"out", {"$@"}},
								{"flags", std::vector<std::string>(mod.flags.begin(), mod.flags.end())}}).str() << std::endl;
						out << std::endl;

					} break;
					case ModType::LIB:
					{
						out << name << ": " << flags["build"] << "/lib/lib" << name << ".a " << flags["build"] << "/lib/" << name << ".so" << std::endl;
						out << flags["build"] << "/lib/lib" << name << ".a:";
						for(const auto& e: mod.objs)
							out << " " << e;
						out << std::endl;

						out << "\t" << cmds["lib"].compile({{"in", {"$^"}}, {"out", {"$@"}}}).str() << std::endl;
						out << std::endl;

						out << flags["build"] << "/lib/" << name << ".so:";
						for(const auto& e: mod.objs)
							out << " " << e;
						out << std::endl;
						
						out << "\t" << cmds["dll"].compile({{"in", {"$^"}}, {"out", {"$@"}}}).str() << std::endl;
						out << std::endl;
					} break;
					case ModType::APP:
					{
						out << name << ": " << flags["build"] << "/lib/" << name << ".so" << std::endl;
						out << flags["build"] << "/app/" << name << ".so:";
						for(const auto& e: mod.objs)
							out << " " << e;
						out << std::endl;

						out << "\t" << cmds["dll"].compile({{"in", {"$^"}}, {"out", {"$@"}}}).str() << std::endl;
						out << std::endl;
					} break;
				}
			}

			out << "dirs :=";
			for(const auto& dir: dirs)
				out << " " << dir;
			out << std::endl;
			out << std::endl;
			out << ".PHONY: all" << std::endl;
			out << "all: $(dirs)";
			for(const auto& [name, mod]: mods)
				out << " " << name;
			out << std::endl;
			out << std::endl;
			out << "$(dirs):" << std::endl;
			out << "\tmkdir -p $@" << std::endl;
			out << std::endl;
			out << ".PHONY: clean" << std::endl;
			out << "clean:" << std::endl;
			out << "\t$(RM) -r " << flags["build"] << std::endl;
			out << std::endl;
				
			return 0;
		}

		inline int makefile(){
			std::ofstream out("Makefile");
			if(!out)
				return 1;

			int ret = makefile(out);

			out.close();

			return ret;
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
