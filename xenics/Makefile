canopy_app_dir = `ls -d /Applications/Canopy.app/appdata/canopy-* | tail -1`/
username = `whoami`


xenics:  xenics.cpp xenics.i
	swig -c++ -python -o xenics_wrap.cpp xenics.i
	g++ -c -fPIC -O -ansi -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES -D_DEBUG -g3 -L/usr/local/lib/ -I/usr/local/include/ xenics.cpp -o xenics.o 
	g++ -c -fPIC -O -ansi -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES -D_DEBUG -g3 xenics_wrap.cpp -o xenics_wrap.o -I$(canopy_app_dir)Canopy.app/Contents/include/python2.7/ -I/Users/$(username)/Library/Enthought/Canopy_64bit/User/lib/python2.7/site-packages/numpy/core/include/
	ld -bundle -flat_namespace -macosx_version_min 10.7 -undefined suppress -lusb -o _xenics.so xenics.o xenics_wrap.o
