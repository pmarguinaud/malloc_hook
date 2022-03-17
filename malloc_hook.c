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
#include <pthread.h>


static pthread_spinlock_t lock;

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
static double nan = 0.0;

typedef struct
{
  void * ptr;
  size_t siz;
  void * loc;
} ptr_t;

#define NPTR 3000000
#define SIZE 128

static ptr_t ptrlist[NPTR];
static int enabled = 0;
static int fill = 0;


static inline void init ()
{
  if (__for_allocate != NULL)
    return;

  printf (" INIT X \n");

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

  char * MALLOC_HOOK = getenv ("MALLOC_HOOK");

  if (MALLOC_HOOK == NULL)
    return;

  if (strcmp (MALLOC_HOOK, "1"))
    return;


  enabled = 1;

  memset (&ptrlist[0], 0, NPTR * sizeof (ptr_t));
  count = 0;

  pthread_spin_init (&lock, PTHREAD_PROCESS_SHARED); 

  char * MALLOC_HOOK_FILL = getenv ("MALLOC_HOOK_FILL");

  if (MALLOC_HOOK_FILL != NULL)
    if (strcmp (MALLOC_HOOK_FILL, "1") == 0)
      fill = 1;

  nan = 0./0.;

  printf (" enabled = %d, fill = %d\n", enabled, fill);

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

static inline void newptr (void * Ptr, size_t Siz, void * Loc)
{
  if (count >= NPTR)
    abort ();

  if (ptrlist[count].ptr != NULL)
    abort ();

  ptrlist[count].ptr = Ptr;
  ptrlist[count].siz = Siz;
  ptrlist[count].loc = Loc;

  size_t * pind = (size_t*)Ptr;

  *pind = count;

  char * c = (char*)Ptr;
  for (int i = sizeof (size_t); i < SIZE; i++)
    c[i] = 'X';
 

  if (fill)
    {
      char * nan8 = (char*)&nan;
      for (int i = 0; i < Siz - 2 * SIZE; i++)
        c[SIZE+i] = nan8[i%8];
    }

  for (int i = 0; i < SIZE; i++)
    c[i+Siz-SIZE] = 'X';

  count++;

  return;
}


static inline int delptr (void * Ptr, ptr_t * pp)
{
  size_t Ind = *((size_t*)Ptr);
  ptr_t pp0;

  pp0.siz = 0; pp0.ptr = pp0.loc = NULL;

 *pp = pp0;

  if (Ind >= count)
    goto end;

  if (ptrlist[Ind].ptr != Ptr)
   {
     int i;
     for (i = 0; i < count; i++)
       if (ptrlist[i].ptr == Ptr)
         {
          *pp = ptrlist[i];
           Ind = i;
           break;
         }
    }
  else
    {
     *pp = ptrlist[Ind];
    }

  /* Not found */
  if (pp->siz == 0)
    goto end;

  if (Ind != count-1)
    {
      ptrlist[Ind] = ptrlist[count-1];
      size_t *pInd = (size_t*)ptrlist[Ind].ptr;
      *pInd = Ind;
    }

  ptrlist[count-1] = pp0;
  count--;


end:
  return pp->siz;
}

static void * alloc (size_t size, void ** pptr, void ** a, alloc_t alloc_fun)
{
  void * ret = alloc_fun (size + 2 * SIZE, pptr, a);
  void * Loc[3];
  
  backtrace (&Loc[0], 3);
  
  newptr (*pptr, size + 2 * SIZE, Loc[2]);

  if (verbose)
    printf ("> ptr = 0x%llx (%d) (%d) (0x%llx)\n", *pptr, size, count, Loc[2]); 

  *pptr = (char*)(*pptr) + SIZE;

  return ret;
}

static void checkptr (void * ptr, size_t size, void * Loc)
{
  char * c = (char*)ptr;

  for (int i = sizeof (size_t); i < SIZE; i++)
    if (c[i] != 'X')
      {
        fprintf (stderr, "0x%llx of size %lld allocated at 0x%llx\n", ptr, size, Loc);
        abort ();
      }

  for (int i = 0; i < SIZE; i++)
    if (c[i+size+SIZE] != 'X')
      {
        fprintf (stderr, "0x%llx of size %lld allocated at 0x%llx\n", ptr, size, Loc);
        abort ();
      }

}

static void * dealloc (void * ptr, void ** a, dealloc_t dealloc_fun)
{
  void * p = ptr - SIZE;
  void * Loc = NULL;
  ptr_t pp;

  if (delptr (p, &pp))
    {
      ptr = p;
      checkptr (ptr, pp.siz - 2 * SIZE, pp.loc);
    }

  void * ret = dealloc_fun (ptr, a);

  if (verbose)
    printf ("< ptr = 0x%llx (%d) (%d)\n", p, pp.siz - 2 * SIZE, count);

  return ret;
}

void * for_allocate (size_t size, void ** pptr, void ** a)
{
  init ();
  if (enabled)
    {
      void * ret;
     
      pthread_spin_lock (&lock);
      ret = alloc (size, pptr, a, __for_allocate);
      pthread_spin_unlock (&lock);
     
      return ret;
    }
  else
    {
      return __for_allocate (size, pptr, a);
    }
}

void * for_alloc_allocatable (size_t size, void ** pptr, void ** a)
{
  init ();
  if (enabled)
    {
      void * ret;

      pthread_spin_lock (&lock);
      ret = alloc (size, pptr, a, __for_allocate);
      pthread_spin_unlock (&lock);

      return ret;
    }
  else
    {
      return __for_allocate (size, pptr, a);
    }
}

void * for_dealloc_allocatable (void * ptr, void ** a)
{
  init ();
  if (enabled)
    {
      void * ret;

      pthread_spin_lock (&lock);
      ret = dealloc (ptr, a, __for_deallocate);
      pthread_spin_unlock (&lock);

      return ret;
    }
  else
    {
      return __for_deallocate (ptr, a);
    }
}


void * for_deallocate (void * ptr, void ** a)
{
  init ();
  if (enabled)
    {
      void * ret;
      
      pthread_spin_lock (&lock);
      ret = dealloc (ptr, a, __for_deallocate);
      pthread_spin_unlock (&lock); 

      return ret;
    }
  else
    {
      return __for_deallocate (ptr, a);
    }
}

void malloc_hook_exit_ ()
{
  int i;
  for (i = 0; i < count; i++)
    {
      size_t Siz = ptrlist[i].siz;
      void * Ptr = ptrlist[i].ptr;
      void * Loc = ptrlist[i].loc;
      checkptr (Ptr, Siz - 2 * SIZE, Loc);
    }
}



