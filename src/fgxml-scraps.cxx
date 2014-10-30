/*\
 * fgxmlset.cxx
 *
 * Copyright (c) 2014 - Geoff R. McLane
 * Licence: GNU GPL version 2
 *
\*/

#include <stdio.h>
#if defined (_WIN32) && !defined(__CYGWIN__)
#if defined (_MSC_VER) || defined(__BORLANDC__)
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
// #define gettimeofday(p1,p2)
#endif /* _MSC_VER */
#endif /* _WIN32 */

#include <libxml/libxml.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/debugXML.h>
#include <libxml/xmlerror.h>
#ifdef LIBXML_XINCLUDE_ENABLED
#include <libxml/xinclude.h>
#endif
#ifdef LIBXML_CATALOG_ENABLED
#include <libxml/catalog.h>
#endif
#include <libxml/globals.h>
#include <libxml/xmlreader.h>
#ifdef LIBXML_SCHEMATRON_ENABLED
#include <libxml/schematron.h>
#endif
#ifdef LIBXML_SCHEMAS_ENABLED
#include <libxml/relaxng.h>
#include <libxml/xmlschemas.h>
#endif
#ifdef LIBXML_PATTERN_ENABLED
#include <libxml/pattern.h>
#endif
#ifdef LIBXML_C14N_ENABLED
#include <libxml/c14n.h>
#endif
#ifdef LIBXML_OUTPUT_ENABLED
#include <libxml/xmlsave.h>
#endif

#include "gen_utils.hxx"
#include "sprtf.hxx"
#include "fgxmlset.hxx"

#ifndef SPRTF
#define SPRTF printf
#endif
// forward refs
static void walkDoc(xmlDocPtr doc, int lev, const char *file);
int check_doc( xmlDocPtr doc );

static const char *module = "fgxmlset";
static int options = XML_PARSE_COMPACT | XML_PARSE_BIG_LINES;
static const char *filename = "X:\\fgdata\\Aircraft\\777\\777-200-set.xml";
static std::string root_path;

static int verbosity = 0;
#define VERB1 (verbosity >= 1)
#define VERB2 (verbosity >= 2)
#define VERB5 (verbosity >= 5)
#define VERB9 (verbosity >= 9)

#ifdef LIBXML_PATTERN_ENABLED
//static const char *pattern = NULL;
//static xmlPatternPtr patternc = NULL;
//static xmlStreamCtxtPtr patstream = NULL;
#endif

static int debug = 1;
static vSTG xmlpath;
#ifdef LIBXML_READER_ENABLED
/**
 * processNode:
 * @reader: the xmlReader
 *
 * Dump information about the current node
 */
static void processNode(xmlTextReaderPtr reader, int lev) 
{
    int nt, depth, attcnt, empty;
    const xmlChar *name, *value, *att;
    const char *type;
    xmlAttrPtr  kid;
    xmlNodePtr  child;
    xmlNodePtr	cur = xmlTextReaderCurrentNode(reader);
    bool show = true;
    std::string s, path, ifile;
    size_t ii,max;
    depth = xmlTextReaderDepth(reader);
    nt = xmlTextReaderNodeType(reader);
    type = getNodeTypeStg(nt);
    name = xmlTextReaderConstName(reader);
    if (name == NULL)
	name = BAD_CAST "--";
    value = xmlTextReaderConstValue(reader);
    attcnt = xmlTextReaderAttributeCount(reader);
    empty = xmlTextReaderIsEmptyElement(reader);

    if (!VERB9) {
        if (nt == XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
            show = false;
    }
    if (nt == XML_READER_TYPE_ELEMENT) {
        if (!empty) {
            xmlpath.push_back((char *)name);
        }
    } else if (nt == XML_READER_TYPE_END_ELEMENT) {
        if (xmlpath.size()) {
            s = xmlpath.back();
            if (strcmp((char *)name,s.c_str()) == 0) {
                xmlpath.pop_back();
            } else {
                SPRTF("WARNING: END element '%s' is NOT last on stack '%s'\n", name, s.c_str());
            }
        } else {
            SPRTF("WARNING: END element '%s' is NOT on stack!\n", name);
        }
    }
    max = xmlpath.size();
    for (ii = 0; ii < max; ii++) {
        s = xmlpath[ii];
        if (path.size())
            path += "/";
        path += s;
    }
    if (show) {
        int i = lev;
        std::string ind;
        while (i--)
            ind += "   ";

        SPRTF("%s%d %s (%d) %s %d %d %d", ind.c_str(),
	        depth,
            type,
	        nt,
	        name,
	        empty,
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
                        if (strcmp((char *)kid->name,"include") == 0) {
                            ifile = root_path;
                            ifile += PATH_SEP;
                            ifile += (char *)child->content;
                        }
                    }
                }
            }
            att = xmlTextReaderGetAttributeNo(reader,1);
            if (att) {
                //SPRTF(" %s",att);
                xmlFree((void *)att);
            }
        }
        SPRTF(" path=%s", path.c_str());
        if (value == NULL)
	        SPRTF("\n");
        else {
            if (xmlStrlen(value) > 40)
                SPRTF(" %.40s...\n", value);
            else
	            SPRTF(" %s\n", value);
        }
        if (ifile.size()) {
            xmlDocPtr idoc = xmlReadFile(ifile.c_str(), NULL, options);
            if (idoc) {
                SPRTF("\n%s: Loaded file '%s'\n", module, ifile.c_str() );
                if (!check_doc(idoc))
                    walkDoc(idoc, lev + 1, ifile.c_str());
                xmlFreeDoc(idoc);       // free document
            } else {
                SPRTF("WARNING: Failed to load include file '%s'!\n", ifile.c_str());
            }
        }
    }
}


static void walkDoc(xmlDocPtr doc, int lev, const char *file )
{
    xmlTextReaderPtr reader;
    int ret = -1;

#ifdef LIBXML_PATTERN_ENABLED
    xmlNodePtr root;
    const xmlChar *namespaces[22];
    int i, node_cnt = 0;
    xmlNsPtr ns;

    root = xmlDocGetRootElement(doc);
    for (ns = root->nsDef, i = 0;ns != NULL && i < 20;ns=ns->next) {
        namespaces[i++] = ns->href;
        namespaces[i++] = ns->prefix;
    }
    namespaces[i++] = NULL;
    namespaces[i] = NULL;

#endif /* LIBXML_PATTERN_ENABLED */
    reader = xmlReaderWalker(doc);
    if (reader != NULL) {
	    //if ((timing) && (!repeat)) {
	    //    startTimer();
	    //}
	    ret = xmlTextReaderRead(reader);
	    while (ret == 1) {
            if (node_cnt || (lev == 0))
                processNode(reader, lev);
            node_cnt++;
	        ret = xmlTextReaderRead(reader);
	    }
	    //if ((timing) && (!repeat)) {
	    //    endTimer("walking through the doc");
	    //}
	    xmlFreeTextReader(reader);
	    if (ret != 0) {
	        SPRTF("%s: failed to walk through the doc '%s'\n", module, file);
	    }
    } else {
	    SPRTF("%s: Failed to create a reader from the document '%s'\n", module, file);
    }
    if (ret == 0)
        SPRTF("%s: Done file '%s'\n", file );
}

#endif /* LIBXML_READER_ENABLED */

static int printReference (xmlNodePtr cur) {

	int cnt = 0;
    xmlChar *inc;
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
        if (cur->name)
            cnt += SPRTF("%s ", cur->name);
        inc = xmlGetProp(cur,(const xmlChar *)"include");
        if (inc) {
            cnt += SPRTF("include=%s ",inc);
            xmlFree(inc);
        }
#if 0 // 0000000000000000000000000000000000000
        xmlAttrPtr attr = cur->properties;
        for(; attr != NULL; attr = attr->next) {
            if (attr->name) {
                inc = xmlGetProp(cur, attr->name);
                if (inc) {
                    cnt += SPRTF("%s=%s ", attr->name, inc );
                    xmlFree(inc);
                }
            }
        }
#endif // 000000000000000000000000000000000000
        cur = cur->next;
	}
    return cnt;
}

static void print_element_names(xmlNode * a_node)
{
    xmlNode *cur_node = NULL;
    int cnt = 0;
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        cnt = 0;
        if (cur_node->type == XML_ELEMENT_NODE) {
            cnt += SPRTF("Element, name: %s ", cur_node->name);
        }
        cnt += printReference(cur_node);
        if (cnt)
            SPRTF("\n");
        print_element_names(cur_node->children);
    }
}
void
parseNode (xmlDocPtr doc, xmlNodePtr cur) {

	xmlChar *key;
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"keyword"))) {
	        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	        printf("keyword: %s\n", key);
	        xmlFree(key);
 	    }
    	cur = cur->next;
	}
    return;
}

void test_stg()
{
    std::string path = filename;
    ensure_unix_sep(path);
    vSTG vs = PathSplit(path);
    std::string file = get_file_only(path);
    path = get_path_only(path);
    exit(1);
}

int check_doc( xmlDocPtr doc )
{
    xmlNodePtr cur = xmlDocGetRootElement(doc);
    if (cur == NULL) {
        SPRTF("empty document\n");
		xmlFreeDoc(doc);
        return 1;
    }
    if (xmlStrcmp(cur->name, (const xmlChar *) "PropertyList")) {
		fprintf(stderr,"document of the wrong type, root node != PropertyList");
		xmlFreeDoc(doc);
		return 1;
	}
    return 0;
}

// main() OS entry
int main( int argc, char **argv )
{
    int iret = 0;
    xmlDocPtr doc = NULL;
    //xmlNodePtr cur;
    std::string file = filename;
    LIBXML_TEST_VERSION
    // test_stg();
    doc = xmlReadFile(filename, NULL, options);
    if (doc == NULL) {
        SPRTF("%s: Failed to load file '%s'\n", module, filename );
        return 1;
    }
    SPRTF("%s: Loaded file '%s'\n", module, filename );
    if (check_doc(doc)) {
        return 1;
    }
    root_path = get_path_only(file);
    //print_element_names(cur);
    // parseNode (doc, cur);
    walkDoc(doc, 0, filename);

    xmlFreeDoc(doc);       // free document
    xmlCleanupParser();    // Free globals
    return iret;
}


// eof = fgxmlset.cxx

