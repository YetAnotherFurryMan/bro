#include "bro.hpp"

int main(int argc, const char** argv){
	bro::Bro bro(argc, argv);
	
	bro.log.info("Fresh: {}", bro.isFresh());
	bro.log.info("Has ~FRESH: {}", bro.hasFlag("~FRESH"));
	
	bro.fresh();

	bro.log.info("Header: {}", bro.header);

	std::size_t cxx_ix = bro.cmd("cxx", {"g++", "-c", "$in", "-o", "$out"});
	std::size_t c_ix = bro.cmd("cc", {"gcc", "-c", "$in", "-o", "$out"});
	std::size_t exe_ix = bro.cmd("exe", {"gcc", "$in", "-o", "$out", "$flags"});

	bro::CmdTmpl run({"./$in"});

	std::filesystem::create_directories("src/mod");
	std::filesystem::create_directories("src/common");
	std::filesystem::create_directories("common");
	bro::Directory mod("src/mod");

	std::size_t mod_ix = bro.mod("mod");
	// bro.link("mod", "-lstdc++");

	std::size_t obj_ix = bro.transform("obj", ".o");
	bro.useCmd(obj_ix, cxx_ix, ".cpp");
	bro.useCmd(obj_ix, c_ix, ".c");

	{
		std::ofstream mod_main("src/mod/main.cpp");
		mod_main << "#include <iostream>\nvoid hello();int main(){std::cout << \"Hello World!\" << std::endl; hello(); return 0;}";
		mod_main.close();

		std::ofstream mod_hello("src/mod/hello.cpp");
		mod_hello << "#include <iostream>\nvoid hello(){std::cout << \"Hello from hello()\" << std::endl;}";
		mod_hello.close();

		bro::Module& mod = bro.mods[mod_ix];
		mod.addDirectory("src/mod");

		bro::Transform out_trans("out", ".o");
		bro::Stage& out = out_trans;
		out.add(".cpp", bro.cmds["cxx"]);

		bro::CmdPool pool;
		for(auto& cmd: out.apply(mod)){
			cmd.smart = true;
			pool.push(cmd);
		}

		pool.sync(bro.log);

		bro::Link bin("bin", "$mod");
		bin.add(".o", bro.cmds[exe_ix].resolve("flags", "-lstdc++"));

		pool.clear();

		for(const auto& cmd: bin.apply(mod)){
			pool.push(cmd);
		}

		pool.sync(bro.log);

		//bro.run();

		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

#if 0
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

#endif
	if(!bro.isFlagSet("save")){
		std::filesystem::remove_all("src");
		std::filesystem::remove_all("common");
		std::filesystem::remove_all("build");
		std::filesystem::remove("build.ninja");
		std::filesystem::remove("Makefile");
	}

	bro.log.info("Cmds: {}", bro.cmds.size());
	for(const auto& [key, val_ix]: bro.cmds.dict){
		auto cmd = bro.cmds[val_ix].compile();
		bro.log.info("Cmd {}: {}", key, cmd.str());
	}
	
	return 0;
}
