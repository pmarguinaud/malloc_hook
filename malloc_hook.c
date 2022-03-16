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


typedef void * (*alloc_t)   (size_t, void **, void **);
typedef void * (*dealloc_t) (void **, void **);

static alloc_t   __for_allocate            = NULL;
static dealloc_t __for_deallocate          = NULL;

static alloc_t   __for_alloc_allocatable   = NULL;
static dealloc_t __for_dealloc_allocatable = NULL;

void * for_allocate (size_t size, void ** pptr, void ** a);
void * for_deallocate (void * ptr, void ** a);

void * for_alloc_allocatable (size_t size, void ** pptr, void ** a);
void * for_dealloc_allocatable (void * ptr, void ** a);

static size_t count = 0;
static int verbose = 0;

typedef struct
{
  void * ptr;
  size_t siz;
} ptr_t;

#define NPTR 3000000
#define SIZE 128

static ptr_t ptrlist[NPTR];

static inline void init ()
{
  if (__for_allocate != NULL)
    return;

  printf (" INIT \n");

  __for_allocate            = dlsym (RTLD_NEXT, "for_allocate");
  __for_deallocate          = dlsym (RTLD_NEXT, "for_deallocate");

  __for_alloc_allocatable   = dlsym (RTLD_NEXT, "for_alloc_allocatable");
  __for_dealloc_allocatable = dlsym (RTLD_NEXT, "for_dealloc_allocatable");

  printf (" __for_allocate            = 0x%llx\n", __for_allocate);
  printf (" __for_deallocate          = 0x%llx\n", __for_deallocate);
  printf (" __for_alloc_allocatable   = 0x%llx\n", __for_alloc_allocatable);
  printf (" __for_dealloc_allocatable = 0x%llx\n", __for_dealloc_allocatable);

  printf ("   for_allocate            = 0x%llx\n",   for_allocate);
  printf ("   for_deallocate          = 0x%llx\n",   for_deallocate);
  printf ("   for_alloc_allocatable   = 0x%llx\n",   for_alloc_allocatable);
  printf ("   for_dealloc_allocatable = 0x%llx\n",   for_dealloc_allocatable);

  memset (&ptrlist[0], 0, NPTR * sizeof (ptr_t));
  count = 0;
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
  if (count >= NPTR)
    abort ();

  if (ptrlist[count].ptr != NULL)
    abort ();

  ptrlist[count].ptr = Ptr;
  ptrlist[count].siz = Siz;

  size_t * pind = (size_t*)Ptr;

  *pind = count;

  char * c = (char*)Ptr;
  for (int i = sizeof (size_t); i < SIZE; i++)
    c[i] = 'X';
  for (int i = 0; i < SIZE; i++)
    c[i+Siz-SIZE] = 'X';

  count++;

  return;
}


static inline size_t delptr (void * Ptr)
{
  size_t Siz = 0;
  size_t Ind = *((size_t*)Ptr);

  if (Ind >= count)
    return 0;

  if (ptrlist[Ind].ptr != Ptr)
   {
     int i;
     Siz = 0;
     for (i = 0; i < count; i++)
       if (ptrlist[i].ptr == Ptr)
         {
           Siz = ptrlist[i].siz;
           Ind = i;
           break;
         }
    }
  else
    {
      Siz = ptrlist[Ind].siz;
    }

  /* Not found */
  if (Siz == 0)
    return 0;


  if (Ind == count-1)
    {
      ptrlist[count-1].ptr = NULL;
      ptrlist[count-1].siz = 0;
      count--;
      return Siz;
    }

  ptrlist[Ind] = ptrlist[count-1];

  size_t *pInd = (size_t*)ptrlist[Ind].ptr;
  *pInd = Ind;

  ptrlist[count-1].ptr = NULL;
  ptrlist[count-1].siz = 0;
  count--;

  return Siz;;
}

static void * alloc (size_t size, void ** pptr, void ** a, alloc_t alloc_fun)
{
  void * ret = alloc_fun (size + 2 * SIZE, pptr, a);

  newptr (*pptr, size + 2 * SIZE);

  if (verbose)
    printf ("> ptr = 0x%llx (%d) (%d)\n", *pptr, size, count); fflush (stdout);

  *pptr = (char*)(*pptr) + SIZE;

  return ret;
}

void * for_allocate (size_t size, void ** pptr, void ** a)
{
  init ();
  return alloc (size, pptr, a, __for_allocate);
}

void * for_alloc_allocatable (size_t size, void ** pptr, void ** a)
{
  init ();
  return alloc (size, pptr, a, __for_allocate);
}

static void checkptr (void * ptr, size_t size)
{
  char * c = (char*)ptr;

  for (int i = sizeof (size_t); i < SIZE; i++)
    if (c[i] != 'X')
      abort ();

  for (int i = 0; i < SIZE; i++)
    if (c[i+size+SIZE] != 'X')
      abort ();

}

static void * dealloc (void * ptr, void ** a, dealloc_t dealloc_fun)
{
  void * p = ptr - SIZE;
  size_t size = delptr (p);

  if (size)
    {
      ptr = p;
      checkptr (ptr, size - 2 * SIZE);
    }

  void * ret = dealloc_fun (ptr, a);

  if (verbose)
    printf ("< ptr = 0x%llx (%d) (%d)\n", p, size - 2 * SIZE, count); fflush (stdout);

  return ret;
}

void * for_dealloc_allocatable (void * ptr, void ** a)
{
  init ();
  return dealloc (ptr, a, __for_deallocate);
}


void * for_deallocate (void * ptr, void ** a)
{
  init ();
  return dealloc (ptr, a, __for_deallocate);
}



