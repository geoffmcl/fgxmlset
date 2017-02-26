README.fgxmlset.xml - 20150113

Program notes:

*: No model file found in D:\FG\fgaddon\Aircraft\A380\A380-set.xml
There is a reference to XML/A380.xml but it is NOT loaded.
Bug was the parsing_flag was cleared during parsing an include,
now save and restore the flag...

*: File D:\FG\fgaddon\Aircraft\F-8E-Crusader\Crusader-set.xml
had \n in authors text - add a service agressive_trim() to remove
