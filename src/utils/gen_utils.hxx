/*\
 * gen_utils.hxx
 *
 * Copyright (c) 2014 - Geoff R. McLane
 * Licence: GNU GPL version 2
 *
\*/

#ifndef _GEN_UTILS_HXX_
#define _GEN_UTILS_HXX_
#include <stdint.h>
#ifdef __cplusplus
#include <vector>
#include <string>
#include <map>
#endif // #ifdef __cplusplus

#ifdef __cplusplus
extern "C" {
#endif

extern const char *getNodeTypeStg( int nt );
#ifdef WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

extern int is_file_or_directory ( const char * path ); // 1 = file, 2 = dir, 0 = neither
extern size_t get_last_file_size();
extern char *get_seconds_stg( double dsecs );
extern double get_seconds();
extern char *uint64_to_stg( uint64_t val );
extern char *nice_num( char * dst, char * src );


#ifdef __cplusplus
typedef std::vector<std::string> vSTG;
extern void ensure_win_sep( std::string &path );
extern void ensure_unix_sep( std::string &path );
extern void ensure_native_sep( std::string &path );
extern int find_extension(std::string &file, const char *ext);
#endif // #ifdef __cplusplus

#ifdef __cplusplus
}

extern vSTG PathSplit( std::string &path );
extern std::string get_file_only( std::string &path );
extern std::string get_path_only( std::string &file );

#endif

#endif // #ifndef _GEN_UTILS_HXX_
// eof - gen_utils.hxx
