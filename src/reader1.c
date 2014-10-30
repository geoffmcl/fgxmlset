/**
 * section: xmlReader
 * synopsis: Parse an XML file with an xmlReader
 * purpose: Demonstrate the use of xmlReaderForFile() to parse an XML file
 *          and dump the informations about the nodes found in the process.
 *          (Note that the XMLReader functions require libxml2 version later
 *          than 2.6.)
 * usage: reader1 <filename>
 * test: reader1 test2.xml > reader1.tmp && diff reader1.tmp $(srcdir)/reader1.res
 * author: Daniel Veillard
 * copy: see Copyright for the status of this software.
 * 20141029 - from : http://www.xmlsoft.org/examples/reader1.c
 */

#include <stdio.h>
#ifndef _MSC_VER
#include <string.h> // strdup(), ...
#endif
#if defined (_WIN32) && !defined(__CYGWIN__)
#if defined (_MSC_VER) || defined(__BORLANDC__)
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif /* _MSC_VER */
#endif /* _WIN32 */
#include <libxml/xmlreader.h>
#include "gen_utils.hxx"

#ifndef SPRTF
#define SPRTF printf
#endif

static const char *filename = "X:\\fgdata\\Aircraft\\777\\777-200-set.xml";


#ifdef LIBXML_READER_ENABLED

/**
 * processNode:
 * @reader: the xmlReader
 *
 * Dump information about the current node
 */
static void processNode(xmlTextReaderPtr reader) 
{
    int nt, depth, attcnt;
    const xmlChar *name, *value, *att;
    const char *type;
    xmlAttrPtr  kid;
    xmlNodePtr  child;
    xmlNodePtr	cur = xmlTextReaderCurrentNode(reader);
    depth = xmlTextReaderDepth(reader);
    nt = xmlTextReaderNodeType(reader);
    type = getNodeTypeStg(nt);
    name = xmlTextReaderConstName(reader);
    if (name == NULL)
	name = BAD_CAST "--";
    value = xmlTextReaderConstValue(reader);
    attcnt = xmlTextReaderAttributeCount(reader);
    SPRTF("%d %s (%d) %s %d %d %d", 
	    depth,
        type,
	    nt,
	    name,
	    xmlTextReaderIsEmptyElement(reader),
	    xmlTextReaderHasValue(reader),
        attcnt
        );
    if (attcnt) {
        if (cur && cur->properties) {
            kid = cur->properties;
            if (kid->name && kid->children) {
                child = kid->children;
                if (child->content) {
                    SPRTF(" %s=%s", kid->name, child->content);
                }
            }
        }
        att = xmlTextReaderGetAttributeNo(reader,1);
        if (att) {
            //SPRTF(" %s",att);
            xmlFree((void *)att);
        }
    }
    if (value == NULL)
	    SPRTF("\n");
    else {
        if (xmlStrlen(value) > 40)
            SPRTF(" %.40s...\n", value);
        else
	        SPRTF(" %s\n", value);
    }
}

/**
 * streamFile:
 * @filename: the file name to parse
 *
 * Parse and print information about an XML file.
 */
static int streamFile(const char *filename) 
{
    int iret = 0;
    xmlTextReaderPtr reader;
    int ret;

    reader = xmlReaderForFile(filename, NULL, 0);
    if (reader != NULL) {
        ret = xmlTextReaderRead(reader);
        while (ret == 1) {
            processNode(reader);
            ret = xmlTextReaderRead(reader);
        }
        xmlFreeTextReader(reader);
        if (ret != 0) {
            SPRTF( "%s : failed to parse\n", filename);
            iret = 0;
        }
    } else {
        SPRTF( "Unable to open %s\n", filename);
        iret = 1;
    }
    return iret;
}

int main(int argc, char **argv)
{
    int iret = 0;

    /*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION

    if (argc != 2) {
        SPRTF("Using default file '%s'\n", filename);
    } else {
        filename = strdup(argv[1]);
        SPRTF("Using file '%s'\n", filename);
    }

    iret = streamFile(filename);

    /*
     * Cleanup function for the XML library.
     */
    xmlCleanupParser();
    /*
     * this is to debug memory for regression tests
     */
    xmlMemoryDump();

    return iret;
}

#else // !LIBXML_READER_ENABLED
int main(void) {
    SPRTF("XInclude support not compiled in\n");
    return 1;
}
#endif // LIBXML_READER_ENABLED y/n

// eof
