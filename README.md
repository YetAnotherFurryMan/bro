# bro
Bro is a simple header-only C++17 library made to replace CMake in my projects. It was inspired by [nob.h](https://github.com/tsoding/nob.h). 

## How to use it? 
Simply create a file `bro.cpp`, include there `bro.hpp`, describe your project (and commands used) and perform `bro.build();`. See local `bro.cpp` as an example.

## Features
- [x] Run commands (sync/async)
- [x] Command pools and queues
- [x] Build a modular project
- [x] Build a project where modules depends on each other
- [x] Build a project where modules depends on external modules (np. local libraries)
- [ ] Download external dependencies from git(hub)
- [ ] App+engine project model
- [x] Command-line flags
- [x] Choose between clang and gcc via flags
