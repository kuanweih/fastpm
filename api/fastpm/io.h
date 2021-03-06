#ifndef __FASTPM_IO_H__
#define __FASTPM_IO_H__
#include <bigfile.h>
FASTPM_BEGIN_DECLS

typedef void (*FastPMSnapshotSorter)(const void * ptr, void * radix, void * arg);

void
FastPMSnapshotSortByID(const void * ptr, void * radix, void * arg);

void
FastPMSnapshotSortByLength(const void * ptr, void * radix, void * arg);

void
FastPMSnapshotSortByAEmit(const void * ptr, void * radix, void * arg);

void
fastpm_sort_snapshot(FastPMStore * p, MPI_Comm comm, FastPMSnapshotSorter sorter, int redistribute);

int
write_snapshot(FastPMSolver * fastpm,
        FastPMStore * p,
        const char * filebase,
        const char * dataset,
        const char * parameters,
        int Nwriters
    );

int
append_snapshot(FastPMSolver * fastpm,
        FastPMStore * p,
        const char * filebase,
        const char * dataset,
        const char * parameters,
        int Nwriters
    );

int
write_snapshot_data(FastPMStore * p,
        int Nwriters,
        int append,
        BigFile * bf,
        const char * dataset,
        MPI_Comm comm
);

void
write_snapshot_header(FastPMSolver * fastpm, FastPMStore * p,
    const char * parameters, BigFile * bf, MPI_Comm comm);

int
read_snapshot(FastPMSolver * fastpm, FastPMStore * p, const char * filebase);

int
write_complex(PM * pm, FastPMFloat * data, const char * filename, const char * blockname, int Nwriters);

int
read_complex(PM * pm, FastPMFloat * data, const char * filename, const char * blockname, int Nwriters);

size_t
read_angular_grid(FastPMStore * store,
        const char * filename,
        const double * r,
        const double * aemit,
        const size_t Nr,
        int sampling_factor,
        MPI_Comm comm);

void
write_aemit_hist(const char * fn, const char * ds,
            FastPMStore * p,
            double amin, double amax, size_t nbins,
            MPI_Comm comm);

FASTPM_END_DECLS
#endif
