to build
```
mkdir build
cd build
cmake ..
make
cd ..
```

then in the root directory, run
```
clang -fpass-plugin=`echo build/part1/part1pass.*` example.c
```


with debug info
```
clang -O0 -g -fno-discard-value-names -fpass-plugin=`echo build/part1/part1pass.*` example.c
```