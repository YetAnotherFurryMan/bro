#include "bro.hpp"

#include <fstream>

int main(int argc, const char** argv){
	bro::Bro bro(argc, argv);
	
	bro.log.info("Fresh: {}", bro.isFresh());
	
	bro.fresh();

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
