#include "bro.hpp"

#include <fstream>

int main(int argc, const char** argv){
	bro::Bro bro(argc, argv);
	bro.log.error("Hello {}!", "World");
	bro.log.warning("Hello {}!", "World");
	bro.log.info("Hello {}!", "World");
	bro.log.info("Src: {}", bro.src);
	bro.log.info("Exe: {}", bro.exe);
	bro.log.info("Argc: {}", bro.args.size());
	bro.log.info("Src file: {}", bro.src);
	bro.log.info("Exe file: {}", bro.exe);

	bro.log.info("Fresh: {}", bro.isFresh());

	bro::File dne("this_file_do_not_exists.txt");
	bro.log.info("Dne: {}", dne);

	/* std::string cmdE[] = {"g++", bro.src.path.string(), "-o", bro.exe.path.string()}; */
	/* bro::Cmd cmd("cxx", cmdE, 4); */
	/* bro.log.info("Cmd name: {}", cmd.name); */
	/* for(const auto& e: cmd.cmd){ */
	/* 	bro.log.info("E:\t{}", e); */
	/* } */

	/* if(!bro.isFresh()){ */
	/* 	bro.log.warning("Src is old, recompiling..."); */
	/* 	if(cmd.runSync(bro.log) != 0){ */
	/* 		bro.log.error("Failed to compile src {}", bro.src); */
	/* 		return 1; */
	/* 	} */
		
	/* 	std::string cmdRun[] = {bro.exe.path.string()}; */
	/* 	bro::Cmd runCmd("run", cmdRun, 1); */
	/* 	return runCmd.runSync(bro.log); */
	/* } */

	/* std::vector<std::future<int>> cmds; */

	/* for(int i = 0; i < 10; i++){ */
	/* 	std::stringstream ss; */
	/* 	ss << "hello"; */
	/* 	ss << i; */
	/* 	std::ofstream src(ss.str() + ".cpp"); */
	/* 	src << "#include <iostream>\nint main(){std::cout << \"Hello " << i << " World!\" << std::endl; return 0;}"; */
	/* 	src.close(); */

	/* 	std::string cs[] = {"g++", "-c", ss.str() + ".cpp", "-o", ss.str() + ".o"}; */
	/* 	bro::Cmd c("cxx", cs, 5); */
	/* 	cmds.push_back(c.runAsync(bro.log)); */
	/* } */

	/* for(auto& e: cmds){ */
	/* 	if(e.get() != 0){ */
	/* 		bro.log.error("Failed"); */
	/* 		return -1; */
	/* 	} */
	/* } */

	/* cmds.clear(); */
	/* for(int i = 0; i < 10; i++){ */
	/* 	std::stringstream ss; */
	/* 	ss << "hello"; */
	/* 	ss << i; */
	/* 	std::string cs[] = {"g++", ss.str() + ".o", "-o", ss.str()}; */

	/* 	bro::Cmd c("cxx", cs, 4); */
	/* 	cmds.push_back(c.runAsync(bro.log)); */
	/* } */

	/* for(auto& e: cmds){ */
	/* 	if(e.get() != 0){ */
	/* 		bro.log.error("Failed 2"); */
	/* 		return -1; */
	/* 	} */
	/* } */

	return 0;
}
