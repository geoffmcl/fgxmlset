fgxmlset project
================

Description
-----------

This small project was to explore the <a href="http://www.xmlsoft.org/"
target="_blank">libXml2</a> UI, and building 
from source requires finding libXml2 in your system.

It is built to *only* handle <a href="http://flightgear.org" target="_blank">FlightGear</a>
special Aircraft *-set.xml file. And the file should be in the standard fgdata base data 
directory, since it has some quite special relative path handling. And it *must* find the 
correct 'root' node name, &lt;PropertyList&gt;

But aside from that it is a reasonable example of how to find and extract specific data 
from a 'known' xml file.

Building
--------

The building is using cmake to find libXml2, and generate 
the desired build files.

So for a standard unix makefile build  
$ cd build  
$ cmake .. [-DCMAKE_INSTALL_PREFIX]  
$ make  
$ [sudo] make install (if desired)  

For Windows, use the cmake GUI and set source directory, the binary directory to build, or 
&gt; cd build  
&gt; cmake .. [-DCMAKE_INSTALL_PREFIX]  
&gt; cmake --build . --config Release  
&gt; cmake --build . --config Release --target INSTALL (if desired)  


Regards,  
Geoff.  
20141030  

;eof



