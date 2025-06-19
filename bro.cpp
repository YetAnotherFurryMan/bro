#include "bro.hpp"

int main(int argc, const char** argv){
	bro::Bro bro(argc, argv);
	
	bro.log.info("Fresh: {}", bro.isFresh());
	bro.log.info("Has ~FRESH: {}", bro.hasFlag("~FRESH"));
	
	bro.fresh();

	bro.log.info("Header: {}", bro.header);

	std::size_t cxx_ix = bro.cmd("cxx", {"g++", "-c", "$in", "-o", "$out"});
	std::size_t cc_ix = bro.cmd("cc", {"gcc", "-c", "$in", "-o", "$out"});
	std::size_t exe_ix = bro.cmd("exe", {"gcc", "$in", "-o", "$out", "$flags", "-lstdc++"});

	bro::CmdTmpl run("run", {"./$in"});

	std::filesystem::create_directories("src/mod");
	std::filesystem::create_directories("src/common");
	std::filesystem::create_directories("common");
	bro::Directory mod("src/mod");

	std::size_t mod_ix = bro.mod("mod", false);
	// bro.link("mod", "-lstdc++");

	std::size_t obj_ix = bro.transform("obj", ".o");
	bro.useCmd(obj_ix, cxx_ix, ".cpp");
	bro.useCmd(obj_ix, cc_ix, ".c");

	std::size_t bin_ix = bro.link("bin", "$mod");
	bro.useCmd(bin_ix, exe_ix, ".o");

	bro.applyMod(obj_ix, mod_ix);
	bro.applyMod(bin_ix, mod_ix);

	{
		bro.log.info("NO: {}", 1);

		std::ofstream mod_main("src/mod/main.cpp");
		mod_main << "#include <iostream>\nvoid hello();int main(){std::cout << \"Hello World!\" << std::endl; hello(); return 0;}";
		mod_main.close();

		std::ofstream mod_hello("src/mod/hello.cpp");
		mod_hello << "#include <iostream>\nvoid hello(){std::cout << \"Hello from hello()\" << std::endl;}";
		mod_hello.close();

		bro.mods[mod_ix].addDirectory("src/mod");

		bro.run();

		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		bro.log.info("NO: {}", 2);

		std::ofstream mod_main("src/mod/main.cpp");
		mod_main << "#include <iostream>\nvoid hello();void bye();int main(){std::cout << \"Hello World!\" << std::endl; hello(); bye(); return 0;}";
		mod_main.close();

		std::ofstream mod_bye("src/mod/bye.cpp");
		mod_bye << "#include <iostream>\nvoid bye(){std::cout << \"Hello from bye()\" << std::endl;}";
		mod_bye.close();

		bro.mods[mod_ix].files.clear();
		bro.mods[mod_ix].addDirectory("src/mod");

		bro.run();

		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		bro.log.info("NO: {}", 3);

		bro.run();
		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		bro.log.info("NO: {}", 4);

		std::ofstream mod_main("src/mod/main.cpp");
		mod_main << "#include <iostream>\nvoid hello();extern \"C\" void bye();int main(){std::cout << \"Hello World!\" << std::endl; hello(); bye(); return 0;}";
		mod_main.close();

		std::filesystem::remove("src/mod/bye.cpp");

		std::ofstream mod_bye("src/mod/bye.c");
		mod_bye << "#include <stdio.h>\nvoid bye(){printf(\"Hello from bye()\\n\");}";
		mod_bye.close();

		bro.mods[mod_ix].files.clear();
		bro.mods[mod_ix].addDirectory("src/mod");

		bro.run();
		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		bro.log.info("NO: {}", 5);

		std::filesystem::rename("src/mod/bye.c", "src/common/bye.c");

		bro.mods[mod_ix].files.clear();
		bro.mods[mod_ix].addDirectory("src/mod");
		bro.mods[mod_ix].addDirectory("src/common");

		bro.run();
		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		bro.log.info("NO: {}", 6);

		std::ofstream mod_main("src/mod/main.cpp");
		mod_main << "#include <iostream>\nvoid hello();extern \"C\" void bye();extern \"C\" void ex();int main(){std::cout << \"Hello World!\" << std::endl; hello(); bye(); ex(); return 0;}";
		mod_main.close();

		std::ofstream mod_bye("common/ex.c");
		mod_bye << "#include <stdio.h>\nvoid ex(){printf(\"Hello from ex()\\n\");}";
		mod_bye.close();

		bro.mods[mod_ix].files.clear();
		bro.mods[mod_ix].addDirectory("src/mod");
		bro.mods[mod_ix].addDirectory("src/common");
		bro.mods[mod_ix].addFile("common/ex.c");

		bro.run();
		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		std::filesystem::remove_all("build");
		bro.ninja();

		bro::Cmd ninja({"ninja"});
		ninja.sync(bro.log);

		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		std::filesystem::remove_all("build");
		bro.makefile();

		bro::Cmd make({"make"});
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
	for(const auto& [key, val_ix]: bro.cmds.dict){
		auto cmd = bro.cmds[val_ix].compile();
		bro.log.info("Cmd {}: {}", key, cmd.str());
	}
	
	return 0;
}
