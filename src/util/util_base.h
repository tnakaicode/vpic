/* 
 * Written by:
 *   Kevin J. Bowers, Ph.D.
 *   Plasma Physics Group (X-1)
 *   Applied Physics Division
 *   Los Alamos National Lab
 * March/April 2004 - Original version
 *
 */

#ifndef _util_base_h_
#define _util_base_h_

#ifdef __cplusplus
#define BEGIN_C_DECLS extern "C" {
#define END_C_DECLS }
#else
#define BEGIN_C_DECLS
#define END_C_DECLS
#endif

#include <stdlib.h> // For malloc, realloc, free, size_t, NULL
#include <string.h> // For string and memory manipulation
#include <math.h>   // For math prototypes
#include <stdint.h> // For fixed width integer types
#include <limits.h> // For integer limits
#include <float.h>  // For floating point limits

// These macros facilitate doing evil tricks

#define BEGIN_PRIMITIVE do
#define END_PRIMITIVE   while(0)

#define _UTIL_STRINGIFY(s)#s
#define EXPAND_AND_STRINGIFY(s)_UTIL_STRINGIFY(s)

// Alignment macros

// This pointer modifier indicates that a pointer can be assumed to
// have at least the given power-of-two alignment.

// FIXME: May want to use compiler instrincs for to provide compiler
// the aligment information as well.

#ifndef ALIGNED
#define ALIGNED(a)
#endif

// Normal pointers (e.g. a *) are in whatever address space the given
// compile unit uses.  However, sometimes it is necessary to declare
// pointers that are understable in multiple address spaces.  The
// MEM_PTR macro declares a pointer to external memory with the
// specified alignment.  This allows declaractions to be compiled on
// both the SPU and PPU with appropriate annotations to necessary
// write the appropriate DMA transfers.

#ifdef __SPU__
# ifdef USE_32_BIT_POINTERS
#   define MEM_PTR(type,align) uint32_t
# else
#   define MEM_PTR(type,align) uint64_t
# endif
#else
# define MEM_PTR(type,align) type * ALIGNED(align)
#endif

// The SIZEOF_MEM_PTR macro gives the number of bytes taken by a MEM_PTR.

#define SIZEOF_MEM_PTR sizeof(MEM_PTR(void,1))

// DECLARE_ALIGNED_ARRAY declares an array containing count elements
// with the given alignment in memory.  The scope of the array is the
// scope of context in which it is declared.  Note: This macro is
// really two statements (there is no way to bundle the macro into one
// semantic statement linguistically without defeating the purpose of
// the macro).  Thus, it is not as robust as it could be.  Thus, sure
// any usage of this macro occurs in contexts where two back-to-back
// statments in the same context would be in the same scope.  That is:
//
//   if(...) { DECLARE_ALIGNED_ARRAY(type,align,name,count); ... } // OKAY!
//   else      DECLARE_ALIGNED_ARRAY(type,align,name,count);     // NOT OKAY!
//
// For 99.9% of the expected usage, this should not matter.
//
// align should be a power of two.

#define DECLARE_ALIGNED_ARRAY(type,align,name,count)                        \
  char _aa_##name[(count)*sizeof(type)+(align)];                            \
  type * ALIGNED(align) const name = (type * ALIGNED(align))                \
    ( ( (size_t)_aa_##name + (align) - 1 ) & (~((align)-1)) )

// PAD(s,a) computes the amount of bytes necessary to add to "s" bytes
// to make "s" evenly divisible by "a" (a power of two).  Note: PAD is
// more wasteful than necessary if no padding needed.  Ideally PAD
// should be:
//   ( ( (a) - ( (s) & ( (a)-1 ) ) ) & ( (a)-1 ) )
// Unfortunately, C++98 does not support zero sized structure members
// and the preprocessor cannot evaluate the typical inputs to size to
// allow correct autogeneration when no alignment necessary ... sigh
// ...

#define PAD(s,a) ( (a) - ( (s) & ( (a)-1 ) ) ) 

// Workload distribution macros

// Let items be enumerated 0,1, ... "N"-1 and pipelines be
// enumerated 0,1, ... "P" (0:P-1 refer to pipeline processes
// and pipeline P refers to the dispatching process).
// DISTRIBUTE determines the first item "i" and the number
// of items "n" that should be assigned to pipeline "p".
// The items are assigned such that each pipeline process
// gets an approximate equal share of items and that the
// number of items each pipeline process gets is a
// multiple of the block size "b".  The dispatching process
// is assigned all remaining items.  The items are assigned
// in monotonically increasing order.
//
// This macro is robust.  (All arguments only evaluated once;
// inputs can be same as output.)  Any compiler worth its
// salt will replace the divison and modulo with bit shifts
// and masks for power-of-two block sizes.

#define DISTRIBUTE( N, b, p, P, i, n ) BEGIN_PRIMITIVE {             \
    int _N = (N), _b = (b), _p = (p), _P = (P);                      \
    double _t = (double)(_N/_b)/(double)_P;                          \
    int _i =                    _b*(int)(_t*(double) _p   +0.5);     \
    (n) = (_p==_P) ? (_N%_b) : (_b*(int)(_t*(double)(_p+1)+0.5)-_i); \
    (i) = _i;                                                        \
  } END_PRIMITIVE

// INDEX_FORTRAN_x and INDEX_C_x give macros for accessing
// multi-dimensional arrays with different conventions. To eliminate
// potential side effects and maximize optimization possibilites, xl,
// xh, yl, yh, zl, zh should be local constant ints

#define INDEX_FORTRAN_1(x,xl,xh)                 \
 ((x)-(xl))
#define INDEX_FORTRAN_2(x,y,xl,xh,yl,yh)         \
 ((x)-(xl) + ((xh)-(xl)+1)*((y)-(yl)))
#define INDEX_FORTRAN_3(x,y,z,xl,xh,yl,yh,zl,zh) \
 ((x)-(xl) + ((xh)-(xl)+1)*((y)-(yl) + ((yh)-(yl)+1)*((z)-(zl))))

#define INDEX_C_1(x,xl,xh)                 \
 ((x)-(xl))
#define INDEX_C_2(x,y,xl,xh,yl,yh)         \
 ((y)-(yl) + ((yh)-(yl)+1)*((x)-(xl)))
#define INDEX_C_3(x,y,z,xl,xh,yl,yh,zl,zh) \
 ((z)-(zl) + ((zh)-(zl)+1)*((y)-(yl) + ((yh)-(yl)+1)*((x)-(xl))))

// The following macros deal with linked lists

#define LIST_FOR_EACH(node,list)        \
  for((node)=(list); (node)!=NULL; (node)=(node)->next)
#define LIST_FIND_FIRST(node,list,cond) \
  for((node)=(list); (node)!=NULL; (node)=(node)->next) if(cond) break

// The following macros give a provide a simple logging capabilty. Due
// to the way they work, usage needs double parenthesis. That is:
//
//   ERROR(("Could not allocate %i bytes", req));
//
// will print the following message to the log:
//
//   Error at src/module/file.c(34):
//           Could not allocate 45 bytes
//
// Note: Error messages are abortive but MESSAGE and WARNING are not

// FIXME: SHOULD PRINT THE WORLD RANK!

#define CHECKPOINT() BEGIN_PRIMITIVE {		           \
  print_log( "%s(%i): Checkpoint\n", __FILE__, __LINE__ ); \
} END_PRIMITIVE

#define MESSAGE(args) BEGIN_PRIMITIVE {		\
  print_log( "%s(%i): ", __FILE__, __LINE__ );	\
  print_log args;			        \
  print_log( "\n" );                            \
} END_PRIMITIVE

#define WARNING(args) BEGIN_PRIMITIVE {			     \
  print_log( "Warning at %s(%i):\n\t", __FILE__, __LINE__ ); \
  print_log args;                                            \
  print_log( "\n" );                                         \
} END_PRIMITIVE

#define ERROR(args) BEGIN_PRIMITIVE {			   \
  print_log( "Error at %s(%i):\n\t", __FILE__, __LINE__ ); \
  print_log args;					   \
  print_log( "\n" );                                       \
  /* FIXME: SHOULD WAIT A FEW SECONDS HERE! */             \
  exit(1);                                                 \
} END_PRIMITIVE

// FIXME: DEPRECATE THIS
typedef const char *error_code;
#define ERROR_CODE(s) \
  ((error_code)(__FILE__"("EXPAND_AND_STRINGIFY(__LINE__)"): "s))
#define NO_ERROR ((error_code)NULL)

// Opaque data type so users do not have to see message passing
// internals moved from mp.h.

typedef void * mp_handle; // FIXME: This really doesn't belong here.

BEGIN_C_DECLS

// In util.c

void *
malloc_aligned( size_t n, size_t a );

void
free_aligned( void * mem );

void
print_log( const char *fmt, ... );

// This function returns a value to prevent the compiler from
// optimizing it away the function body.  The caller should not use it
// though so the declaration casts away the return.

#define nanodelay(i) ((void)_nanodelay(i))
uint32_t
_nanodelay( uint32_t i );

END_C_DECLS

#endif // _util_base_h_
