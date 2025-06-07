#include "bro.hpp"

#include <fstream>

struct Module{
	std::string name;
	std::vector<bro::File> files;

	Module() = default;
	Module(std::string_view name):
		name{name}
	{}

	inline bool addFile(const bro::File& file){
		if(!file.exists)
			return true;

		files.emplace_back(file);
		
		return false;
	}

	inline bool addFile(std::string_view file){
		return addFile(bro::File{file});
	}

	inline bool addDirectory(const bro::Directory& dir){
		if(!dir.exists)
			return true;

		for(const auto& file: dir.files())
			files.emplace_back(file);

		return false;
	}

	inline bool addDirectory(std::string_view dir){
		return addDirectory(bro::Directory{dir});
	}
};

struct CmdEntry: public bro::Runnable{
	bro::Cmd cmd;
	std::string output;
	std::vector<std::string> inputs;
	std::vector<std::string> dependences;
	
	CmdEntry() = default;

	CmdEntry(std::string_view output, const std::vector<std::string>& inputs, const std::vector<std::string>& cmd):
		cmd{cmd},
		output{output},
		inputs{inputs}
	{}

	template<std::size_t N, std::size_t M>
	CmdEntry(std::string_view output, const std::array<std::string, N>& inputs, const std::array<std::string, M>& cmd):
		cmd{cmd},
		output{output},
		inputs{inputs.begin(), inputs.end()}
	{}

	inline bro::Directory directory() const {
		return bro::Directory{output.substr(0, output.rfind('/'))};
	}

	inline int sync(bro::Log& log) override {
		directory().make(log);
		return cmd.sync(log);
	}

	inline std::future<int> async(bro::Log& log) override {
		directory().make(log);
		return cmd.async(log);
	}

	// TODO: inline std::string ninja()
	// TODO: inline std::string make()
};

struct Stage{
	std::string name;
	std::unordered_map<std::string, bro::CmdTmpl> cmds;

	Stage() = default;
	Stage(std::string_view name):
		name{name}
	{}

	virtual ~Stage() = default;

	virtual std::vector<CmdEntry> apply(Module& mod){
		return {};
	};

	inline bool add(std::string_view ext, const bro::CmdTmpl& cmd){
		std::string e{ext};

		if(cmds.find(e) != cmds.end())
			return true;

		cmds[e] = cmd;

		return false;
	}
};

inline bool starts_with(const std::string& a, std::string_view b){
	if(a.length() < b.length())
		return false;

	for(std::size_t i = 0; i < b.length(); i++)
		if(a[i] != b[i])
			return false;

	return true;
}

struct Transform: public Stage{
	std::string outext;

	Transform() = default;
	Transform(std::string_view name, std::string_view outext):
		Stage{name},
		outext{outext}
	{}

	virtual std::vector<CmdEntry> apply(Module& mod) override {
		std::vector<CmdEntry> ret;

		for(const auto& file: mod.files){
			std::string ext = file.path.extension();
			if(cmds.find(ext) == cmds.end())
				continue;

			std::string out = file.path.string();
			if(starts_with(out, "build/")){
				out = out.substr(out.find('/', 6)); // Cut build/$stage_name/
				out = out.substr(out.find('/')); // Cut $mod_name/
			}
			out = "build/" + name + "/" + mod.name + "/" + out + outext;

			ret.emplace_back(out, std::vector<std::string>{file.path.string()}, cmds[ext].compile({
						{"in", {file.path.string()}},
						{"out", {out}},
						{"mod", {mod.name}}
					}).cmd);
		}

		for(const auto& entry: ret){
			mod.files.emplace_back(entry.output);
		}

		return ret;
	}
};

struct Link: public Stage{
	std::string outtmpl;
	
	Link() = default;
	Link(std::string_view name, std::string_view outtmpl):
		Stage{name},
		outtmpl{outtmpl}
	{}

	virtual std::vector<CmdEntry> apply(Module& mod) override {
		CmdEntry ret;
		ret.output = "build/" + name + "/" + outtmpl;
		
		std::string::size_type pos = 0;
		while((pos = ret.output.find("$mod", pos)) != std::string::npos){
			ret.output.replace(pos, 4, mod.name);
			pos += mod.name.length();
		}

		for(const auto& file: mod.files){
			std::string ext = file.path.extension();
			if(cmds.find(ext) == cmds.end())
				continue;
			ret.inputs.emplace_back(file.path);
		}

		ret.cmd = (*cmds.begin()).second.compile({
				{"out", {ret.output}},
				{"in", ret.inputs},
				{"mod", {mod.name}}
			});

		return {ret};
	}
};

int main(int argc, const char** argv){
	bro::Bro bro(argc, argv);
	
	bro.log.info("Fresh: {}", bro.isFresh());
	bro.log.info("Has ~FRESH: {}", bro.hasFlag("~FRESH"));
	
	bro.fresh();

	bro.log.info("Header: {}", bro.header);

	bro.registerCmd("cxx", ".cpp", ".cpp.o", {"g++", "-c", "$in", "-o", "$out"});
	bro.registerCmd("cc", ".c", ".c.o", {"gcc", "-c", "$in", "-o", "$out"});

	bro::CmdTmpl run({"./$in"});

	std::filesystem::create_directories("src/mod");
	std::filesystem::create_directories("src/common");
	std::filesystem::create_directories("common");
	bro::Directory mod("src/mod");

	bro.registerModule(bro::ModType::EXE, "mod");
	bro.use("mod", "cxx");
	bro.link("mod", "-lstdc++");

	{
		std::ofstream mod_main("src/mod/main.cpp");
		mod_main << "#include <iostream>\nvoid hello();int main(){std::cout << \"Hello World!\" << std::endl; hello(); return 0;}";
		mod_main.close();

		std::ofstream mod_hello("src/mod/hello.cpp");
		mod_hello << "#include <iostream>\nvoid hello(){std::cout << \"Hello from hello()\" << std::endl;}";
		mod_hello.close();

		Module mod("mod");
		mod.addDirectory("src/mod");

		Transform out("out", ".o");
		out.add(".cpp", bro.cmds["cxx"]);

		bro::CmdPool pool;
		for(const auto& cmd: out.apply(mod)){
			pool.push(cmd);
		}

		pool.sync(bro.log);

		Link bin("bin", "$mod");
		bin.add(".o", bro.cmds["exe"].resolve("flags", "-lstdc++"));

		pool.clear();

		for(const auto& cmd: bin.apply(mod)){
			pool.push(cmd);
		}

		pool.sync(bro.log);

		//bro.run();

		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	return 0;

	{
		std::ofstream mod_main("src/mod/main.cpp");
		mod_main << "#include <iostream>\nvoid hello();void bye();int main(){std::cout << \"Hello World!\" << std::endl; hello(); bye(); return 0;}";
		mod_main.close();

		std::ofstream mod_bye("src/mod/bye.cpp");
		mod_bye << "#include <iostream>\nvoid bye(){std::cout << \"Hello from bye()\" << std::endl;}";
		mod_bye.close();

		bro.run();

		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		bro.run();
		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		std::ofstream mod_main("src/mod/main.cpp");
		mod_main << "#include <iostream>\nvoid hello();extern \"C\" void bye();int main(){std::cout << \"Hello World!\" << std::endl; hello(); bye(); return 0;}";
		mod_main.close();

		std::filesystem::remove("src/mod/bye.cpp");

		std::ofstream mod_bye("src/mod/bye.c");
		mod_bye << "#include <stdio.h>\nvoid bye(){printf(\"Hello from bye()\\n\");}";
		mod_bye.close();

		bro.use("mod", "cc");
		bro.run();
		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		std::filesystem::rename("src/mod/bye.c", "src/common/bye.c");

		bro.addDirectory("mod", "src/common");
		bro.run();
		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		std::ofstream mod_main("src/mod/main.cpp");
		mod_main << "#include <iostream>\nvoid hello();extern \"C\" void bye();extern \"C\" void ex();int main(){std::cout << \"Hello World!\" << std::endl; hello(); bye(); ex(); return 0;}";
		mod_main.close();

		std::ofstream mod_bye("common/ex.c");
		mod_bye << "#include <stdio.h>\nvoid ex(){printf(\"Hello from ex()\\n\");}";
		mod_bye.close();

		bro.addFile("mod", "common/ex.c");
		bro.run();
		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		std::filesystem::remove_all("build");
		bro.ninja();

		bro::CmdTmpl ninja({"ninja"});
		ninja.sync(bro.log);
		
		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		std::filesystem::remove_all("build");
		bro.makefile();

		bro::CmdTmpl make({"make"});
		make.sync(bro.log);
		
		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	if(!bro.isFlagSet("save")){
		std::filesystem::remove_all("src");
		std::filesystem::remove_all("common");
		std::filesystem::remove_all("build");
		std::filesystem::remove("build.ninja");
		std::filesystem::remove("Makefile");
	}

	bro.log.info("Cmds: {}", bro.cmds.size());
	for(const auto& [key, val]: bro.cmds){
		auto cmd = val.compile();
		bro.log.info("Cmd {}: {}", key, cmd.str());
	}

	return 0;
}
