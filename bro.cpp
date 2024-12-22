#include "bro.hpp"

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

	std::string cmdE[] = {"g++", bro.src.path.string(), "-o", bro.exe.path.string()};
	bro::Cmd cmd("cxx", cmdE, 4);
	bro.log.info("Cmd name: {}", cmd.name);
	for(const auto& e: cmd.cmd){
		bro.log.info("E:\t{}", e);
	}

	if(!bro.isFresh()){
		bro.log.warning("Src is old, recompiling...");
		cmd.runSync(bro.log);
	}

	return 0;
}
