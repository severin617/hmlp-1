mkdir build
cp tools.i build/
cd build
swig -o toolswrap.cpp -c++ -python tools.i
/opt/intel/compilers_and_libraries_2018.2.199/linux/bin/intel64/icpc -o tools_wrap.os -c -I/opt/apps/intel18/python3/3.7.0/include/python3.7m -I../gofmm/ -I../include/ -I../frame/ -I ../frame/base/ -I../frame/containers/ toolswrap.cpp -fPIC
