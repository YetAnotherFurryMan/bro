#include "bro.hpp"

#include <fstream>

int main(int argc, const char** argv){
	bro::Bro bro(argc, argv);
	
	bro.log.info("Fresh: {}", bro.isFresh());
	
	bro.fresh();

	bro.log.info("Header: {}", bro.header);

	bro.registerCmd("cxx", ".cpp", {"g++", "-c", "$in", "-o", "$out"});
	bro.registerCmd("cc", ".c", {"gcc", "-c", "$in", "-o", "$out"});

	bro::CmdTmpl run({"./$in"});

	std::filesystem::create_directories("src/mod");
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

		bro.build();

		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		std::ofstream mod_main("src/mod/main.cpp");
		mod_main << "#include <iostream>\nvoid hello();void bye();int main(){std::cout << \"Hello World!\" << std::endl; hello(); bye(); return 0;}";
		mod_main.close();

		std::ofstream mod_bye("src/mod/bye.cpp");
		mod_bye << "#include <iostream>\nvoid bye(){std::cout << \"Hello from bye()\" << std::endl;}";
		mod_bye.close();

		bro.build();

		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	{
		bro.build();
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
		bro.build();
		run.sync(bro.log, {{"in", {"build/bin/mod"}}});
	}

	bro.ninja();

	if(bro.flags.find("save") == bro.flags.end()){
		std::filesystem::remove_all("src");
		std::filesystem::remove_all("build");
	}

	bro.log.info("Cmds: {}", bro.cmds.size());
	for(const auto& [key, val]: bro.cmds){
		auto cmd = val.compile();
		bro.log.info("Cmd {}: {}", key, cmd.str());
	}

	return 0;
}
