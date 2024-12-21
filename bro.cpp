#include "bro.hpp"

int main(int argc, const char** argv){
	bro::Bro bro;
	bro.log.error("Hello {}!", "World");
	bro.log.warning("Hello {}!", "World");
	bro.log.info("Hello {}!", "World");
	return 0;
}
