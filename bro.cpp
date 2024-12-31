#include "bro.hpp"

#include <fstream>

int main(int argc, const char** argv){
	bro::Bro bro(argc, argv);
	
	bro.log.info("Fresh: {}", bro.isFresh());
	
	bro.fresh();

	std::string cxx_cmd[] = {"g++", "-c", "$in", "-o", "$out"};
	bro::CmdTmpl cxx(cxx_cmd, 5);

	std::string link_cmd[] = {"g++", "$in", "-o", "$out"};
	bro::CmdTmpl link(link_cmd, 4);

	std::string run_cmd[] = {"./$in"};
	bro::CmdTmpl run(run_cmd, 1);

	std::string rm_cmd[] = {"rm", "-f", "$in", "$in.o", "$in.cpp"};
	bro::CmdTmpl rm(rm_cmd, 5);

	bro::CmdPool pool;

	for(int i = 0; i < 10; i++){
		std::stringstream ss;
		ss << "hello";
		ss << i;

		std::ofstream src(ss.str() + ".cpp");
		src << "#include <iostream>\nint main(){std::cout << \"Hello " << i << " World!\" << std::endl; return 0;}";
		src.close();

		std::unique_ptr<bro::CmdQueue> q = std::make_unique<bro::CmdQueue>();
		q->push(cxx.compile(ss.str() + ".o", ss.str() + ".cpp"));
		q->push(link.compile(ss.str(), ss.str() + ".cpp"));
		q->push(run.compile(ss.str()));
		q->push(rm.compile(ss.str()));
		pool.push_back(std::move(q));
	}

	bro.log.info("Pool: {}", pool.async(bro.log).wait());

	std::filesystem::create_directories("mod/a/a");
	std::filesystem::create_directories("mod/a/b");
	std::filesystem::create_directories("mod/a/c");
	std::filesystem::create_directories("mod/b/a");
	std::filesystem::create_directories("mod/b/b");
	std::filesystem::create_directories("mod/b/c");

	bro::Directory mod("mod");

	bro.log.info("Copy tree: {}", mod.copyTree(bro.log, "build/obj"));

	std::filesystem::remove_all("mod");
	std::filesystem::remove_all("build");

	return 0;
}
