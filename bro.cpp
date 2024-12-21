#include "bro.hpp"

int main(int argc, const char** argv){
	bro::Bro bro(argc, argv);
	bro.log.error("Hello {}!", "World");
	bro.log.warning("Hello {}!", "World");
	bro.log.info("Hello {}!", "World");
	bro.log.info("Exe: {}", bro.exe);
	bro.log.info("Argc: {}", bro.args.size());

	bro::File exe(bro.exe);
	bro.log.info("Exe file: {}", exe);
	return 0;
}
