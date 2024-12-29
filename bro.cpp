#include "bro.hpp"

#include <fstream>

int main(int argc, const char** argv){
	bro::Bro bro(argc, argv);
	
	bro.log.info("Fresh: {}", bro.isFresh());
	
	bro.fresh();

	std::string cxx_cmd[] = {"g++", "-c", "$in", "-o", "$out"};
	bro::CmdTmpl cxx("cxx", cxx_cmd, 5);

	std::string link_cmd[] = {"g++", "$in", "-o", "$out"};
	bro::CmdTmpl link("link", link_cmd, 4);

	std::string rm_cmd[] = {"rm", "-f", "$in", "$in.o", "$in.cpp"};
	bro::CmdTmpl rm("rm", rm_cmd, 5);

	bro::CmdPool pool;

	for(int i = 0; i < 10; i++){
		std::stringstream ss;
		ss << "hello";
		ss << i;
		std::ofstream src(ss.str() + ".cpp");
		src << "#include <iostream>\nint main(){std::cout << \"Hello " << i << " World!\" << std::endl; return 0;}";
		src.close();

		pool.push_back(cxx.compile(ss.str() + ".o", ss.str() + ".cpp"));
	}

	int ret = pool.async(bro.log).wait();
	bro.log.info("Pool1: {}", ret);

	pool.clear();

	for(int i = 0; i < 10; i++){
		std::stringstream ss;
		ss << "hello";
		ss << i;

		pool.push_back(link.compile(ss.str(), ss.str() + ".o"));
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

	for(int i = 0; i < 10; i++){
		std::stringstream ss;
		ss << "hello";
		ss << i;

		pool.push_back(rm.compile("", ss.str()));
	}

	ret = pool.async(bro.log).wait();
	bro.log.info("Pool4: {}", ret);

	pool.clear();

	return 0;
}
