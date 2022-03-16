#define _GNU_SOURCE
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <execinfo.h>
#include <malloc.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>



static void * (*__for_allocate) (size_t, void **, void **) = NULL;
static void * (*__for_dealloc_allocatable) (void **, void **) = NULL;
static void * (*__for_deallocate) (void **, void **) = NULL;

typedef struct
{
  void * ptr;
  size_t siz;
} ptr_t;

#define N  3000000
static ptr_t ptrlist[N];


static inline void init ()
{
  if (__for_allocate != NULL)
    return;

  printf (" INIT \n");

  __for_allocate            = dlsym (RTLD_NEXT, "for_allocate");
  __for_dealloc_allocatable = dlsym (RTLD_NEXT, "for_dealloc_allocatable");
  __for_deallocate          = dlsym (RTLD_NEXT, "for_deallocate");

  memset (&ptrlist[0], 0, N * sizeof (ptr_t));
}


static inline void * from ()
{
  const int n = 3;
  int i;
  void * addr[n];
  for (i = 0; i < n; i++)
    addr[i] = NULL;
  int m = backtrace (addr, n);
  return addr[2];
}

static inline void newptr (void * Ptr, size_t Siz)
{
  int i;
  char c = '+';
  for (i = 0; i < N; i++)
    if (ptrlist[i].ptr == NULL)
      {
        ptrlist[i].ptr = Ptr;
        ptrlist[i].siz = Siz;
        goto done;
      }
  abort ();
done:
  return;
}


static inline size_t delptr (void * Ptr)
{
  size_t Siz = 0;
  int i;
  for (i = 0; i < N; i++)
    if (ptrlist[i].ptr == Ptr)
      {
        ptrlist[i].ptr = NULL;
        Siz = ptrlist[i].siz;
        break;
      }
  return Siz;
}

#define SIZE 128

void * for_allocate (size_t size, void ** pptr, void ** a)
{
  init ();

  void * ret = __for_allocate (size + 2 * SIZE, pptr, a);

  newptr (*pptr, size + 2 * SIZE);

  char * c = (char*)*pptr;
  for (int i = 0; i < SIZE; i++)
    c[i] = c[i+size+SIZE] = 'X';


  printf ("> ptr = 0x%llx (%d)\n", *pptr, size);

  *pptr = (char*)(*pptr) + SIZE;

  return ret;
}


static void checkptr (void * ptr, size_t size)
{
  char * c = (char*)ptr;
  for (int i = 0; i < SIZE; i++)
    {
      if (c[i] != 'X')
        abort ();
      if (c[i+size+SIZE] != 'X')
        abort ();
    }

}

void * for_dealloc_allocatable (void * ptr, void ** a)
{
  init ();

  void * p = ptr - SIZE;
  size_t size = delptr (p);

  printf ("< ptr = 0x%llx (%d)\n", p, size - 2 * SIZE);

  if (size)
    {
      ptr = p;
      checkptr (ptr, size - 2 * SIZE);
    }

  void * ret = __for_dealloc_allocatable (ptr, a);

  return ret;
}


void * for_deallocate (void * ptr, void ** a)
{
  init ();

  void * p = ptr - SIZE;
  size_t size = delptr (p);

  printf ("< ptr = 0x%llx (%lld)\n", p, size - 2 * SIZE);

  if (size)
    {
      ptr = p;
      checkptr (ptr, size - 2 * SIZE);
    }

  void * ret = __for_deallocate (ptr, a);

  return ret;
}



