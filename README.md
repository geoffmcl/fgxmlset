fgxmlset project
================

This small project was to explore the <a href="http://www.xmlsoft.org/"
target="_blank">libXml2</a> UI, and building 
from source requires finding libXml2 in your system.

The building is using cmake to find libXml2, and generate 
the desired build files.

So for a standard unix makefile build  
$ cd build  
$ cmake .. [-DCMAKE_INSTALL_PREFIX]  
$ make  
$ [sudo] make install (if desired)  

For Windows, use the cmake GUI and set source directory, the binary directory to build, or 
> cd build  
> cmake .. [-DCMAKE_INSTALL_PREFIX]  
> cmake --build . --config Release
> cmake --build . --config Release --target INSTALL (if desired)  


Regards,  
Geoff.  
20141030  

# eof


