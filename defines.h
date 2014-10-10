#ifndef DEFINES_H
#define DEFINES_H

/* You can change these: */
// Verbosity level, the following are available:
// 0: Log only converted files.
// 1: Log effects and converted files, and some general info
// 2: Log effects, their fields, converted files and anything debug-related.
#define VERBOSE 0
// Output fields like Color Map IDs and Color Map color IDs as well, which are meaningless outside of AVS proper.
#define ALL_FIELDS 1
// Default input dir, useful for debugging.
#ifdef QT_DEBUG
#define DEFAULT_IN_DIR "D:\\Daten\\Dropbox"
#endif

/* You shouldn't change these: */
// The length of the header string in .avs files and Clear Frame byte.
#define AVS_HEADER_LENGTH (24+1) // one extra for Clear Frame
// 2^14. Effect codes higher than this denote an plugin/DLL/APE effect.
#define BUILTIN_MAX 16384
// sizeof int shouldn't change, this is just to avoid a magic number.
#define SIZE_INT 4

#endif // DEFINES_H
