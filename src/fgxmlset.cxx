/*\
 * fgxmlset.cxx
 *
 * Copyright (c) 2014 - Geoff R. McLane
 * Licence: GNU GPL version 2
 *
\*/

#include <stdio.h>
#include <sstream>
#if defined (_WIN32) && !defined(__CYGWIN__)
#if defined (_MSC_VER) || defined(__BORLANDC__)
#include <winsock2.h>
#include <direct.h> // for getcwd(), ...

#pragma comment(lib, "ws2_32.lib")
// #define gettimeofday(p1,p2)
#endif /* _MSC_VER */
#endif /* _WIN32 */
#include <stdint.h>
#ifdef _MSC_VER
#include <libxml/libxml.h>
#else
#include <string.h> // for strcmp(), ...
#include <unistd.h> // getcwd(), ...
#endif
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

#ifndef DEF_VERB
#define DEF_VERB 1
//#define DEF_VERB 9
#endif

#ifndef SPRTF
#define SPRTF printf
#endif
// forward refs
static void walkDoc(xmlDocPtr doc, int lev, const char *file);
int check_doc( xmlDocPtr doc );

static std::string main_file;
static vSTG loaded_files;
static const char *module = "fgxmlset";
static int options = XML_PARSE_COMPACT | XML_PARSE_BIG_LINES;
static const char *root_node = "PropertyList";
static const char *ac_folder = "Aircraft";
static const char *filename = "X:\\fgdata\\Aircraft\\777\\777-200-set.xml";
//static const char *filename = "X:\\fgdata\\Aircraft\\ufo\\ufo-set.xml";
//static const char *filename = "ufo-set.xml";
static char *out_file = 0;

static std::string root_path;
static std::string ac_path;
static int scanned_count = 0;
static double bgn_secs;
static uint64_t bytes_processed = 0;
static int verbosity = DEF_VERB;

#define VERB1 (verbosity >= 1)
#define VERB2 (verbosity >= 2)
#define VERB5 (verbosity >= 5)
#define VERB9 (verbosity >= 9)

//////////////////////////////////////////////////////////
// flags 
static int parsing_flag = 0;
#define flg_sim         0x00000001
#define flg_author      0x00000002
#define flg_status      0x00000004
#define flg_rating      0x00000008
#define flg_rFDM        0x00000010
#define flg_rsystems    0x00000020
#define flg_rcockpit    0x00000040
#define flg_rmodel      0x00000080
#define flg_avers       0x00000100
#define flg_fmodel      0x00000200
#define flg_path        0x00000400

typedef struct tagFLGITEMS {
    std::string authors;
    std::string status;
    std::string rFDMr;
    std::string rsystems;
    std::string rcockpit;
    std::string rmodel;
    std::string avers;
    std::string fmodel;
    vSTG acpaths;
}FLGITEMS, *PFLGITEMS;

typedef struct tagFLG2TXT {
    int flag;
    const char *text;
}FLG2TXT, *PFLG2TXT;

static FLG2TXT flg2txt[] = {
    { flg_sim, "sim" }, // 0x00000001
    { flg_author, "author" },   //  0x00000002
    { flg_status, "status" },   // 0x00000004
    { flg_rating, "rating" },   // 0x00000008
    { flg_rFDM, "FDM" }, //         0x00000010
    { flg_rsystems, "systems" }, //     0x00000020
    { flg_rcockpit, "cockpit" }, //     0x00000040
    { flg_rmodel, "model" }, //       0x00000080
    { flg_avers, "aircraft-version" },   //       0x00000100
    { flg_fmodel, "flight-model" }, //      0x00000200
    { flg_path, "path" },   // 0x00000400
    // last entry
    { 0, 0 }
};

static int simauthor = (flg_sim | flg_author);
static int simstatus = (flg_sim | flg_status);
static int simrFDM  = (flg_sim | flg_rating | flg_rFDM);
static int simrsystems  = (flg_sim | flg_rating | flg_rsystems);
static int simrcockpit  = (flg_sim | flg_rating | flg_rcockpit);
static int simrmodel  = (flg_sim | flg_rating | flg_rmodel);
static int simavers  = (flg_sim | flg_avers);
static int simfmod  = (flg_sim | flg_fmodel);

static int simmodpath = flg_path;
//static int simmodpath = (flg_sim | flg_path);
//static int simmodpath = (flg_sim | flg_rmodel | flg_path);

void add_parsing_flag( char *element )
{
    PFLG2TXT f2t = flg2txt;
    int flag;
    while (f2t->text) {
        if (strcmp(f2t->text,element) == 0) {
            flag = f2t->flag;
            parsing_flag |= flag;
            flag = parsing_flag;
            ///SPRTF("DBG: Add flag '%s' %#X\n", element, flag);
            return;
        }
        f2t++;
    }
}

void remove_parsing_flag( char *element )
{
    PFLG2TXT f2t = flg2txt;
    int flag;
    while (f2t->text) {
        if (strcmp(f2t->text,element) == 0) {
            flag = f2t->flag;
            if (parsing_flag & flag) {
                parsing_flag &= ~flag;
                flag = parsing_flag;
                // SPRTF("DBG: Removed flag '%s' %#X\n", element, flag);
            } else {
                flag = 0;
            }
            return;
        }
        f2t++;
    }
}

bool has_no_excluded_folders(std::string &path)
{
    vSTG vs = PathSplit(path);
    size_t ii, max = vs.size();
    std::string s;
    for (ii = 0; ii < max; ii++) {
        s = vs[ii];
        if (strcmp(s.c_str(),"Instruments-3d") == 0)
            return false;
        if (strcmp(s.c_str(),"Generic") == 0)
            return false;
    }
    return true;
}

PFLGITEMS pflgitems = 0;
#define MEOL std::endl

void show_items_found()
{
    if (!pflgitems)
        return;
    size_t ii, max = pflgitems->acpaths.size();
    std::stringstream ss;

    ss << module << ": Processed the main file '" << main_file << MEOL;
    ss << "Items found in scan of " << scanned_count << " xml file(s), " << nice_num(GetNxtBuf(),uint64_to_stg(bytes_processed));
    ss << " bytes, in " << get_seconds_stg(get_seconds() - bgn_secs) << MEOL;

    if (pflgitems->authors.size())
        ss << "author(s)       : " << pflgitems->authors << MEOL;
    if (pflgitems->status.size())
        ss << "status          : " << pflgitems->status << MEOL;
    if (pflgitems->rFDMr.size())
        ss << "rating FDM      : " << pflgitems->rFDMr << MEOL;
    if (pflgitems->rsystems.size())
        ss << "rating systems  : " << pflgitems->rsystems << MEOL;
    if (pflgitems->rcockpit.size())
        ss << "rating cockpit  : " << pflgitems->rcockpit << MEOL;
    if (pflgitems->rmodel.size())
        ss << "rating model    : " << pflgitems->rmodel << MEOL;
    if (pflgitems->avers.size())
        ss << "aircarft-version: " << pflgitems->avers << MEOL;
    if (pflgitems->fmodel.size())
        ss << "flight-model    : " << pflgitems->fmodel << MEOL;
    if (max) {
        if (max == 1) {
            ss << "model-file      : " << pflgitems->acpaths[0] << MEOL;
        } else {
            ss << "Got " << max << " 'model' files..." << MEOL;
            for (ii = 0; ii < max; ii++) {
                ss << (ii + 1) << ": " << pflgitems->acpaths[ii] << MEOL;
            }
        }
    }
    if (VERB1) {
        if (VERB2)
            SPRTF("\n");
        SPRTF("%s", ss.str().c_str());
    }
    if (out_file) {
        FILE *fp = fopen(out_file,"w");
        if (fp) {
            size_t res, len = ss.str().size();
            res = fwrite(ss.str().c_str(),1,len,fp);
            fclose(fp);
            if (res == len) {
                SPRTF("%s: Results witten to file '%s'\n", module, out_file);
            } else {
                SPRTF("WARNING: Write to file '%s' failed! req %d, wrote %d\n", out_file, (int)len, (int)res);
            }
        } else {
            SPRTF("WARNING: Unable to Write to file '%s'!\n", out_file);
        }
    }

    // clean up...
    pflgitems->acpaths.clear();
    delete pflgitems;
    pflgitems = 0;
}


bool Already_Loaded(std::string &file)
{
    size_t ii, max = loaded_files.size();
    std::string s;
    for (ii = 0; ii < max; ii++) {
        s = loaded_files[ii];
        if (file == s)
            return true;
    }
    return false;
}

bool has_file_path( char * file )
{
    size_t ii, len = strlen(file);
    int c;
    for (ii = 0; ii < len; ii++) {
        c = file[ii];
        if (( c == '/' ) || ( c == '\\' ))
            return true;
    }
    return false;
}

#define GOT_FLG(a) ((parsing_flag & a) == a)


int save_text_per_flag( char *value, std::string &mfile, const char *file )
{
    int iret = 0;
    if (!value)
        return 0;
    if (parsing_flag & flg_sim) {
        if (pflgitems == 0)
            pflgitems = new FLGITEMS;
        if (GOT_FLG(simauthor)) {
            // save the author
            pflgitems->authors = value;
        }
        if (GOT_FLG(simstatus)) {
            pflgitems->status = value;
        }
        if (GOT_FLG(simrFDM)) {
            pflgitems->rFDMr = value;
        }
        if (GOT_FLG(simrsystems)) {
            pflgitems->rsystems = value;
        }
        if (GOT_FLG(simrcockpit)) {
            pflgitems->rcockpit = value;
        }
        if (GOT_FLG(simrmodel)) {
            pflgitems->rmodel = value;
        }
        if (GOT_FLG(simavers)) {
            pflgitems->avers = value;
        }
        if (GOT_FLG(simfmod)) {
            pflgitems->fmodel = value;
        }
        if (GOT_FLG(simmodpath)) {
            // 4 TEXT (3) #text 0 1 0 path=PropertyList\sim\model\path Aircraft/777/Models/777-200.xml
            std::string ifile = ac_path;
            ifile += PATH_SEP;
            ifile += value;
            ensure_native_sep(ifile);
            if (is_file_or_directory( ifile.c_str() ) == 1) {
                if (find_extension(ifile,".xml")) {
                    if (has_no_excluded_folders(ifile)) {
                        iret = 1;
                        mfile = ifile;
                    }
                } else if (find_extension(ifile,".ac")) {
                    pflgitems->acpaths.push_back(ifile);
                }
            } else if (find_extension(ifile,".xml") || find_extension(ifile,".ac")) {
                // can NOT find a file we are interested in, give a warning
                if (has_file_path((char *)value)) { // forget it if it has no path
                    SPRTF("WARNING: Unable to find the model file '%s'\n", ifile.c_str());
                } else if (find_extension(ifile,".ac") && (pflgitems->acpaths.size() == 0)) {
                    // try REAL HARD to find this file... as the first .ac
                    // maybe it is releative to the current file
                    ifile = file;
                    ifile = get_path_only(ifile);
                    ifile += PATH_SEP;
                    ifile += value;
                    if (is_file_or_directory(ifile.c_str()) == 1) {
                        // store this as the .ac file
                        pflgitems->acpaths.push_back(ifile);
                    } else {
                        SPRTF("WARNING: Unable to find ac file '%s' absolute or ralative!\n", value );
                    }
                }
            }

        }
    }

    return iret;
}


//static int debug = 1;
static vSTG xmlpath;
//static vSTG path_stack;
#ifdef LIBXML_READER_ENABLED
/**
 * processNode:
 * @reader: the xmlReader
 *
 * Dump information about the current node
 */
static void processNode(xmlTextReaderPtr reader, int lev, const char *file) 
{
    int nt, depth, attcnt, empty;
    const xmlChar *name, *value;
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
        else if (nt == XML_READER_TYPE_COMMENT)
            show = false;
    }
    if (nt == XML_READER_TYPE_ELEMENT) {
        if (!empty) {
            add_parsing_flag((char *)name );
            xmlpath.push_back((char *)name);
        }
    } else if (nt == XML_READER_TYPE_END_ELEMENT) {
        remove_parsing_flag((char *)name );
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
            path += PATH_SEP;
        path += s;
    }
    //if (strcmp(path.c_str(),"PropertyList\\sim\\model\\path") == 0) {
    //    ii = 0;
    //}
    if (show) {
        int i = lev;
        std::string ind;
        while (i--)
            ind += "   ";
        if (VERB5) {
            SPRTF("%s%d %s (%d) %s %d %d %d", ind.c_str(),
	            depth,
                type,
	            nt,
	            name,
	            empty,
	            xmlTextReaderHasValue(reader),
                attcnt
                );
        }
        if (attcnt) {
            if (cur && cur->properties) {
                kid = cur->properties;
                if (kid->name && kid->children) {
                    child = kid->children;
                    if (child->content) {
                        if (VERB5) {
                            SPRTF(" %s=%s", kid->name, child->content);
                        }
                        if (strcmp((char *)kid->name,"include") == 0) {
                            ifile = root_path;
                            if (ifile.size())
                                ifile += PATH_SEP;
                            ifile += (char *)child->content;
                            ensure_native_sep(ifile);
                            if (is_file_or_directory(ifile.c_str()) != 1) {
                                ifile = ac_path;
                                if (ifile.size())
                                    ifile += PATH_SEP;
                                ifile += (char *)child->content;
                                ensure_native_sep(ifile);
                                if (is_file_or_directory(ifile.c_str()) != 1) {
                                    ifile = file;   // this file
                                    ifile = get_path_only(ifile);
                                    if (ifile.size())
                                        ifile += PATH_SEP;
                                    ifile += (char *)child->content;
                                    ensure_native_sep(ifile);
                                }
                            }
                        }
                    }
                }
            }
            //att = xmlTextReaderGetAttributeNo(reader,1);
            //if (att) {
                //SPRTF(" %s",att);
            //    xmlFree((void *)att);
            //}
        }
        if (VERB5) {
            SPRTF(" path=%s", path.c_str());
            if (value == NULL)
	            SPRTF("\n");
            else {
                if (xmlStrlen(value) > 40)
                    SPRTF(" %.40s...\n", value);
                else
	                SPRTF(" %s\n", value);
            }
        }
        if (nt == XML_READER_TYPE_TEXT) {
            if (save_text_per_flag((char *)value, s, file)) {
                if ((is_file_or_directory(s.c_str()) == 1) && !Already_Loaded(s)) {
                    loaded_files.push_back(s);  // mark as done
                    xmlDocPtr idoc = xmlReadFile(s.c_str(), NULL, options);
                    if (idoc) {
                        if (VERB5)
                            SPRTF("\n");
                        if (VERB2) {
                            SPRTF("%s: Loaded file '%s'\n", module, s.c_str() );
                        }
                        if (!check_doc(idoc)) {
                            bytes_processed += get_last_file_size();
                            scanned_count++;
                            //path_stack.push_back(path);
                            xmlpath.clear();
                            walkDoc(idoc, lev + 1, s.c_str());
                            xmlFreeDoc(idoc);       // free document
                            xmlpath.clear();
                            xmlpath = PathSplit(path);
                        }
                    } else {
                        SPRTF("WARNING: Failed to load include file '%s'!\n", s.c_str());
                    }
                }
            }
        }
        if (ifile.size()) {
            if ((is_file_or_directory(ifile.c_str()) == 1)  && !Already_Loaded(ifile)) {
                loaded_files.push_back(ifile);  // mark as done
                xmlDocPtr idoc = xmlReadFile(ifile.c_str(), NULL, options);
                if (idoc) {
                    if (VERB5)
                        SPRTF("\n");
                    if (VERB2) {
                        SPRTF("%s: Loaded file '%s'\n", module, ifile.c_str() );
                    }
                    if (!check_doc(idoc)) {
                        scanned_count++;
                        bytes_processed += get_last_file_size();
                        xmlpath.clear();
                        walkDoc(idoc, lev + 1, ifile.c_str());
                        xmlFreeDoc(idoc);       // free document
                        xmlpath.clear();
                        xmlpath = PathSplit(path);
                    }
                } else {
                    SPRTF("WARNING: Failed to load include file '%s'!\n", ifile.c_str());
                }
            }
        }
    }
}


static void walkDoc(xmlDocPtr doc, int lev, const char *file )
{
    xmlTextReaderPtr reader;
    int ret = -1;
    int node_cnt = 0;
#if 0 // 0000000000000000000000000000000000
#ifdef LIBXML_PATTERN_ENABLED
    xmlNodePtr root;
    const xmlChar *namespaces[22];
    int i;
    xmlNsPtr ns;

    root = xmlDocGetRootElement(doc);
    for (ns = root->nsDef, i = 0;ns != NULL && i < 20;ns=ns->next) {
        namespaces[i++] = ns->href;
        namespaces[i++] = ns->prefix;
    }
    namespaces[i++] = NULL;
    namespaces[i] = NULL;
#endif /* LIBXML_PATTERN_ENABLED */
#endif // 000000000000000000000000000000000

    reader = xmlReaderWalker(doc);
    if (reader != NULL) {
	    //if ((timing) && (!repeat)) {
	    //    startTimer();
	    //}
	    ret = xmlTextReaderRead(reader);
	    while (ret == 1) {
            processNode(reader, lev, file);
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
    if (ret == 0) {
        if (VERB5)
            SPRTF("%s: Done file '%s', %d nodes...\n\n", module, file, node_cnt );
    }
}

#endif /* LIBXML_READER_ENABLED */


int check_doc( xmlDocPtr doc )
{
    xmlNodePtr cur = xmlDocGetRootElement(doc);
    if (cur == NULL) {
        SPRTF("%s: empty document\n", module);
		xmlFreeDoc(doc);
        return 1;
    }
    if (xmlStrcmp(cur->name, (const xmlChar *) root_node) ) {
		SPRTF("%s: document of the wrong type, root node != %s\n", module, root_node);
		xmlFreeDoc(doc);
		return 1;
	}
    return 0;
}

void set_root_paths( std::string &file)
{
    std::string s;
    root_path = get_path_only(file);
    if (root_path.size() == 0) {
        char *tb = GetNxtBuf();
        *tb = 0;
        if (getcwd(tb,264)) {
            if (*tb) {
                root_path = tb;
                if (VERB5) {
                    SPRTF("%s: getcwd() returned '%s'\n", module, root_path.c_str());
                }
                s = root_path;
                s += PATH_SEP;
                s += file;
                file = s;   // set the fully qualified file name
                if (VERB1) {
                    SPRTF("%s: Set full qualified file to '%s\n", module, file.c_str());
                }
            }
        } else {
            SPRTF("%s: Appears 'getcwd()' FAILED! Paths may be wrong. Try using a fully\n", module);
            SPRTF(" qualified file name.\n");
        }
    }
    vSTG vs = PathSplit(file);
    size_t ii,max = vs.size();
    if (max > 1) {
        for (ii = 0; ii < max; ii++) {
            s = vs[ii];
            if (strcmp(s.c_str(),ac_folder) == 0) {
                break;
            }
            if (ac_path.size() || ii)
                ac_path += PATH_SEP;
            ac_path += s;
        }
    } 

}

void give_help( char *name )
{
    std::string file = name;
    file = get_file_only(file);
    printf("%s [options] input-fg-xml-set-file\n", file.c_str());
    printf("Options:\n");
    printf(" --help (-h or -?) = This help and exit(2)\n");
    printf(" --verb[nn]        = Bump or set verbosity. (def=%d)\n", verbosity);
    printf(" --log <file>      = Set the log file to be used for output. (def=%s)\n", get_log_file());
    printf(" --out <file>      = Write results to out file. (def=%s)\n",
        out_file ? out_file : "none");
    printf("\n");
    printf("Will parse the input as a FlightGear '-set' xml file, and extract information.\n");
    printf("While there is an attempt to handle a relative file name, it is certainly better\n");
    printf("to use a fully qualified input file name.\n");
    printf("\n");
    printf("NOTE: This will ONLY work for FlightGear Aircraft '-set.xml' files that are\n");
    printf("      in the fgdata folder since there are some very FG specific relative\n");
    printf("      paths applied.\n");
    printf("\n");
    printf("But in general terms it could be taken as a reasonable example of how to\n");
    printf("extract information from any xml file using the services of libXml2.\n");
    
}

#define ISDIGIT(a) (( a >= '0' ) && ( a <= '9' ))

int scan_for_log_file( int argc, char **argv )
{
    int c, i;
    int i2;
    char *arg, *sarg;
    for (i = 1; i < argc; i++) {
        i2 = i + 1;
        arg = argv[i];
        if (*arg == '-') {
            sarg = &arg[1];
            while (*sarg == '-')
                sarg++;
            c = *sarg;
            if (c == 'l') {
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    set_log_file(sarg,false);
                } else {
                    printf("%s: Expected log file name to follow '%s'! Aborting...\n", module, arg );
                    return 1;
                }
            }
        }
    }
    return 0;
}


int parse_args( int argc, char **argv )
{
    int c, i;
    int i2;
    char *arg, *sarg;
    if (scan_for_log_file(argc,argv))
        return 1;
    for (i = 1; i < argc; i++) {
        arg = argv[i];
        i2 = i + 1;
        if (*arg == '-') {
            sarg = &arg[1];
            while (*sarg == '-')
                sarg++;
            c = *sarg;
            switch (c) {
            case 'h':
            case '?':
                give_help(argv[0]);
                return 1;
                break;
            case 'l':
                i++;    // already checked an dealt with
                break;
            case 'o':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    out_file = strdup(sarg);
                } else {
                    SPRTF("%s: Expected output file name to follow '%s'! Aborting...\n", module, arg);
                    return 1;
                }
                break;
            case 'v':
                sarg++;
                verbosity++;
                while (*sarg && !ISDIGIT(*sarg)) {
                    if (*sarg == 'v')
                        verbosity++;
                    sarg++;
                }
                if (ISDIGIT(*sarg))
                    verbosity = atoi(sarg);
                if (VERB1)
                    printf("Set verbosity to %d\n", verbosity);
                break;
            default:
                printf("%s: Unknown argument '%s'! Aborting...\n", module, arg );
                return 1;
            }
        } else {
            if (is_file_or_directory(arg) != 1) {
                printf("%s: Unable to 'stat' file '%s'! Aborting...\n", module, arg);
                return 1;
            }
            filename = strdup(arg);
        }

    }
    return 0;
}

#if 0 // 0000000000000000000000000000000000000000000000
void test_stg()
{
    std::string path ="/media/Disk2/ufo-set.txt";
    ensure_unix_sep(path);
    vSTG vs = PathSplit(path);
    std::string file = get_file_only(path);
    path = get_path_only(path);
    exit(1);
}
#endif // 00000000000000000000000000000000000000000000

// main() OS entry
int main( int argc, char **argv )
{
    // test_stg();
    int iret = parse_args(argc,argv);
    if (iret)
        return iret;
    xmlDocPtr doc = NULL;
    bgn_secs = get_seconds();
    main_file = filename;
    ensure_native_sep(main_file);
    if (is_file_or_directory(main_file.c_str()) != 1) {
        SPRTF("%s: Failed to 'stat' file '%s'\n", module, main_file.c_str() );
        return 1;
    }
    bytes_processed += get_last_file_size();
    LIBXML_TEST_VERSION
    doc = xmlReadFile(main_file.c_str(), NULL, options);
    if (doc == NULL) {
        SPRTF("%s: Failed to load file '%s'\n", module, main_file.c_str() );
        return 1;
    }
    if (VERB1) {
        SPRTF("%s: Processing file '%s'...\n", module, main_file.c_str() );
    }
    if (check_doc(doc)) {
        xmlCleanupParser();    // Free globals
        return 1;
    }

    set_root_paths(main_file);
    scanned_count++;
    loaded_files.push_back(main_file);
    walkDoc(doc, 0, main_file.c_str());

    xmlFreeDoc(doc);       // free document
    xmlCleanupParser();    // Free globals

    show_items_found();

    return iret;
}


// eof = fgxmlset.cxx

