#include "bro.hpp"

int main(int argc, const char** argv){
	bro::Bro bro(argc, argv);
	bro.log.error("Hello {}!", "World");
	bro.log.warning("Hello {}!", "World");
	bro.log.info("Hello {}!", "World");
	bro.log.info("Src: {}", bro.src);
	bro.log.info("Exe: {}", bro.exe);
	bro.log.info("Argc: {}", bro.args.size());

	bro::File src(bro.src);
	bro.log.info("Src file: {}", src);

	bro::File exe(bro.exe);
	bro.log.info("Exe file: {}", exe);

	return 0;
}
