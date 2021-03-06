/*\
 * fgxmlset.cxx
 *
 * Copyright (c) 2014 - Geoff R. McLane
 * Licence: GNU GPL version 2
 *
\*/

#include <stdio.h>
#include <stdlib.h>
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
// #include <libxml/libxml.h>
#else
#include <string.h> // for strcmp(), ...
#include <unistd.h> // getcwd(), ...
#define stricmp strcasecmp
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
void set_full_path( std::string &file);

static std::string main_file;
static vSTG loaded_files;
// NOTE: This are parallel lists
////////////////////////////////
static vSTG all_ac_files;
static vSTG all_ac_xml;
static vSTG all_xml_files;
////////////////////////////////
static std::string loaded_file;
static const char *module = "fgxmlset";
static int options = XML_PARSE_COMPACT | XML_PARSE_BIG_LINES;
static const char *root_node = "PropertyList";
static const char *ac_folder = "Aircraft";
static const char *filename = 0;
//static const char *filename = "X:\\fgdata\\Aircraft\\777\\777-200-set.xml";
//static const char *filename = "X:\\fgdata\\Aircraft\\ufo\\ufo-set.xml";
//static const char *filename = "ufo-set.xml";
//static const char *filename = "X:\\fgdata\\Aircraft\\A320-family\\A320-231-set.xml";
static char *out_file = 0;
static char *fg_root_path = 0;  // path to fgdata root
static char *suggested_root = 0;
static bool in_primary_set = true;
static bool prim_set_desc = false;

static std::string set_root_path;   // root path of main 'set' file input
static std::string ac_path;
static int scanned_count = 0;
static double bgn_secs;
static uint64_t bytes_processed = 0;
static int verbosity = DEF_VERB;
static char *last_name = 0; // last name
static char *active_name = 0; // active name
// last loaded file components
static char *last_file = 0;
static vSTG vlastPaths; // stack of last PATHS of found files

int add2Vector( vSTG &vs, std::string itm )
{
    size_t ii, max = vs.size();
    std::string s;
    for (ii = 0; ii < max; ii++) {
        s = vs[ii];
        if (s == itm)
            return 0;
    }
    vs.push_back(itm);  // add new item
    return 1;
}

bool inLastPaths( std::string &tmp, std::string value )
{
    size_t ii, max = vlastPaths.size();
    std::string s;
    for (ii = 0; ii < max; ii++) {
        s = vlastPaths[ii];
        s += PATH_SEP;
        s += value;
        ensure_native_sep(s);
        fix_relative_path(s);
        if (is_file_or_directory(s.c_str()) == 1) {
            tmp = s;
            return true;
        }
    }
    return false;
}

void set_last_components( std::string &file )
{
    std::string name = get_file_only(file);
    std::string path = get_path_only(file);
    if (name.size()) {
        if (last_file) {
            if (strcmp(last_file,name.c_str())) {
                free(last_file);
                last_file = strdup(name.c_str());
            }
        } else {
            last_file = strdup(name.c_str());
        }
    }
    if (path.size()) {
        add2Vector(vlastPaths,path);
        // get a suggested root
        if (suggested_root == 0) {
            size_t pos = path.find("Aircraft");
            if (pos > 0) {
                std::string tmp = path.substr(0,pos-1);
                suggested_root = strdup(tmp.c_str());
            }
        }
    }
}

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
#define flg_aero        0x00000800
#define flg_desc        0x00001000
#define flg_tags        0x00002000
#define flg_tag         0x00004000
#define flg_navdb       0x00008000
#define flg_minrwy      0x00010000
// <limits>
//  <mass-and-balance>
//   <maximum-ramp-mass-lbs>2407</maximum-ramp-mass-lbs>
//   <maximum-takeoff-mass-lbs>2400</maximum-takeoff-mass-lbs>
//   <maximum-landing-mass-lbs>2400</maximum-landing-mass-lbs>
//  </mass-and-balance>
// </limits>
#define flg_limits      0x00020000
#define flg_mass        0x00040000
#define flg_maxramp     0x00080000
#define flg_maxtoff     0x00100000
#define flg_maxland     0x00200000

typedef struct tagFLGITEMS {
    std::string authors;
    std::string status;
    std::string rFDMr;
    std::string rsystems;
    std::string rcockpit;
    std::string rmodel;
    std::string avers;
    std::string fmodel;
    std::string desc;
    std::string aero;
    std::string tags;
    std::string minrwy;
    std::string maxramp;
    std::string maxtoff;
    std::string maxland;
    vSTG acfiles;
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
    { flg_aero, "aero" },   // 0x00000400
    { flg_desc, "description" }, // 0x00001000
    { flg_tags, "tags" },
    { flg_tag,  "tag" },
    { flg_navdb, "navdb" },
    { flg_minrwy, "min-runway-length-ft" },
    { flg_limits, "limits" },
    { flg_mass, "mass-and-balance" },
    { flg_maxramp, "maximum-ramp-mass-lbs" },
    { flg_maxtoff, "maximum-takeoff-mass-lbs" },
    { flg_maxland, "maximum-landing-mass-lbs" },
    //////////////////////////////////////
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
static int simaero  = (flg_sim | flg_aero);
static int simdesc  = (flg_sim | flg_desc);
static int simtags  = (flg_sim | flg_tags | flg_tag);
static int simminrwy = (flg_sim | flg_navdb | flg_minrwy);

#ifndef STRICT_MODEL_PATH
static int simmodpath = flg_path;
#else   // STRICT_MODEL_PATH
//static int simmodpath = (flg_sim | flg_path);
static int simmodpath = (flg_sim | flg_rmodel | flg_path);
#endif // STRICT_MODEL_PATH y/n

static int limmassramp = (flg_limits | flg_mass | flg_maxramp);
static int limmasstoff = (flg_limits | flg_mass | flg_maxtoff);
static int limmassland = (flg_limits | flg_mass | flg_maxland);



std::string get_flag_name(int flag)
{
    std::string s;
    PFLG2TXT f2t = flg2txt;
    while (f2t->text) {
        if (flag & f2t->flag) {
            if (s.size())
                s += "/";
            s += f2t->text;
        }
        f2t++;
    }
    if (flag == 0)
        s = "none";
    else if (s.size() == 0)
        s = "unknown";
    return s;
}

void add_parsing_flag( char *element )
{
    PFLG2TXT f2t = flg2txt;
    int flag;
    while (f2t->text) {
        if (strcmp(f2t->text,element) == 0) {
            flag = f2t->flag;
            parsing_flag |= flag;
            if (VERB9) {
                std::string s1 = get_flag_name(flag); 
                std::string s2 = get_flag_name(parsing_flag); 
                SPRTF("DBG: Added flag '%s' %#X %s, now %s\n", element, flag, s1.c_str(), s2.c_str());
            }
            flag = parsing_flag;
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
                if (VERB9) {
                    std::string s1 = get_flag_name(flag); 
                    std::string s2 = get_flag_name(parsing_flag); 
                    SPRTF("DBG: Removed flag '%s' %#X %s, now %s\n", element, flag, s1.c_str(), s2.c_str());
                }
                flag = parsing_flag;
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
bool has_excluded_folders(std::string &path)
{
    return !has_no_excluded_folders(path);
}

PFLGITEMS pflgitems = 0;
#define MEOL std::endl

std::string choose_best_model_file(vSTG &ac)
{
    std::string p,s,f,name,path;
    size_t ii, max = ac.size();
    size_t i2, max2;
    std::string acr = "Aircraft";
    for (ii = 0; ii < max; ii++) {
        f = ac[ii];
        name = get_file_only(f);
        path = get_path_only(f);
        if (name.size() && path.size()) {
            vSTG vsp = PathSplit(path);
            vSTG vsf = FileSplit(name);
            s = vsf[0];
            max2 = vsp.size();
            for (i2 = 0; i2 < max2; i2++) {
                p = vsp[i2];
                if (p == acr) {
                    i2++;   // get path after 'Aircraft'
                    if (i2 < max2) {
                        p = vsp[i2];
                        if (p == s) {
                            return f;
                        }
                    }
                    break;
                }
            }
        }
    }
    for (ii = 0; ii < max; ii++) {
        f = ac[ii];
        name = get_file_only(f);
        path = get_path_only(f);
        if (name.size() && path.size()) {
            vSTG vsp = PathSplit(path);
            vSTG vsf = FileSplit(name);
            s = vsf[0];
            max2 = vsp.size();
            for (i2 = 0; i2 < max2; i2++) {
                p = vsp[i2];
                if (p == acr) {
                    i2++;   // get path after 'Aircraft'
                    if (i2 < max2) {
                        p = vsp[i2];
                        if (stricmp(p.c_str(),s.c_str()) == 0) {
                            return f;
                        }
                    }
                    break;
                }
            }
        }
    }
    if (pflgitems && pflgitems->aero.size()) {
        // does the ac file match the 'aero' name
        // like aero = 777-200
        // model-file1 = X:\fgdata\Aircraft\777\Models\777-200.ac
        for (ii = 0; ii < max; ii++) {
            f = ac[ii];
            name = get_file_only(f);
            if (name.size()) {
                vSTG vsf = FileSplit(name);
                s = vsf[0];
                if (s == pflgitems->aero) {
                    return f;
                }
            }
        }
    }
    // TODO: could try even harder...
    s = "";
    return s;
}

void show_items_found()
{
    if (!pflgitems)
        return;
    size_t ii, max = pflgitems->acfiles.size();
    std::stringstream ss;
    std::string s;

    if (VERB5) {
        max = all_ac_files.size();
        SPRTF("\n");
        SPRTF("%s: ac files seen %d...\n", module, (int)max );
        if (max == all_ac_xml.size()) {
            for (ii = 0; ii < max; ii++) {
                s = all_ac_xml[ii];
                s = get_path_only(s);
                s += PATH_SEP;
                s += all_ac_files[ii];
                if (is_file_or_directory(s.c_str()) == 1) {
                    // found the file, so
                    SPRTF("%s\n", s.c_str());
                } else {
                    s = all_ac_files[ii];
                    SPRTF("%s in ", s.c_str());
                    SPRTF("%s\n", all_ac_xml[ii].c_str()); 
                }
            }
        } else {
            // Drat can only show this file
            for (ii = 0; ii < max; ii++) {
                s = all_ac_files[ii];
                SPRTF("%3d: %s\n", (int)(ii + 1), s.c_str());
            }
        }
    }
    if (VERB1) {
        max = loaded_files.size();
        SPRTF("\n");
        SPRTF("%s: Loaded %d xml files...\n", module, (int)max);
        if (VERB2) {
            for (ii = 0; ii < max; ii++) {
                s = loaded_files[ii];
                SPRTF("%s\n", s.c_str());
            }
        }
    }

    int itemcnt = 0;
    max = pflgitems->acfiles.size();
    ss << "# " << module << ": Processed the main file '" << main_file << MEOL;
    if (VERB5) {
        ss << "# Items found in scan of " << scanned_count << " xml file(s), " << nice_num(GetNxtBuf(),uint64_to_stg(bytes_processed));
        ss << " bytes, in " << get_seconds_stg(get_seconds() - bgn_secs) << MEOL;
    }
    ss << "[model]" << MEOL;

    if (pflgitems->aero.size()) {
        ss << "aero            = " << pflgitems->aero << MEOL;
        itemcnt++;
    }
    if (pflgitems->desc.size()) {
        ss << "description     = " << pflgitems->desc << MEOL;
        itemcnt++;
    }
    if (pflgitems->authors.size()) {
        ss << "authors         = " << pflgitems->authors << MEOL;
        itemcnt++;
    }
    if (pflgitems->status.size()) {
        ss << "status          = " << pflgitems->status << MEOL;
        itemcnt++;
    }
    if (pflgitems->rFDMr.size()) {
        ss << "rating_FDM      = " << pflgitems->rFDMr << MEOL;
        itemcnt++;
    }
    if (pflgitems->rsystems.size()) {
        ss << "rating_systems  = " << pflgitems->rsystems << MEOL;
        itemcnt++;
    }
    if (pflgitems->rcockpit.size()) {
        ss << "rating_cockpit  = " << pflgitems->rcockpit << MEOL;
        itemcnt++;
    }
    if (pflgitems->rmodel.size()) {
        ss << "rating_model    = " << pflgitems->rmodel << MEOL;
        itemcnt++;
    }
    if (pflgitems->avers.size()) {
        ss << "aircraft-version= " << pflgitems->avers << MEOL;
        itemcnt++;
    }
    if (pflgitems->fmodel.size()) {
        ss << "flight-model    = " << pflgitems->fmodel << MEOL;
        itemcnt++;
    }
    if (pflgitems->tags.size()) {
        ss << "tags            = " << pflgitems->tags << MEOL;
        itemcnt++;
    }
    if (pflgitems->minrwy.size()) {
        ss << "min-runway-ft   = " << pflgitems->minrwy << MEOL;
        itemcnt++;
    }
    if (pflgitems->maxramp.size()) {
        ss << "max-ramp-lbs    = " << pflgitems->maxramp << MEOL;
        itemcnt++;
    }
    if (pflgitems->maxtoff.size()) {
        ss << "max-takeoff-lbs = " << pflgitems->maxtoff << MEOL;
        itemcnt++;
    }
    if (pflgitems->maxland.size()) {
        ss << "max-landing-lbs = " << pflgitems->maxland << MEOL;
        itemcnt++;
    }

    if (max) {
        itemcnt++;
        if (max == 1) {
            ss << "model-file      = " << pflgitems->acfiles[0] << MEOL;
        } else {
            s = choose_best_model_file(pflgitems->acfiles);
            if (s.size()) {
                ss << "model-file      = " << s << MEOL;
            }
            if ((s.size() == 0) || VERB2) {
                ss << "# Got " << max << " 'model' files..." << MEOL;
                for (ii = 0; ii < max; ii++) {
                    s = pflgitems->acfiles[ii];
                    ss << "model-file" << (ii + 1) << " = " << s << MEOL;
                }
            }
        }
    }
    if (itemcnt) {
        if (VERB1) {
            if (VERB2)
                SPRTF("\n");
            direct_out_it((char *)ss.str().c_str());
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
    } else {
        SPRTF("%s: No known item found in '%s'\n", filename ); // usr_input
    }

    // clean up...
    pflgitems->acfiles.clear();
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


bool Already_Pushed(std::string &file)
{
    size_t ii, max = all_ac_files.size();
    std::string s;
    for (ii = 0; ii < max; ii++) {
        s = all_ac_files[ii];
        if (file == s)
            return true;
    }
    return false;
}

bool Already_Saved_Xml(std::string &file)
{
    size_t ii, max = all_xml_files.size();
    std::string s;
    for (ii = 0; ii < max; ii++) {
        s = all_xml_files[ii];
        if (file == s)
            return true;
    }
    return false;
}

bool conditional_addition(vSTG &vs, std::string &ifile)
{
    std::string f;
    size_t ii, max = vs.size();
    for (ii = 0; ii < max; ii++) {
        f = vs[ii];
        if (f == ifile)
            return false;
    }
    vs.push_back(ifile);
    return true;
}

// #define ADD_DEBUG_STOP
#ifdef ADD_DEBUG_STOP
size_t debug_stop(std::string &val)
{
    size_t i;
    i = val.size();
    return i;
}

void debug_test_stg(std::string &value)
{
    //const char *fnd = "A380.xml";
    const char *fnd = "bo105.ac";
    size_t pos = value.find(fnd);
    if (pos != std::string::npos) {
        debug_stop(value);
    }
}
#endif // ADD_DEBUG_STOP

int save_text_per_flag( char *in_value, std::string &mfile, const char *file )
{
    int iret = 0;
    if (!in_value)
        return 0;
    std::string value = in_value;
    trim_in_place(value);
    if (value.size() == 0)
        return 0;
#ifdef ADD_DEBUG_STOP
    debug_test_stg(value);
#endif
    if (parsing_flag & flg_sim) {
        if (VERB5) {
            if (active_name && (strcmp(active_name,"path") == 0)) {
                std::string s = value;
                if (find_extension(s,".ac")) {
                    if (!Already_Pushed(s)) {
                        // parallel lists
                        all_ac_files.push_back(s);
                        all_ac_xml.push_back(loaded_file);
                    }
                } else if (find_extension(s,".xml")) {
                    if (!Already_Saved_Xml(s)) {
                        all_xml_files.push_back(s);
                    }
                }
            }
        }
        if (pflgitems == 0)
            pflgitems = new FLGITEMS;
        if (GOT_FLG(simauthor)) {
            // save the author
            pflgitems->authors = agressive_trim(value);
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
        if (GOT_FLG(simaero)) {
            pflgitems->aero = value;
        }
        if (GOT_FLG(simdesc)) {
            if (pflgitems->desc.size() == 0) {
                // only take in the 'first' decriptions
                if (in_primary_set)
                    prim_set_desc = true;
                pflgitems->desc = agressive_trim(value);
            } else if (in_primary_set) {
                // if in the primary set file use thsi description if not already set
                if (!prim_set_desc) {
                    pflgitems->desc = agressive_trim(value);
                    prim_set_desc = true;
                }
            }
        }
        // added 20150111
        if (GOT_FLG(simtags)) {
            if (pflgitems->tags.size())
                pflgitems->tags += ";";
            pflgitems->tags += value;
        }
        if (GOT_FLG(simminrwy)) {
            pflgitems->minrwy = value;
        }
        // =============================
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
                    //pflgitems->acpaths.push_back(ifile);
                    conditional_addition(pflgitems->acfiles,ifile);
                }
            } else if (find_extension(ifile,".xml") || find_extension(ifile,".ac")) {
                // can NOT find a file we are interested in, give a warning
                if (has_file_path((char *)value.c_str())) { // if it has a path
                    // like 'X:\fgdata\..\Models\a320.fuselage.ac'
                    // 20150621: MUST ALSO USE THE PATH RELATIVE TO THE SET FILE BEING PROCESSES
                    std::string tmp, tmp2, tmp3, tmp4;
                    if (find_extension(ifile,".ac")) {
                        // is an AC file
                        if (inLastPaths( tmp, value )) {
                            // store this as the .ac file
                            conditional_addition(pflgitems->acfiles,tmp);
                        } else {
                            if (fg_root_path) {
                                tmp2 = fg_root_path;
                                tmp2 += PATH_SEP;
                                tmp2 += value;
                                ensure_native_sep(tmp2);
                                fix_relative_path(tmp2);
                                if (is_file_or_directory(tmp2.c_str()) == 1) {
                                    conditional_addition(pflgitems->acfiles,tmp2);
                                    return 0;
                                }
                            }
                            if (suggested_root) {
                                tmp3 = suggested_root;
                                tmp3 += PATH_SEP;
                                tmp3 += value;
                                ensure_native_sep(tmp3);
                                fix_relative_path(tmp3);
                                if (is_file_or_directory(tmp3.c_str()) == 1) {
                                    conditional_addition(pflgitems->acfiles,tmp3);
                                    return 0;
                                }
                            }

                            SPRTF("WARNING: Unable to find the model file '%s'\n", value.c_str());
                            SPRTF("As '%s'\n", ifile.c_str());
                            if (VERB2) {
                                if (tmp.size())
                                    SPRTF("Nor '%s'\n", tmp.c_str());
                                if (tmp2.size())
                                    SPRTF("Nor '%s'\n", tmp2.c_str());
                                if (tmp3.size() && (ifile != tmp3))
                                    SPRTF("Nor '%s'\n", tmp3.c_str());
                            }
                        }
                    } else {
                        // is an XML file
                        // maybe relative to set file path
                        if (set_root_path.size()) {
                            tmp4 = set_root_path;
                            tmp4 += PATH_SEP;
                            tmp4 += value;
                            ensure_native_sep(tmp4);
                            if (is_file_or_directory(tmp.c_str()) == 1) {
                                mfile = tmp;  // can be processed
                                iret = 1;
                                return iret;
                            }
                        }

                        if (inLastPaths(tmp,value)) {
                             mfile = tmp;  // can be processed
                             iret = 1;
                             return iret;
                        }
                        if (fg_root_path) {
                            tmp2 = fg_root_path;
                            tmp2 += PATH_SEP;
                            tmp2 += value;
                            ensure_native_sep(tmp2);
                            fix_relative_path(tmp2);
                            if (is_file_or_directory(tmp2.c_str()) == 1) {
                                mfile = tmp2;  // can be processed
                                iret = 1;
                                return iret;
                            }
                        }
                        if (suggested_root) {
                            tmp3 = suggested_root;
                            tmp3 += PATH_SEP;
                            tmp3 += value;
                            ensure_native_sep(tmp3);
                            fix_relative_path(tmp3);
                            if (is_file_or_directory(tmp3.c_str()) == 1) {
                                mfile = tmp3;  // can be processed
                                iret = 1;
                                return iret;
                            }
                        }
                        size_t pos = value.find("Aircraft");
                        if (pos == 0) {
                            if (has_excluded_folders(value)) {
                                iret = 0;   // we can forget this include
                                mfile = ifile;  // mark as a processed file
                                return iret;
                            }
                        }
                        SPRTF("WARNING: Given path '%s'\nunable to find file '%s'\n", value.c_str(), ifile.c_str());
                        if (!fg_root_path)
                            SPRTF("Advice: No FG_ROOT (-r path) given. It may be in there...\n");
                        if (VERB2) {
                            if (tmp.size())
                                SPRTF("Nor '%s'\n", tmp.c_str());
                            if (tmp2.size())
                                SPRTF("Nor '%s'\n", tmp2.c_str());
                            if (tmp3.size() && (ifile != tmp3))
                                SPRTF("Nor '%s'\n", tmp3.c_str());
                        }

                    }
                //} else if (find_extension(ifile,".ac") && (pflgitems->acfiles.size() == 0)) {
                } else if (find_extension(ifile,".ac")) {
                    // try REAL HARD to find this file... as the possible model .ac
                    // maybe it is relative to the current file
                    ifile = file;
                    ifile = get_path_only(ifile);
                    ifile += PATH_SEP;
                    ifile += value;
                    if (is_file_or_directory(ifile.c_str()) == 1) {
                        // store this as the .ac file
                        conditional_addition(pflgitems->acfiles,ifile);
                    } else {
                        SPRTF("WARNING: Unable to find ac file '%s' absolute or relative!\n", value.c_str() );
                    }
                }
            }
        }
    }

#if 0 // 0000000000000000000000000000000000000000000000000000
    // this is CRASHING!!! WHy????????????????
    if (GOT_FLG(limmassramp)) {
        pflgitems->maxramp = value;
    }
    if (GOT_FLG(limmasstoff)) {
        pflgitems->maxtoff = value;
    }
    if (GOT_FLG(limmassland)) {
        pflgitems->maxland = value;
    }
#endif // 000000000000000000000000000000000000000000000000000

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
    int save_parse;

    depth = xmlTextReaderDepth(reader);
    nt = xmlTextReaderNodeType(reader);
    type = getNodeTypeStg(nt);
    name = xmlTextReaderConstName(reader);
    if (name == NULL)
	name = BAD_CAST "--";
    last_name = (char *)name;
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
            active_name = (char *)name;
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
                            ifile = set_root_path;
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
                            loaded_file = s;
                            set_last_components(s);
                            //path_stack.push_back(path);
                            xmlpath.clear();
                            save_parse = parsing_flag;
#ifdef CLEAR_PARSE_FLAG
                            parsing_flag = 0;   // restart parsing flag for new document
#endif // CLEAR_PARSE_FLAG
                            in_primary_set = false;
                            walkDoc(idoc, lev + 1, s.c_str());
                            xmlFreeDoc(idoc);       // free document
                            xmlpath.clear();
                            xmlpath = PathSplit(path);
                            parsing_flag = save_parse;
                            in_primary_set = true;
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
                        loaded_file = ifile;
                        set_last_components(ifile);
                        bytes_processed += get_last_file_size();
                        xmlpath.clear();
                        save_parse = parsing_flag;
#ifdef CLEAR_PARSE_FLAG
                        parsing_flag = 0;   // restart parsing flag for new document
#endif // CLEAR_PARSE_FLAG
                        in_primary_set = false;
                        walkDoc(idoc, lev + 1, ifile.c_str());
                        xmlFreeDoc(idoc);       // free document
                        xmlpath.clear();
                        xmlpath = PathSplit(path);
                        parsing_flag = save_parse;
                        in_primary_set = true;
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

void set_full_path( std::string &file)
{
    char *tb = GetNxtBuf();
#ifdef WIN32
    if (_fullpath(tb,file.c_str(),MX_ONE_BUF) != NULL) {
        if (VERB5) {
            if (strcmp(tb,file.c_str())) {
                SPRTF("%s: Relative path '%s' modified to full path '%s'\n", module, file.c_str(), tb);
            }
        }
        file = tb;
    }
#else
    if (realpath(file.c_str(),tb) != NULL) {
        if (VERB5) {
            if (strcmp(tb,file.c_str())) {
                SPRTF("%s: Relative path '%s' modified to full path '%s'\n", module, file.c_str(), tb);
            }
        }
        file = tb;
    }
#endif
}

// try to always get the full path if given a relative file
void set_root_paths( std::string &file)
{
    std::string s;
    set_full_path(file);
    set_root_path = get_path_only(file);
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
    printf("\n");
    printf("%s [options] input-fg-xml-set-file\n", file.c_str());
    printf("\n");
    printf("Options:\n");
    printf(" --help  (-h or -?) = This help and exit(2)\n");
    printf(" --verb[nn]    (-v) = Bump or set verbosity. (def=%d)\n", verbosity);
    printf(" --log <file>  (-l) = Set the log file for output.  Use 'none' to disable.\n (def=%s)\n", get_log_file());
    printf(" --out <file>  (-o) = Write results to out file. (def=%s)\n",
        out_file ? out_file : "none");
    printf(" --root <path> (-r) = Set the 'root' ie FG_ROOT path. Specific aircraf xml can refer to common xml.\n");
    printf("\n");
    printf("Will parse the input as a FlightGear 'xxx-set' xml file, and extract information.\n");
    printf("While there is a good attempt to handle a relative file name, it is certainly better\n");
    printf("to use a fully qualified input file name.\n");
    printf("\n");
    printf("NOTE: This may ONLY work for FlightGear Aircraft 'xxx-set.xml' files that are\n");
    printf("      in the fgdata folder since there are some very FG specific relative\n");
    printf("      paths applied to some.\n");
    printf("\n");
    printf("But in general terms it could be taken as a reasonable example of how to\n");
    printf("extract information from any xml file using the services of libXml2.\n");
    printf("\n");
}

#define ISDIGIT(a) (( a >= '0' ) && ( a <= '9' ))

int scan_for_log_file( int argc, char **argv )
{
    int c, i;
    int i2 = 0;
    char *arg, *sarg;
    std::string file;
    // set a log file per the exe path
    char *tb;
    int res;
#ifdef WIN32
    tb = GetNxtBuf();
    *tb = 0;
    res = GetModuleFileName(NULL,tb,MX_ONE_BUF);
    file = tb;
#else
    arg = argv[0];
    if (*arg == '/') {
        file = arg;
    } else {
        tb = GetNxtBuf();
        *tb = 0;
        res = readlink("/proc/self/exe",tb,MX_ONE_BUF);
        if ((res > 0)&&(*tb)) {
            tb[res] = 0;
            file = tb;
        } else {
            file = argv[0];
        }
    }
#endif
    if (i2) printf("runtime: %s\n", file.c_str());
    file = get_path_only(file);
    if (i2) printf("path   : %s\n", file.c_str());
    file += PATH_SEP;
    file += "tempfgxml.txt";
    if (i2) printf("file   : %s\n", file.c_str());
    set_full_path(file);
    if (i2) printf("full   : %s\n", file.c_str());
    set_log_file((char *)file.c_str(),false);

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

char *get_full_path( char *file )
{
    char *cp = GetNxtBuf();
    strcpy(cp,file);
#ifdef _MSC_VER
    DWORD siz = (DWORD)GetBufSiz();
    DWORD res = GetFullPathName(file,
        siz,
        cp,
        NULL );
#else
    // TODO: Get full path in UNIX
    char *res = realpath(file,cp);
    if (!res)
        strcpy(cp,file);
#endif
    return cp;
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
                return 2;
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
            case 'r':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    fg_root_path = strdup(sarg);
                    if (is_file_or_directory(fg_root_path) != 2) {
                        SPRTF("%s: Unable to 'stat' fg root path '%s'! Check name. Aborting...\n", module,
                            fg_root_path);
                        return 1;
                    }
                } else {
                    SPRTF("%s: Expected root path to follow '%s'! Aborting...\n", module, arg);
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
            filename = strdup(get_full_path(arg));
        }

    }
    if (VERB5) {
        SPRTF("%s: Log file is '%s'\n", module, get_log_file());
    }
    if (!filename) {
        give_help(argv[0]);
        SPRTF("%s: ERROR: No input file found in command! Aborting...\n", module);
        return 1;
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
    // test_stg_trim();
    int iret = parse_args(argc,argv);
    if (iret) {
        if (iret == 2)
            iret = 0;
        return iret;
    }
    xmlDocPtr doc = NULL;
    bgn_secs = get_seconds();
    // establish the MAIN file
    main_file = filename;
    ensure_native_sep(main_file);
    set_root_paths(main_file);
    if (is_file_or_directory(main_file.c_str()) != 1) {
        SPRTF("%s: Failed to 'stat' file '%s'\n", module, main_file.c_str() );
        return 1;
    }
    bytes_processed += get_last_file_size();
    in_primary_set = true;
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

    scanned_count++;
    loaded_files.push_back(main_file);
    loaded_file = main_file;
    set_last_components( main_file );
    walkDoc(doc, 0, main_file.c_str());

    xmlFreeDoc(doc);       // free document
    xmlCleanupParser();    // Free globals

    show_items_found();

    if (VERB2) {
        printf("See log file '%s' for details...\n", get_log_file());
    }

    return iret;
}


// eof = fgxmlset.cxx

