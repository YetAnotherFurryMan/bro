# bro
Bro is a simple header-only C++17 library made to replace CMake in my projects. It was inspired by [nob.h](https://github.com/tsoding/nob.h). 

## How to use it? 
Simply create a file `bro.cpp`, include there `bro.hpp`, describe your project (and commands used) and perform `bro.build();`. See local `bro.cpp` as an example.

## Examples

Lets start simple, imagine you have a hello wirld written in hello.cpp.

``` C++
// Include bro header
#import "bro.hpp"

int main(int argc, const char** argv){
    // Create bro main object
    bro::Bro bro(argc, argv);

    // Ensure the newiest version
    bro.fresh();

    // Register a command to compile your sources
    std::size_t cxx_ix = bro.cmd("cxx", {std::string{bro.getFlag("cxx")}, "-c", "$in", "-o", "$out"});

    // Register linking command
    std::size_t ld_ix = bro.cmd("ld", {std::string{bro.getFlag("ld")}, "$in", "-o", "$out"});

    // Create a new transform stage and add the cxx command
    std::size_t obj_ix = bro.transform("obj", ".o");
    bro.useCmd(obj_ix, cxx_ix, ".cpp");

    // Create a new link stage
    std::size_t bin_ix = bro.link("bin", "$mod");
    bro.useCmd(bin_ix, ld_ix, ".o");

    // Create a module and add the sources
    std::size_t hello_ix = bro.mod("hello", false);
    bro.mods[hello_ix].addFile("hllo.cpp");

    // Apply stages to the module
    bro.applyMod(obj_ic, hello_ix);
    bro.applyMod(bin_ix, hello_ix);

    // run() is an extended build()
    bro.run();
}
```

## Features
- [x] Run commands (sync/async)
- [x] Command pools and queues
- [x] Build a project where modules depend on each other
- [x] Build a project where modules depend on external modules (np. local libraries)
- [ ] Download external dependencies from git(hub)
- [x] Command-line flags
- [x] Choose between clang, gcc or other C compiler via flags
- [ ] Support for Mercurial
- [ ] Support for Git
