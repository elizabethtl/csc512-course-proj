this is the submission repo for csc512

the pass is in [part1/](part1/)

to build the pass, run:
```
mkdir build
cd build
cmake ..
make
cd ..
```

to test the pass on C programs
in the root directory, run with debug info
```
clang -O0 -g -fno-discard-value-names -fpass-plugin=`echo build/part1/part1pass.*` example.c
```

the outputs of running the pass on the test files are in [test-outputs/](test-outputs/)


tests 3-5 are interactive, so I also included the executables of the programs:
- [test3-snake-game](test3-snake-game)
- [test4-library-management-system](test4-library-management-system)
- [test5-cafeteria-system](test5-cafeteria-system)

running the executable might generate txt files related to the systems
