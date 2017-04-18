xenics:  xenics.cpp xenics.i
	swig -c++ -python -o xenics_wrap.cpp xenics.i
	sed -e 's?load_buffer_file(\".*\");?load_buffer_file(\"'`pwd`'/xccfgh5_2.ttb\");?g' xenics.cpp > xenics_copy.cpp
	g++ -c -fPIC -O -ansi -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES -D_DEBUG -g3 -L/usr/local/lib/ -I/usr/local/include/ xenics_copy.cpp -o xenics.o
	rm -f xenics_copy.cpp
	g++ -c -fPIC -O -ansi -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES -D_DEBUG -g3 xenics_wrap.cpp -o xenics_wrap.o -I/Users/xcam/anaconda2/include/python2.7/ -I/Users/xcam/anaconda2/lib/python2.7/site-packages/numpy/core/include/
	ld -bundle -flat_namespace -macosx_version_min 10.7 -undefined suppress -lusb -o _xenics.so xenics.o xenics_wrap.o
