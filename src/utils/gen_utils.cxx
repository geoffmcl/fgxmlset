/*\
 * gen_utils.cxx
 *
 * Copyright (c) 2014 - Geoff R. McLane
 * Licence: GNU GPL version 2
 *
\*/

#ifndef USE_PERF_COUNTER
#define USE_PERF_COUNTER
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string>
#include <stdint.h>
#include <libxml/xmlreader.h>
#ifndef _MSC_VER
#include <string.h> // for strlen(), ...
#include <sys/time.h> // got gettimeofday(), ..
#endif
#if (defined(WIN32) && defined(USE_PERF_COUNTER))
#include <Windows.h>
#endif
#include "sprtf.hxx"
#include "gen_utils.hxx"

static const char *module = "gen_utils";

/**
 * xmlReaderTypes:
 *
 * Predefined constants for the different types of nodes.
 */
typedef struct tagTYP2STG {
    xmlReaderTypes type;
    const char *stg;
}TYP2STG, *PTYP2STG;

static TYP2STG type2stg[] = {
    { XML_READER_TYPE_NONE, "NONE" }, // = 0,
    { XML_READER_TYPE_ELEMENT, "ELEMENT" }, // = 1,
    { XML_READER_TYPE_ATTRIBUTE, "ATTRIBUTE" }, // = 2,
    { XML_READER_TYPE_TEXT, "TEXT" },   // = 3,
    { XML_READER_TYPE_CDATA, "CDATA" }, // = 4,
    { XML_READER_TYPE_ENTITY_REFERENCE, "ENTITY_REF" }, // = 5,
    { XML_READER_TYPE_ENTITY, "ENTITY" },   // = 6,
    { XML_READER_TYPE_PROCESSING_INSTRUCTION, "PROCINST" }, // = 7,
    { XML_READER_TYPE_COMMENT, "COMMENT" }, // = 8,
    { XML_READER_TYPE_DOCUMENT, "DOC" },    // = 9,
    { XML_READER_TYPE_DOCUMENT_TYPE, "DOCTYPE" }, // = 10,
    { XML_READER_TYPE_DOCUMENT_FRAGMENT, "DOC_FRAG" },  // = 11,
    { XML_READER_TYPE_NOTATION, "NOTAT" },  // = 12,
    { XML_READER_TYPE_WHITESPACE, "SPACE" },    // = 13,
    { XML_READER_TYPE_SIGNIFICANT_WHITESPACE, "SIGSPACE" }, // = 14,
    { XML_READER_TYPE_END_ELEMENT, "END_ELE" }, // = 15,
    { XML_READER_TYPE_END_ENTITY, "END_ENT" },  // = 16,
    { XML_READER_TYPE_XML_DECLARATION, "DECL" },    // = 17

    // MUST BE LAST
    { (xmlReaderTypes)-1, 0 }
};

const char *getNodeTypeStg( int nt )
{
    PTYP2STG t2s = type2stg;
    while (t2s->stg) {
        if (t2s->type == nt )
            return t2s->stg;
        t2s++;
    }
    return "NOT FOUND";
}

void ensure_win_sep( std::string &path )
{
    std::string::size_type pos;
    std::string s = "/";
    std::string n = "\\";
    while( (pos = path.find(s)) != std::string::npos ) {
        path.replace( pos, 1, n );
    }
}

void ensure_unix_sep( std::string &path )
{
    std::string::size_type pos;
    std::string n = "/";
    std::string s = "\\";
    while( (pos = path.find(s)) != std::string::npos ) {
        path.replace( pos, 1, n );
    }
}

void ensure_native_sep( std::string &path )
{
#ifdef WIN32
    ensure_win_sep(path);
#else
    ensure_unix_sep(path);
#endif
}

vSTG PathSplit( std::string &path ) 
{
    std::string tmp = path;
    std::string s = PATH_SEP;
    vSTG result;
    int pos;
    bool done = false;

    result.clear();
    ensure_native_sep(tmp);
    while ( !done ) {
        pos = (int)tmp.find(s);
        if (pos >= 0) {
            result.push_back( tmp.substr(0, pos) );
            tmp = tmp.substr( pos + 1 );
        } else {
            if ( !tmp.empty() )
                result.push_back( tmp );
            done = true;
        }
    }
    return result;
}

vSTG FileSplit( std::string &file ) 
{
    size_t pos = file.rfind(".");
    vSTG vs;
    if (pos > 0) {
        std::string s = file.substr(0,pos);
        vs.push_back(s);
        s = file.substr(pos);
        vs.push_back(s);
    } else {
        vs.push_back(file);
    }
    return vs;
}

void fix_relative_path( std::string &path )
{
    vSTG vs = PathSplit(path);
    size_t ii, max = vs.size();
    std::string npath, tmp;
    vSTG n;
    for (ii = 0; ii < max; ii++) {
        tmp = vs[ii];
        if (tmp == ".")
            continue;
        if (tmp == "..") {
            if (n.size()) {
                n.pop_back();
                continue;
            }
            return;
        }
        n.push_back(tmp);
    }
    ii = n.size();
    if (ii && (ii != max)) {
        max = ii;
        for (ii = 0; ii < max; ii++) {
            tmp = n[ii];
            if (npath.size())
                npath += PATH_SEP;
            npath += tmp;
        }
        path = npath;
    }
}

////////////////////////////////////////////////////////////////
// unix fix 
// when given like /media/path/file.xml
// the PathSplit correctly add a 'blank' to the vector, so added a 
// small fix to make sure a path separator gets add at the beginning
//
std::string get_path_only( std::string &file )
{
    std::string path;
    vSTG vs = PathSplit(file);
    size_t ii, max = vs.size();
    if (max)
        max--;
    for (ii = 0; ii < max; ii++) {
        if (path.size() || ii)
            path += PATH_SEP;
        path += vs[ii];
    }
    return path;
}

std::string get_file_only( std::string &path )
{
    std::string file;
    vSTG vs = PathSplit(path);
    size_t max = vs.size();
    if (max) {
        file = vs[max - 1];
    }
    return file;
}

#ifdef _MSC_VER
#define M_IS_DIR _S_IFDIR
#else // !_MSC_VER
#define M_IS_DIR S_IFDIR
#endif

static	struct stat _s_buf;
int is_file_or_directory ( const char * path ) // 1 = file, 2 = dir, 0 = neither
{
    if (!path)
        return 0;
	if (stat(path,&_s_buf) == 0)
	{
		if (_s_buf.st_mode & M_IS_DIR)
			return 2;
		else
			return 1;
	}
	return 0;
}
size_t get_last_file_size() { return _s_buf.st_size; }

int find_extension(std::string &file, const char *ext)
{
    if (!ext)
        return 0;
    size_t len = strlen(ext);
    if (!len)
        return 0;
    size_t pos = file.find(ext);
    if (pos == file.size() - len)
        return 1;
    return 0;
}

#if (defined(WIN32) && defined(USE_PERF_COUNTER))
// QueryPerformanceFrequency( &frequency ) ;
// QueryPerformanceCounter(&timer->start) ;
double get_seconds()
{
    static double dfreq;
    static bool done_freq = false;
    static bool got_perf_cnt = false;
    if (!done_freq) {
        LARGE_INTEGER frequency;
        if (QueryPerformanceFrequency( &frequency )) {
            got_perf_cnt = true;
            dfreq = (double)frequency.QuadPart;
        }
        done_freq = true;
    }
    double d;
    if (got_perf_cnt) {
        LARGE_INTEGER counter;
        QueryPerformanceCounter (&counter);
        d = (double)counter.QuadPart / dfreq;
    }  else {
        DWORD dwd = GetTickCount(); // milliseconds that have elapsed since the system was started
        d = (double)dwd / 1000.0;
    }
    return d;
}

#else // !WIN32
double get_seconds()
{
    struct timeval tv;
    gettimeofday(&tv,0);
    double t1 = (double)(tv.tv_sec+((double)tv.tv_usec/1000000.0));
    return t1;
}
#endif // WIN32 y/n

///////////////////////////////////////////////////////////////////////////////

// see : http://geoffair.org/misc/powers.htm
char *get_seconds_stg( double dsecs )
{
    static char _s_secs_buf[256];
    char *cp = _s_secs_buf;
    sprintf(cp,"%g",dsecs);
    if (dsecs < 0.0) {
        strcpy(cp,"?? secs");
    } else if (dsecs < 0.0000000000001) {
        strcpy(cp,"~0 secs");
    } else if (dsecs < 0.000000005) {
        // nano- n 10 -9 * 
        dsecs *= 1000000000.0;
        sprintf(cp,"%.3f nano-secs",dsecs);
    } else if (dsecs < 0.000005) {
        // micro- m 10 -6 * 
        dsecs *= 1000000.0;
        sprintf(cp,"%.3f micro-secs",dsecs);
    } else if (dsecs <  0.005) {
        // milli- m 10 -3 * 
        dsecs *= 1000.0;
        sprintf(cp,"%.3f milli-secs",dsecs);
    } else if (dsecs < 60.0) {
        sprintf(cp,"%.3f secs",dsecs);
    } else {
        int mins = (int)(dsecs / 60.0);
        dsecs -= (double)mins * 60.0;
        if (mins < 60) {
            sprintf(cp,"%d:%.3f min:secs", mins, dsecs);
        } else {
            int hrs = mins / 60;
            mins -= hrs * 60;
            sprintf(cp,"%d:%02d:%.3f hrs:min:secs", hrs, mins, dsecs);
        }
    }
    return cp;
}

//#ifdef _MSC_VER
//#define PFX64 "I64u"
//#else
//#define PFX64 PRIu64
//#endif

char *uint64_to_stg( unsigned long long val )
{
    char *nb = GetNxtBuf();
    sprintf(nb,"%llu", val);
    return nb;
}

/* ======================================================================
   nice_num = get nice number, with commas
   given a destination buffer,
   and a source buffer of ascii
   NO CHECK OF LENGTH DONE!!! assumed destination is large enough
   and assumed it is a NUMBER ONLY IN THE SOURCE
   ====================================================================== */
char *nice_num( char * dst_buf, char * src ) // get nice number, with commas
{
   size_t i;
   size_t len = strlen(src);
   size_t rem = len % 3;
   size_t cnt = 0;
   char *dst = dst_buf;
   for( i = 0; i < len; i++ )
   {
      if( rem ) {
         *dst++ = src[i];
         rem--;
         if( ( rem == 0 ) && ( (i + 1) < len ) )
            *dst++ = ',';
      } else {
         *dst++ = src[i];
         cnt++;
         if( ( cnt == 3 ) && ( (i + 1) < len ) ) {
            *dst++ = ',';
            cnt = 0;
         }
      }
   }
   *dst = 0;
   return dst_buf;
}

// just to avoid gcc warning ‘module’ defined but not used [-Wunused-variable]
void out_module_name()
{
    SPRTF("%s",module);
}

/////////////////////////////////////////////////////////////
// trim a string utilities
// modifies input string, returns input
std::string& trim_left_in_place(std::string& str) {
    size_t i = 0;
    while(i < str.size() && isspace(str[i])) { ++i; };
    return str.erase(0, i);
}

std::string& trim_right_in_place(std::string& str) {
    size_t i = str.size();
    while(i > 0 && isspace(str[i - 1])) { --i; };
    return str.erase(i, str.size());
}

std::string& trim_in_place(std::string& str) {
    return trim_left_in_place(trim_right_in_place(str));
}

std::string trim_ws(const std::string& str,
                 const std::string& whitespace = " \t")
{
    const size_t strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    const size_t strEnd = str.find_last_not_of(whitespace);
    const size_t strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

std::string trim_reduce(const std::string& str,
                   const std::string& fill = " ",
                   const std::string& whitespace = " \t\r\n")
{
    // trim first
    std::string result = trim_ws(str, whitespace);

    // replace sub ranges
    size_t beginSpace = result.find_first_of(whitespace);
    while (beginSpace != std::string::npos)
    {
        const size_t endSpace = result.find_first_not_of(whitespace, beginSpace);
        const size_t range = endSpace - beginSpace;

        result.replace(beginSpace, range, fill);

        const size_t newStart = beginSpace + fill.length();
        beginSpace = result.find_first_of(whitespace, newStart);
    }

    return result;
}

std::string agressive_trim( std::string &str )
{
    return trim_reduce(str);
}

// eof = gen_utils.cxx
