#include "bro.hpp"

#include <fstream>

int main(int argc, const char** argv){
	bro::Bro bro(argc, argv);
	
	bro.log.info("Fresh: {}", bro.isFresh());
	
	bro.fresh();

	bro::CmdPool pool;

	for(int i = 0; i < 10; i++){
		std::stringstream ss;
		ss << "hello";
		ss << i;
		std::ofstream src(ss.str() + ".cpp");
		src << "#include <iostream>\nint main(){std::cout << \"Hello " << i << " World!\" << std::endl; return 0;}";
		src.close();

		std::string cs[] = {"g++", "-c", ss.str() + ".cpp", "-o", ss.str() + ".o"};
		bro::Cmd c("cxx", cs, 5);
		pool.push_back(c);
	}

	int ret = pool.async(bro.log).wait();
	bro.log.info("Pool1: {}", ret);

	pool.clear();

	for(int i = 0; i < 10; i++){
		std::stringstream ss;
		ss << "hello";
		ss << i;
		std::string cs[] = {"g++", ss.str() + ".o", "-o", ss.str()};

		bro::Cmd c("link", cs, 4);
		pool.push_back(c);
	}

	ret = pool.async(bro.log).wait();
	bro.log.info("Pool2: {}", ret);

	pool.clear();

	for(int i = 0; i < 10; i++){
		std::stringstream ss;
		ss << "./hello";
		ss << i;
		std::string cs[] = {ss.str()};

		bro::Cmd c("run", cs, 1);
		pool.push_back(c);
	}

	ret = pool.async(bro.log).wait();
	bro.log.info("Pool3: {}", ret);

	pool.clear();

	return 0;
}
