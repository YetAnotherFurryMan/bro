#include "bro.hpp"

#include <fstream>

int main(int argc, const char** argv){
	bro::Bro bro(argc, argv);
	
	bro.log.info("Fresh: {}", bro.isFresh());
	
	bro.fresh();

	std::string cmd_cxx[] = {"g++", "-c", "$in", "-o", "$out"};
	bro::CmdTmpl cxx(cmd_cxx, 5);

	bro.registerCmd("g++", ".cpp", cmd_cxx, 5);

	std::string cmd_exe[] = {"g++", "$in", "-o", "$out"};
	bro::CmdTmpl exe(cmd_exe, 4);

	std::string cmd_run[] = {"./$in"};
	bro::CmdTmpl run(cmd_run, 1);

	std::filesystem::create_directories("src/mod");
	bro::Directory mod("src/mod");

	bro::Directory src("src");
	src.copyTree(bro.log, "build/obj");
	std::filesystem::create_directory("build/bin");

	{
		std::ofstream mod_main("src/mod/main.cpp");
		mod_main << "#include <iostream>\nvoid hello();int main(){std::cout << \"Hello World!\" << std::endl; hello(); return 0;}";
		mod_main.close();

		std::ofstream mod_hello("src/mod/hello.cpp");
		mod_hello << "#include <iostream>\nvoid hello(){std::cout << \"Hello from hello()\" << std::endl;}";
		mod_hello.close();

		std::vector<std::string> outs;
		std::vector<bro::File> ins;

		for(const auto& e: mod.files()){
			bro.log.info("File {}", e);
			if(e.path.extension() == ".cpp"){
				ins.push_back(e);
				outs.push_back(std::filesystem::path("build/obj" + e.path.string().substr(3)).replace_extension(".cpp.o"));
			} else{
				bro.log.warning("WTF: {}", e);
			}
		}

		bro::CmdPool pool;
		for(std::size_t i = 0; i < ins.size(); i++){
			if(!(bro::File(outs[i]) > ins[i])){
				pool.push(bro.cmds["g++"].compile(outs[i], ins[i].path.string()));
			}
		}

		bro.log.info("Pool: {}", pool.size());
		pool.sync(bro.log);

		exe.sync(bro.log, "build/bin/mod", outs.data(), outs.size());

		run.sync(bro.log, "build/bin/mod");
	}

	{
		std::ofstream mod_main("src/mod/main.cpp");
		mod_main << "#include <iostream>\nvoid hello();void bye();int main(){std::cout << \"Hello World!\" << std::endl; hello(); bye(); return 0;}";
		mod_main.close();

		std::ofstream mod_bye("src/mod/bye.cpp");
		mod_bye << "#include <iostream>\nvoid bye(){std::cout << \"Hello from bye()\" << std::endl;}";
		mod_bye.close();

		std::vector<std::string> outs;
		std::vector<bro::File> ins;

		for(const auto& e: mod.files()){
			bro.log.info("File {}", e);
			if(e.path.extension() == ".cpp"){
				ins.push_back(e);
				outs.push_back(std::filesystem::path("build/obj" + e.path.string().substr(3)).replace_extension(".cpp.o"));
			} else{
				bro.log.warning("WTF: {}", e);
			}
		}

		bro::CmdPool pool;
		for(std::size_t i = 0; i < ins.size(); i++){
			if(!(bro::File(outs[i]) > ins[i])){
				pool.push(bro.cmds["g++"].compile(outs[i], ins[i].path.string()));
			}
		}

		bro.log.info("Pool: {}", pool.size());
		pool.sync(bro.log);

		if(pool.size() > 0){
			exe.sync(bro.log, "build/bin/mod", outs.data(), outs.size());
		}

		run.sync(bro.log, "build/bin/mod");
	}

	{
		std::vector<std::string> outs;
		std::vector<bro::File> ins;

		for(const auto& e: mod.files()){
			bro.log.info("File {}", e);
			if(e.path.extension() == ".cpp"){
				ins.push_back(e);
				outs.push_back(std::filesystem::path("build/obj" + e.path.string().substr(3)).replace_extension(".cpp.o"));
			} else{
				bro.log.warning("WTF: {}", e);
			}
		}

		bro::CmdPool pool;
		for(std::size_t i = 0; i < ins.size(); i++){
			if(!(bro::File(outs[i]) > ins[i])){
				pool.push(bro.cmds["g++"].compile(outs[i], ins[i].path.string()));
			}
		}

		bro.log.info("Pool: {}", pool.size());
		pool.sync(bro.log);

		if(pool.size() > 0){
			exe.sync(bro.log, "build/bin/mod", outs.data(), outs.size());
		}

		run.sync(bro.log, "build/bin/mod");
	}

	std::filesystem::remove_all("src");
	std::filesystem::remove_all("build");

	bro.log.info("Cmds: {}", bro.cmds.size());
	for(const auto& [key, val]: bro.cmds){
		auto cmd = val.compile("$out", "$in");
		bro.log.info("Cmd {}: {}", key, cmd.str());
	}

	return 0;
}
