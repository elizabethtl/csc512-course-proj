This is the submission repo for csc512

The pass is in [part1/](part1/)

To build the pass, run:
```
mkdir build
cd build
cmake ..
make
cd ..
```

To test the pass on C programs
In the root directory, run with debug info
```
clang -O0 -g -fno-discard-value-names -fpass-plugin=`echo build/part1/part1pass.*` example.c
```

The outputs of running the pass on the test files are in [test-outputs/](test-outputs/)

The output from running the pass look something like
```
Tracing variable defined by instruction:   %1 = load i32, i32* %n, align 4, !dbg !28
	Variable originates from an alloca:   %n = alloca i32, align 4
	Value has name: 1, value name: n
	Location: /home/elizabeth/Documents/csc512/project/csc512-course-proj/test1.c:6:4
	findDefUseChains()
  called function: __isoc99_scanf
	  --- SEMINAL INPUT ---
	   Value originates from scanf:   %call = call i32 (i8*, ...) @__isoc99_scanf(i8* noundef getelementptr inbounds ([7 x i8], [7 x i8]* @.str, i64 0, i64 0), i32* noundef %id, i32* noundef %n), !dbg !19 --
```

It includes the variable name and Location
If it finds that the variable is from a seminal input, it will print `--- SEMINAL INPUT ---` and additional information on where the input value is from

---


tests 3-5 are interactive, so I also included the executables of the programs:
- [test3-snake-game](test3-snake-game)
- [test4-library-management-system](test4-library-management-system)
- [test5-cafeteria-system](test5-cafeteria-system)

running the executable might generate txt files related to the systems
