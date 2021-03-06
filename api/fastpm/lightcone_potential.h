FASTPM_BEGIN_DECLS

typedef struct{
      int subsample_factor;/*Subsample grid by this factor (default=1, no subsampling).
                        If negative, read subsample from a file */
      char * ra_dec_filename;
      ptrdiff_t start_indx,stop_indx; /*keep track of start and end index of these particles in the
                                      lightcone*/
      size_t size;
      double a_max,a_min;
      int n_a;
      double *a,*r; //a, r array for the line of sight grid.
    } grid_params; //parameters for reading in ra-dec grids


typedef struct {
    int compute_potential;
    /* Storage of the particles on the light cone */
    FastPMCosmology * cosmology;
    double speedfactor;
    double glmatrix[4][4];
    double fov; /* field of view angle. <=0 for flatsky.
                    Remember the lightcone is always along z-direction.*/

    /* private: */
    FastPMStore * p0; /* per step potential and tidal field.  */
    FastPMStore * q; /* stores the output, uniform sampling of potential on lightcone */

    /*FIXME no required anymore here?*/
    FastPMStore * p; /* storing the output, particles on lightcone */


    double (* tileshifts)[3];
    int ntiles;//number of times box is repeated or tiled.


    int read_ra_dec;/*If zero, lightcone on xyz grid. If non-zero then read in read_ra_dec number
                    of grids. eg. if read_ra_dec=2, read in 2 grids*/

    grid_params *shell_params;

    double a_prev,a_now; //a at previous force calculcation and current force calculation

    double G_prev,G_now; //Growth function at previous force calculcation and current force calculation

    ptrdiff_t interp_start_indx;
    ptrdiff_t interp_stop_indx;  /*lc particle index from which we need to start interpolation
                                  after force step calculation */
    ptrdiff_t *interp_q_indx;/*list of indices on the uniform grid, for lc particles.
                      For now we will
                        keep it as array to size of lc particle array. Can be shortened to a list of
                        particles that require interpolation.*/
                        /*XXX We don't need to worry about particle entering lightcone more than once. Right?*/

    struct {
        double * Dc;
        double *Growth;
        size_t size;
    } EventHorizonTable;

    /* Need a table for drift factors */

    void * gsl; // GSL solver pointer

} FastPMLightConeP;

double
fastpm_lcp_horizon(FastPMLightConeP * lcp, double a);

int
fastpm_lcp_intersect(FastPMSolver * fastpm, FastPMTransitionEvent *event, FastPMLightConeP * lcp);

void
fastpm_lcp_init(FastPMLightConeP * lcp, FastPMSolver * fastpm,
                double (*tileshifts)[3], int ntiles);

void
fastpm_lcp_destroy(FastPMLightConeP * lcp);

double
fastpm_lcp_horizon(FastPMLightConeP * lcp, double a);

int
fastpm_lcp_compute_potential(FastPMSolver * fastpm,
        FastPMForceEvent * event,
        FastPMLightConeP * lcp);

double(* fastpm_lcp_tile(FastPMSolver *fastpm,int tile_x, int tile_y, int tile_z,
                        int shift_one_negative, int *ntiles,double (*tiles)[3]))[3];

FASTPM_END_DECLS
