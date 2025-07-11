/*
 * MIT License
 * 
 * Copyright (c) 2023 Damian Satoła
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

// TODO: Write an insert function for Dictionary and for stage API
// TODO: Batch size
// TODO: Mercurial and Git support
// TODO: Unhardcode build directory for Transform and Link (or Stage)
// TODO: Test if bro::Link has any cmds.
// TODO: Tests
// TODO: enable(FLGName) and cmd variants
// TODO: Resolving depth in String
// TODO: Bridge

#pragma once

// Major version macro
#ifndef BRO_VERSION_MAJOR
#define BRO_VERSION_MAJOR 2
#endif

#include <array>
#include <vector>
#include <future>
#include <limits>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <unordered_set>
#include <unordered_map>

namespace bro{

inline const std::string_view VERSION = "2.0";

// Detect C++ compiler name
inline const std::string_view CXX_COMPILER_NAME = 
#if defined(__clang__)
	"clang++"
#elif defined(__GNUC__)
	"g++"
#else
	"c++"
#define BRO_NO_STD_FLAG
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

// Detect C++ std flag
inline const std::string_view CXX_STD_FLAG = 
#if defined(BRO_STD_FLAG)
	BRO_STD_FLAG
#elif defined(BRO_NO_STD_FLAG)
	"-DBRO_NO_STD_FLAG"
#elif __cplusplus < 202202L
	"-std=c++17"
#elif __cplusplus < 202302L
	"-std=c++20"
#else
	// If version could not be detected, use the newest one.
	"-std=c++23"
#endif
;

	inline bool starts_with(const std::string& a, std::string_view b){
		// If C++20 or later use standard starts_with function 
#if __cplusplus < 202002L
		if(a.length() < b.length())
			return false;
	
		for(std::size_t i = 0; i < b.length(); i++)
			if(a[i] != b[i])
				return false;
	
		return true;
#else
		return a.starts_with(b);
#endif
	}

	/*
	 * Helper std::string wrapper that includes escaping and ${template} resolving
	 */
	struct String: public std::string {
		String() = default;
		
		String(std::string_view str):
			std::string{str}
		{}

		String(const std::string& str):
			std::string{str}
		{}

		String(const char* str):
			std::string{str}
		{}

		String(const std::filesystem::path str):
			std::string{str}
		{}

		String escape() const {
			const std::string needs = "\" \n\r";
			if(find_first_of(needs) != std::string::npos){
				std::stringstream ss;
				ss << std::quoted(*this);
				return ss.str();
			}

			return *this;
		}

		std::vector<String> resolve(const std::unordered_map<std::string, std::vector<std::string>> dict, std::string::size_type pos = 0) const {
			if(pos >= size())
				return {*this};

			std::string::size_type dollar = find('$', pos);
			if(dollar == std::string::npos)
				return {*this};

			if(dollar + 1 >= size())
				return {*this};

			if(at(dollar + 1) == '$'){
				String s = *this;
				s.replace(dollar, 2, "$");
				return s.resolve(dict, dollar + 2);
			}

			if(at(dollar + 1) != '{')
				return resolve(dict, dollar + 1);

			std::string::size_type end = find('}', dollar);
			if(end == std::string::npos)
				return {*this};

			std::string var = substr(dollar + 2, end - dollar - 2);
			const auto vals = dict.find(var);
			if(vals == dict.end() || (*vals).second.size() == 0){
				String s = *this;
				s.replace(dollar, end - dollar + 1, "");
				return s.resolve(dict, dollar);
			}

			std::vector<String> ret;
			for(const auto& val: (*vals).second){
				String s = *this;
				s.replace(dollar, end - dollar + 1, val);
				const auto& variants = s.resolve(dict, dollar + val.size() + 1);
				ret.insert(ret.end(), variants.begin(), variants.end());
			}

			return ret;
		}

		// Returns a set of ${template} variables used in the String
		std::unordered_set<std::string> variables() const {
			std::unordered_set<std::string> ret;

			std::string::size_type pos = 0;
			while((pos = find('$', pos)) != std::string::npos){
				if(pos + 1 >= size() || at(pos + 1) != '{'){
					pos += 2;
					continue;
				}

				std::string::size_type end = find('}', pos);
				if(end == std::string::npos)
					return ret;

				ret.emplace(substr(pos + 2, end - pos - 2));
				pos = end;
			}

			return ret;
		}
	};

	/*
	 * Helper std::unordered_map wrapper that uses linear storage (std::vector).
	 * It allows to access the data by both key and index.
	 */
	template <typename K, typename V>
	struct Dictionary: public std::vector<V>{
		std::unordered_map<K, std::size_t> dict;

		inline V& operator[](const K& ix){
			if(dict.find(ix) == dict.end()){
				std::size_t i = std::vector<V>::size();
				std::vector<V>::emplace_back();
				dict[ix] = i;
				return std::vector<V>::operator[](i);
			}

			return std::vector<V>::operator[](dict[ix]);
		}

		inline V& operator[](const std::size_t ix){
			return std::vector<V>::operator[](ix);
		}

		// Create a key alias for an index
		inline bool alias(const K& ix1, std::size_t ix2){
			if(ix2 >= std::vector<V>::size())
				return true;

			dict[ix1] = ix2;

			return false;
		}

		inline bool alias(const K& ix1, const K& ix2){
			if(dict.find(ix2) == dict.end())
				return true;

			return alias(ix1, dict[ix2]);
		}

		inline typename std::vector<V>::iterator find(const K& ix){
			if(dict.find(ix) == dict.end())
				return std::vector<V>::end();

			return std::vector<V>::begin() + dict[ix];
		}

		template<typename... Args>
		inline std::pair<std::size_t, V&> emplace(const K& ix, Args&&... args){
			if(dict.find(ix) != dict.end()){
				std::size_t i = dict[ix];
				V& ref = (std::vector<V>::operator[](i) = V(std::forward<Args>(args)...));
				return std::pair<std::size_t, V&>(i, ref);
			}

			std::size_t i = std::vector<V>::size();
			V& ref = std::vector<V>::emplace_back(std::forward<Args>(args)...);
			dict[ix] = i;
			return std::pair<std::size_t, V&>(i, ref);
		}
	};

	/*
	 * Helper struct used for logging.
	 */
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

	/*
	 * Hepler std::filesystem::path wrapper that stores info about the given path.
	 * It stores if the path exists and if so, what is its last wirite time.
	 * Provides funcionality for basic file operations like copy and move.
	 */
	struct File: public std::filesystem::path{
		bool exists = false;
		std::filesystem::file_time_type time;

		File() = default;

		File(std::filesystem::path p):
			std::filesystem::path{p}
		{
			exists = std::filesystem::exists(p);

			if(exists){
				time = std::filesystem::last_write_time(p);
			}
		}

		constexpr const std::filesystem::path& path() const {
			return *this;
		}

		inline bool operator>(const File& f) const {
			return exists && f.exists && this->time > f.time;
		}

		inline bool operator<(const File& f) const {
			return exists && f.exists && this->time < f.time;
		}

		inline int copy(Log& log, std::filesystem::path to) const {
			if(!exists){
				log.error("File does not exist {}", path());
				return 1;
			}

			std::error_code ec;
			std::filesystem::copy(*this, to, std::filesystem::copy_options::overwrite_existing, ec);
			
			if(ec){
				log.error("Failed to copy from {} to {}: {}", path(), to, ec);
				return ec.value();
			}

			return 0;
		}

		inline int move(Log& log, std::filesystem::path to){
			if(!exists){
				log.error("File does not exist {}", path());
				return 1;
			}

			std::error_code ec;
			std::filesystem::rename(path(), to, ec);
			
			if(ec){
				log.error("Failed to move from {} to {}: {}", path(), to, ec);
				return ec.value();
			}

			*this = to;

			return 0;
		}
	};

	/*
	 * Wrapper for File made for directories.
	 * Provides functionality for copying the directory tree and listing files.
	 * The existence of the directory can be ensured by make function.
	 */
	struct Directory: public File{
		Directory() = default;
		Directory(std::filesystem::path p): File{p} {}

		inline int copyTree(Log& log, std::filesystem::path p) const {
			log.info("Copying tree {} => {}", path(), p);

			if(!exists){
				log.error("File does not exist {}", path());
				return 1;
			}
			
			std::error_code ec;

			std::filesystem::create_directories(p, ec);

			if(ec){
				log.error("Failed to create directory {}: {}", p, ec);
				return ec.value();
			}

			for(const auto& e: std::filesystem::recursive_directory_iterator(path())){
				if(std::filesystem::is_directory(e.status())){
					std::filesystem::path p2 = p / std::filesystem::relative(e.path(), path());
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

			for(const auto& e: std::filesystem::recursive_directory_iterator(path())){
				if(std::filesystem::is_regular_file(e.status())){
					files.emplace_back(e.path());
				}
			}

			return files;
		}

		inline bool make(Log& log) const {
			if(exists)
				return false;

			log.info("Making directory: {}", path());
			std::filesystem::create_directories(path());
			return true;
		}
	};

	/*
	 * An abstraction for anything that you can run.
	 */
	struct Runnable{
		inline virtual int sync(Log& log) const = 0;
		inline virtual std::future<int> async(Log& log) const = 0;
		virtual ~Runnable() = default;
	};

	/*
	 * Low-level command calling abstraction layer.
	 */
	struct Cmd: public Runnable{
		std::vector<String> cmd;
		
		Cmd() = default;

		Cmd(const std::vector<String>& cmd):
			cmd{cmd}
		{}

		template<std::size_t N>
		Cmd(const std::array<String, N>& cmd):
			cmd{cmd.begin(), cmd.end()}
		{}

		// Returns escaped and concatinated command.
		inline String str() const {
			std::stringstream ss;
			for(const auto& e: cmd)
				ss << " " << e.escape();

			return ss.str().substr(1);
		}

		inline int sync(Log& log) const override {
			if(cmd.size() == 0){
				log.error("Cannot run empty CMD...");
				return -1;
			}

			std::string line = str();
			log.cmd(line);
			return system(line.c_str());
		}

		inline std::future<int> async(Log& log) const override {
			if(cmd.size() == 0){
				log.error("Cannot run empty CMD...");
				return std::async([](){ return -1; });
			}
			
			std::string line = str();
			log.cmd(line);
			return std::async([](std::string line){
				return system(line.c_str()); 
			}, line);
		}
	};

	/*
	 * Command template abstraction.
	 * Provides Runnable functionality, but with different parameters.
	 */
	struct CmdTmpl{
		std::string name;
		std::vector<String> cmd;

		CmdTmpl() = default;
		
		CmdTmpl(std::string_view name, const std::vector<String>& cmd):
			name{name},
			cmd{cmd}
		{}

		template<std::size_t N>
		CmdTmpl(std::string_view name, const std::array<String, N>& cmd):
			name{name},
			cmd{cmd.begin(), cmd.end()}
		{}

		inline std::unordered_set<std::string> variables() const {
			std::unordered_set<std::string> ret;

			for(const auto& e: cmd)
				ret.merge(e.variables());
			
			return ret;
		}

		inline std::vector<String> resolve(const std::unordered_map<std::string, std::vector<std::string>> dict) const {
			std::vector<String> ret;
			for(const auto& e: cmd){
				const auto& resolved = e.resolve(dict);
				ret.insert(ret.end(), resolved.begin(), resolved.end());
			}
			return ret;
		}

		inline Cmd compile() const {
			return Cmd(resolve({}));
		}

		inline Cmd compile(const std::unordered_map<std::string, std::vector<std::string>>& vars) const {
			return Cmd(resolve(vars));
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

	/*
	 * Helper struct for CmdPool.
	 */
	struct CmdPoolAsync: public std::vector<std::future<int>> {
		inline int wait(){
			int ret = 0;
			for(auto& cmd: *this){
				ret += cmd.get();
			}
			return ret;
		}
	};

	/*
	 * An abstraction layer for running commands in parallel.
	 */
	struct CmdPool: public std::vector<std::unique_ptr<Runnable>> {
		inline int sync(Log& log) const {
			int ret = 0;
			for(auto& cmd: *this){
				ret = cmd->sync(log);
				if(ret) return ret;
			}
			return 0;
		}

		inline CmdPoolAsync async(Log& log) const {
			CmdPoolAsync pool;
			for(const auto& cmd: *this){
				pool.emplace_back(cmd->async(log));
			}
			return pool;
		}

		template<typename T>
		inline void push(const T& obj){
			std::vector<std::unique_ptr<Runnable>>::push_back(std::make_unique<T>(obj));
		}
	};

	/*
	 * An abstraction layer for running Runnables in a queue (one after another).
	 */
	struct CmdQueue: public std::vector<std::unique_ptr<Runnable>>, public Runnable {
		inline int sync(Log& log) const override {
			int ret = 0;
			for(auto& cmd: *this){
				ret = cmd->sync(log);
				if(ret) return ret;
			}
			return 0;
		}

		inline std::future<int> async(Log& log) const override {
			return std::async([](Log* log, const CmdQueue* q){
				return q->sync(*log);
			}, &log, this);
		}

		template<typename T>
		inline void push(const T& obj){
			std::vector<std::unique_ptr<Runnable>>::push_back(std::make_unique<T>(obj));
		}
	};

	/*
	 * Not-yet-resolved CmdTmpl.
	 * Stores all data needed to resolve a command template (and the template itself).
	 * It is used as an universal abstraction of a command for Ninja and Makefile generation.
	 * (It by itself provides generation of fragments of Ninja and Makefile)
	 */
	struct CmdEntry: public Runnable{
		CmdTmpl cmd;
		std::string output;
		std::vector<std::string> inputs;
		std::vector<std::string> dependences;
		std::unordered_map<std::string, std::vector<std::string>> flags;
		bool smart;
		
		CmdEntry() = default;

		CmdEntry(std::string_view output, const std::vector<std::string>& inputs, const CmdTmpl& cmd, std::unordered_map<std::string, std::vector<std::string>> flags = {}, bool smart = false):
			cmd{cmd},
			output{output},
			inputs{inputs},
			flags{flags},
			smart{smart}
		{}

		template<std::size_t N, std::size_t M>
		CmdEntry(std::string_view output, const std::array<std::string, N>& inputs, const CmdTmpl& cmd, std::unordered_map<std::string, std::vector<std::string>> flags = {}, bool smart = false):
			cmd{cmd},
			output{output},
			inputs{inputs.begin(), inputs.end()},
			flags{flags},
			smart{smart}
		{}

		inline Directory directory() const {
			return Directory{output.substr(0, output.rfind('/'))};
		}

		inline bool smartRun() const {
			if(smart){
				File o(output);
				if(!o.exists)
					return true;
				
				bool run = false;
				for(const auto& i: inputs){
					File f(i);
					if(f > o){
						run = true;
						break;
					}
				}

				if(!run) for(const auto& d: dependences){
					File f(d);
					if(f > o){
						run = true;
						break;
					}
				}

				return run;
			}

			return true;
		}

		inline int sync(Log& log) const override {
			directory().make(log);
			
			if(!smartRun())
				return 0;

			std::unordered_map<std::string, std::vector<std::string>> vars({
				{"in", inputs},
				{"out", {output}}
			});

			vars.merge(std::unordered_map<std::string, std::vector<std::string>>(flags));
			
			return cmd.sync(log, vars);
		}

		inline std::future<int> async(Log& log) const override {
			directory().make(log);

			if(!smartRun())
				return std::async(std::launch::async, []{ return 0; });

			std::unordered_map<std::string, std::vector<std::string>> vars({
				{"in", inputs},
				{"out", {output}}
			});

			vars.merge(std::unordered_map<std::string, std::vector<std::string>>(flags));
			
			return cmd.async(log, vars);
		}

		inline std::string ninja() const {
			std::stringstream ss;
			ss << "build " << output << ": " << cmd.name;

			for(const auto& in: inputs)
				ss << " " << in;

			if(dependences.size() > 0){
				ss << " |";
				for(const auto& dep: dependences)
					ss << " " << dep;
			}

			for(const auto& [name, values]: flags){
				ss << std::endl << "    " << name << " =";
				for(const auto& value: values)
					ss << " " << value;
			}
			
			return ss.str();
		}

		inline std::string make() const {
			std::stringstream ss;
			ss << output << ":";

			for(const auto& in: inputs)
				ss << " " << in;

			for(const auto& dep: dependences)
				ss << " " << dep;

			ss << std::endl << "\t";

			std::unordered_map<std::string, std::vector<std::string>> vars({
				{"in", inputs},
				{"out", {output}},
			});

			vars.merge(std::unordered_map<std::string, std::vector<std::string>>(flags));

			ss << cmd.compile(vars).str() << std::endl;

			return ss.str();
		}
	};

	/*
	 * A high-level abstraction that stores information about a module.
	 */
	struct Module{
		std::string name;
		std::vector<File> files;
		std::vector<std::string> deps;
		std::vector<std::string> flags;
		bool disabled = false;
	
		Module() = default;
		Module(std::string_view name):
			name{name}
		{}
	
		inline bool addFile(const File& file){
			if(!file.exists)
				return true;
	
			files.emplace_back(file);
			
			return false;
		}
	
		inline bool addFile(std::string_view file){
			return addFile(File{file});
		}
	
		inline bool addDirectory(const Directory& dir){
			if(!dir.exists)
				return true;
	
			for(const auto& file: dir.files())
				files.emplace_back(file);
	
			return false;
		}
	
		inline bool addDirectory(std::string_view dir){
			return addDirectory(Directory{dir});
		}
	};

	// TODO: Introduce * extension (for all) and maybe some pattern matching (.c*, etc.)
	/*
	 * A high-level abstraction for appling command templates on modules.
	 */
	struct Stage{
		std::string name;
		Dictionary<std::string, CmdTmpl> cmds;
	
		Stage() = default;
		Stage(std::string_view name):
			name{name}
		{}
	
		virtual ~Stage() = default;
	
		virtual std::vector<CmdEntry> apply(Module& mod, const std::unordered_map<std::string, std::vector<std::string>>& flags = {}){
			(void) mod;
			(void) flags;
			return {};
		};
	
		template<std::size_t N>
		inline bool add(const std::array<std::string, N>& exts, const CmdTmpl& cmd){
			if(N <= 0)
				return true;

			for(std::size_t i = 0; i < N; i++){
				if(cmds.find(exts[i]) != cmds.end())
					return true;
			}

			cmds[exts[0]] = cmd;
			for(std::size_t i = 1; i < N; i++)
				cmds.alias(exts[0], exts[i]);
	
			return false;
		}

		inline bool add(std::string_view ext, const CmdTmpl& cmd){
			return add(std::array<std::string, 1>{std::string{ext}}, cmd);
		}
	};
	
	/*
	 * A stage that transforms files with given extensions to (the same amount of) files with other given extension.
	 * F.e. *.c => *.c.o and *.cpp => *.cpp.o
	 */
	struct Transform: public Stage{
		std::string outext;
	
		Transform() = default;
		Transform(std::string_view name, std::string_view outext):
			Stage{name},
			outext{outext}
		{}
	
		std::vector<CmdEntry> apply(Module& mod, const std::unordered_map<std::string, std::vector<std::string>>& flags = {}) override {
			if(cmds.size() <= 0)
				return {};

			std::vector<CmdEntry> ret;
			for(const auto& file: mod.files){
				std::string ext = file.extension();
				if(cmds.find(ext) == cmds.end())
					continue;
	
				std::string out = file.string();
				if(bro::starts_with(out, "build/")){
					out = out.substr(out.find('/', 6)); // Cut build/$stage_name/
					out = out.substr(out.find('/')); // Cut $mod_name/
				}
				out = "build/" + name + "/" + mod.name + "/" + out + outext;

				std::unordered_map<std::string, std::vector<std::string>> flgs = flags;
				flgs.merge(std::unordered_map<std::string, std::vector<std::string>>{
					{"mod", {mod.name}
				}});
	
				ret.emplace_back(out, std::vector<std::string>{file.string()}, cmds[ext], flgs);
			}
	
			for(const auto& entry: ret){
				mod.files.emplace_back(entry.output);
			}
	
			return ret;
		}
	};
	
	/*
	 * A stage thats links files with given extensions to one file named by resloving output template (outtmpl).
	 */
	struct Link: public Stage{
		String outtmpl;
		
		Link() = default;
		Link(std::string_view name, std::string_view outtmpl):
			Stage{name},
			outtmpl{outtmpl}
		{}
	
		std::vector<CmdEntry> apply(Module& mod, const std::unordered_map<std::string, std::vector<std::string>>& flags = {}) override {
			if(cmds.size() <= 0)
				return {};

			CmdEntry ret;
			ret.output = "build/" + name + "/" + outtmpl.resolve({{"mod", {mod.name}}})[0];
			ret.dependences = mod.deps;
			
			for(const auto& file: mod.files){
				std::string ext = file.extension();
				if(cmds.find(ext) == cmds.end())
					continue;
				ret.inputs.emplace_back(file.path());
			}

			ret.cmd = *cmds.begin();
			ret.flags = {
				{"mod", {mod.name}},
				{"flags", mod.flags}
			};
			ret.flags.merge(std::unordered_map<std::string, std::vector<std::string>>(flags));

			mod.files.emplace_back(ret.output);
	
			return {ret};
		}
	};

	// TODO: Implement something special instead of std::unordered_map<std::string, std::vector<std::string>> so we may take lists from cli args
	// TODO: Use extract and insert(std::move) with maps, so reallocation do not happen
	/*
	 * The highest-level abstraction layer that combined everything together.
	 * It also implements CLI flags.
	 */
	struct Bro{
		Log log;
		File header;
		File src;
		File exe;
		Dictionary<std::string, CmdTmpl> cmds;
		Dictionary<std::string, Module> mods;
		Dictionary<std::string, std::unique_ptr<Stage>> stages;
		std::unordered_map<std::size_t, std::unordered_set<std::size_t>> mods4stage;
		std::unordered_map<std::string, std::string> flags;

		inline void _setup_default(){
			std::string header_path = __FILE__;
			header = File(header_path);
			flags["cc"] = C_COMPILER_NAME;
			flags["cxx"] = CXX_COMPILER_NAME;
			flags["ld"] = C_COMPILER_NAME;
			flags["ar"] = "ar"; // TODO DELETE?
			flags["build"] = "build";

			flags["cxx_std_flag"] = CXX_STD_FLAG;
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

			for(int i = 1; i < argc; i++){
				std::string arg = argv[i];
				auto eq = arg.find('=');
				if(eq != std::string_view::npos){
					std::string name = arg.substr(0, eq);
					std::string value = arg.substr(eq + 1);
					flags[name] = value;
				} else if(arg[0] == '-'){
					std::string name = arg.substr(1, -1);
					flags[name] = "no";
				} else {
					flags[arg] = "yes";
				}
			}
		}

		inline bool isFresh(){
			if(hasFlag("~FRESH"))
				return isFlagSet("~FRESH");
			return !(src > exe || header > exe);
		}

		inline void fresh(){
			if(!isFresh()){
				int ret = 0;
				
				std::string old = exe.string() + ".old";

				// Save exe to .old
				ret = exe.copy(log, old);
				if(ret) std::exit(ret);
				
				// Recompile
				Cmd cmd({String(flags["cxx"]), "-o", exe.path(), src.path(), "-Wall", "-Wextra", String{flags["cxx_std_flag"]}});
				if((ret = cmd.sync(log))){
					log.error("Failed to recompile source: {}", src);
					std::exit(ret);
				}

				// Run
				cmd.cmd.clear();
				cmd.cmd.emplace_back(exe.path());
				for(const auto& [name, value]: flags){
					if(name[0] != '~')
						cmd.cmd.emplace_back(std::string{name} + "=" + std::string(value));
				}

				int status = cmd.sync(log);

				if(status == 0 && isFlagSet("clean", false))
					std::filesystem::remove(old);

				std::exit(status);
			}
		}

		inline bool hasFlag(const std::string& name){
			return flags.find(name) != flags.end();
		}

		// TODO: What about ~ variant
		inline std::string getFlag(const std::string& name, std::string_view dflt = ""){
			if(!hasFlag(name))
				return std::string(dflt);
			
			return std::string(flags[name]);
		}

		inline bool setFlag(const std::string& name, std::string_view value = "yes", bool force = true){
			if(!force && hasFlag(name))
				return false;

			flags[name] = value;
			return true;
		}

		inline bool isFlagSet(const std::string& name, bool dflt = false){
			if(!hasFlag(name))
				return dflt;

			return flags[name] != "no" && flags[name] != "0";
		}

		inline std::size_t cmd(const CmdTmpl& cmd, bool force = false){
			if(!force && cmds.find(cmd.name) != cmds.end())
				return std::numeric_limits<std::size_t>::max();

			auto [ix, _] = cmds.emplace(cmd.name, cmd);
			return ix;
		}

		inline std::size_t cmd(std::string_view name, const std::vector<String>& cmd){
			return this->cmd(CmdTmpl{name, cmd});
		}

		template<std::size_t N>
		inline std::size_t cmd(std::string_view name, const std::array<String, N>& cmd){
			return this->cmd(CmdTmpl{name, cmd});
		}

		inline std::size_t cmdx(const std::string& name){
			return cmds.dict[name];
		}

		inline std::size_t mod(std::string_view name){
			std::string n{name};

			if(mods.find(n) != mods.end())
				return std::numeric_limits<std::size_t>::max();

			auto [ix, ref] = mods.emplace(n, name);

			return ix;
		}

		inline std::size_t modx(const std::string& name){
			return mods.dict[name];
		}

		// TODO: Test these types (Ix, Path) to be accurate
		template<typename Ix, typename Path>
		inline bool addFile(Ix ix, Path path){
			return mods[ix].addFile(path);
		}

		template<typename Ix, typename Path>
		inline bool addDirectory(Ix ix, Path path){
			return mods[ix].addDirectory(path);
		}

		template<typename Ix>
		inline void addFlag(Ix ix, std::string_view flag){
			mods[ix].flags.emplace_back(flag);
		}

		template<typename Ix>
		inline void addDep(Ix ix, std::string_view dep){
			mods[ix].deps.emplace_back(dep);
		}

		template<typename Ix>
		inline void addFlagDep(Ix ix, std::string_view flag, std::string_view dep){
			mods[ix].flags.emplace_back(flag);
			mods[ix].deps.emplace_back(dep);
		}

		template<typename T>
		inline typename std::enable_if<std::is_base_of<Stage, T>::value, std::size_t>::type
		stage(std::string_view name, std::unique_ptr<T> stage){
			std::string n{name};

			if(stages.find(n) != stages.end())
				return std::numeric_limits<std::size_t>::max();

			auto [ix, ref] = stages.emplace(n, std::move(stage));

			return ix;
		}

		template<typename T>
		inline typename std::enable_if<std::is_base_of<Stage, T>::value, std::size_t>::type
		stage(std::string_view name, const T& stage){
			std::string n{name};

			if(stages.find(n) != stages.end())
				return std::numeric_limits<std::size_t>::max();

			auto [ix, ref] = stages.emplace(n, std::make_unique<T>(stage));

			return ix;
		}

		inline std::size_t stagex(const std::string& name){
			return stages.dict[name];
		}

		inline std::size_t transform(std::string_view name, std::string_view outext){
			return stage(name, Transform{name, outext});
		}

		inline std::size_t link(std::string_view name, std::string_view outtmpl){
			return stage(name, Link{name, outtmpl});
		}

		inline bool useCmd(std::size_t stage, std::size_t cmd, std::string_view ext){
			if(stage >= stages.size() || cmd >= cmds.size())
				return true;

			return stages[stage]->add(ext, cmds[cmd]);
		}

		inline bool applyMod(std::size_t stage, std::size_t mod){
			if(stage >= stages.size() || mod >= mods.size())
				return true;

			mods4stage[stage].insert(mod);
			return false;
		}

		inline int build(){
			std::filesystem::create_directory(flags["build"]);

			int ret = 0;

			std::unordered_map<std::string, std::vector<std::string>> flgs;
			for(auto [k, v]: flags){
				flgs[std::string{k}] = {std::string{v}};
			}

			std::vector<Module> mods = this->mods;

			CmdPool pool;
			for(const auto& stage: stages){
				for(std::size_t mod_ix: mods4stage[stages.dict[stage->name]]){
					Module& mod = mods[mod_ix];
					if(!mod.disabled) for(auto& cmd: stage->apply(mod, flgs)){
						cmd.smart = true;
						pool.push(cmd);
					}
				}

				if((ret = pool.async(log).wait()))
					return ret;

				pool.clear();
			}

			return ret;
		}

		inline int run(){
			bool dflt = false;
			for(const auto& [name, mod]: mods.dict){
				if(isFlagSet(name)){
					dflt = true;
					break;
				}
			}

			for(Module& mod: mods)
				mod.disabled = !isFlagSet(mod.name, !dflt);

			// TODO: ninja
			// TODO: make[file]

			if(isFlagSet("clean", false))
				// TODO: Add removing to API with Log
				std::filesystem::remove_all(getFlag("build"));

			return build();
		}

		inline int ninja(std::ostream& out){
			for(const CmdTmpl& tmpl: cmds){
				std::unordered_set<std::string> vars = tmpl.variables();
				std::unordered_map<std::string, std::vector<std::string>> dict;
				for(const auto& var: vars){
					dict[var] = {"$" + var};
				}

				out << "rule " << tmpl.name << std::endl;
				out << "  command = " << tmpl.compile(dict).str() << std::endl;
				out << std::endl;
			}

			std::vector<Module> mods = this->mods;

			// TODO: Phony targets
			for(const auto& stage: stages){
				for(std::size_t mod_ix: mods4stage[stages.dict[stage->name]]){
					Module& mod = mods[mod_ix];
					for(const CmdEntry& cmd: stage->apply(mod)){
						out << cmd.ninja() << std::endl;
					}
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

			out << ".DEFAULT_GOAL: all" << std::endl;
			out << ".MAIN: all" << std::endl;
			out << ".PHONY: default_goal" << std::endl;
			out << "default_goal: all" << std::endl;
			out << std::endl;

			std::vector<Module> mods = this->mods;

			for(const auto& stage: stages){
				for(std::size_t mod_ix: mods4stage[stages.dict[stage->name]]){
					Module& mod = mods[mod_ix];
					for(const CmdEntry& cmd: stage->apply(mod)){
						out << cmd.make() << std::endl;
					}
				}
			}

			for(const auto& mod: mods){
				out << ".PHONY: " << mod.name << std::endl;
				out << mod.name << ":";

				std::for_each(mod.files.begin() + this->mods[mod.name].files.size(), mod.files.end(), [&](const File& file){
					dirs.insert(file.parent_path());
					out << " " << file.string();
				});

				out << std::endl << std::endl;
			}

			out << "dirs :=";
			for(const auto& dir: dirs)
				out << " " << dir;
			out << std::endl;
			out << std::endl;
			out << ".PHONY: all" << std::endl;
			out << "all: $(dirs)";
			for(const auto& mod: mods)
				out << " " << mod.name;
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

	// Pipe operator overloads for commonly printed structs.

	template<typename CharT, typename Traits>
	inline std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& out, const File& file){
		auto time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(file.time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
		std::time_t tt = std::chrono::system_clock::to_time_t(time);
		std::tm* tm = std::localtime(&tt);
		return out << "bro::File{'exists': " << file.exists << ", 'path': " << file.path() << ", 'time': '" << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << "'}";
	}

	template<typename CharT, typename Traits>
	inline std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& out, const CmdTmpl& tmpl){
		out << "bro::CmdTmpl{'name': " << std::quoted(tmpl.name) << ", 'cmd': { ";
		for(const auto& e: tmpl.cmd)
			out << std::quoted(e) << " ";
		return out << "}}";
	}
}
