/*********************
 * Time intergral KDK scheme.
 * kick and drifts.
 * 
 * This code was initially modified by Jun Koda, 
 * from the original serial COLA code
 * by Svetlin Tassev.
 *
 * The kick and drift still supports a COLA compat-mode.
 * Most of the nasty factors are for COLA compat-mode
 * (not needed in PM)
 * We also added a 2LPT mode that does just 2LPT.
 *
 *  Yu Feng <rainwoodman@gmail.com> 
 *
 */

#include <math.h>
#include <assert.h>
#include <mpi.h>

#include <gsl/gsl_integration.h>
#include <gsl/gsl_roots.h>
#include <gsl/gsl_sf_hyperg.h> 
#include <gsl/gsl_errno.h>

#include "particle.h"
#include "msg.h"
#include "stepping.h"
#include "timer.h"

static float Om= -1.0f;
static float subtractLPT= 1.0f;
static const float nLPT= -2.5f;
static int stdDA = 0; // velocity growth model
static int noPM;
static int martinKick = 0;

float growthD(const float a);
float growthD2(const float a);
double Sphi(double ai, double af, double aRef);
double Sq(double ai, double af, double aRef);
float Qfactor(const float a);

// Leap frog time integration
// ** Total momentum adjustment dropped
void stepping_set_subtract_lpt(int flag) {
    subtractLPT = flag?1.0:0.0;
}
void stepping_set_std_da(int flag) {
    stdDA = flag;
}
void stepping_set_no_pm(int flag) {
    noPM = flag;
}

void stepping_kick(Particles* const particles, const float Omega_m,
        const float ai, const float af, const float ac)
/* a_v     avel1     a_x*/
{
                                                          timer_start(evolve);  
  msg_printf(normal, "Kick %g -> %g\n", ai, af);

  Om= Omega_m;
  const float Om143= pow(Om/(Om + (1 - Om)*ac*ac*ac), 1.0/143.0);
  const float dda= Sphi(ai, af, ac);
  const float growth1=growthD(ac);

  msg_printf(normal, "growth factor %g dda=%g \n", growth1, dda);

  const float q2=1.5*Om*growth1*growth1*(1.0 + 7.0/3.0*Om143);
  const float q1=1.5*Om*growth1;

  float Om_;
  if (martinKick)
      Om_ = Om/ (Om + (1 - Om) *ac * ac *ac);
  else
      Om_ = Om;

  Particle* const P= particles->p;
  const int np= particles->np_local;
  float3* const f= particles->force;
  
  // Kick using acceleration at a= ac
  // Assume forces at a=ac is in particles->force

#ifdef _OPENMP
  #pragma omp parallel for default(shared)
#endif
  for(int i=0; i<np; i++) {
    float ax= -1.5*Om_*f[i][0] - subtractLPT*(P[i].dx1[0]*q1 + P[i].dx2[0]*q2);
    float ay= -1.5*Om_*f[i][1] - subtractLPT*(P[i].dx1[1]*q1 + P[i].dx2[1]*q2);
    float az= -1.5*Om_*f[i][2] - subtractLPT*(P[i].dx1[2]*q1 + P[i].dx2[2]*q2);

    if(!noPM) {
        P[i].v[0] += ax * dda;
        P[i].v[1] += ay * dda;
        P[i].v[2] += az * dda;
    } else {
        P[i].v[0] = 0;
        P[i].v[1] = 0;
        P[i].v[2] = 0;
    }
  }

  //velocity is now at a= avel1
                                                           timer_stop(evolve);  
}

void stepping_drift(Particles* const particles, const float Omega_m,
        const float ai, const float af, const float ac)
    /*a_x, apos1, a_v */
{
                                                          timer_start(evolve);
  Particle* const P= particles->p;
  const int np= particles->np_local;

  
  const float dyyy=Sq(ai, af, ac);

  const float da1= growthD(af) - growthD(ai);    // change in D_{1lpt}
  const float da2= growthD2(af) - growthD2(ai);  // change in D_{2lpt}

  msg_printf(normal, "Drift %g -> %g\n", ai, af);
  msg_printf(normal, "dyyy = %g \n", dyyy);
    
  // Drift
#ifdef _OPENMP
  #pragma omp parallel for default(shared)
#endif
  for(int i=0; i<np; i++) {
    P[i].x[0] += P[i].v[0]*dyyy + 
                 subtractLPT*(P[i].dx1[0]*da1 + P[i].dx2[0]*da2);
    P[i].x[1] += P[i].v[1]*dyyy +
                 subtractLPT*(P[i].dx1[1]*da1 + P[i].dx2[1]*da2);
    P[i].x[2] += P[i].v[2]*dyyy + 
                 subtractLPT*(P[i].dx1[2]*da1 + P[i].dx2[2]*da2);
  }
    
                                                            timer_stop(evolve);
}

float growthDtemp(const float a){
    // Decided to use the analytic expression for LCDM. More transparent if I change this to numerical integration?
    float x=-Om/(Om - 1.0)/(a*a*a);
    
    
    float hyperP=0,hyperM=0;
    
    if (fabs(x-1.0) < 1.e-3) {
      hyperP= 0.859596768064608 - 0.1016599912520404*(-1.0 + x) + 0.025791094277821357*pow(-1.0 + x,2) - 0.008194025861121475*pow(-1.0 + x,3) + 0.0029076305993447644*pow(-1.0 + x,4) - 0.0011025426387159761*pow(-1.0 + x,5) + 0.00043707304964624546*pow(-1.0 + x,6) - 0.0001788889964687831*pow(-1.0 + x,7);
      hyperM= 1.1765206505266006 + 0.15846194123099624*(-1.0 + x) - 0.014200487494738975*pow(-1.0 + x,2) + 0.002801728034399257*pow(-1.0 + x,3) - 0.0007268267888593511*pow(-1.0 + x,4) + 0.00021801569226706922*pow(-1.0 + x,5) - 0.00007163321597397065*pow(-1.0 + x,6) +    0.000025063737576245116*pow(-1.0 + x,7);
    }
    else {
      if (x < 1.0) {
	hyperP=gsl_sf_hyperg_2F1(1.0/2.0,2.0/3.0,5.0/3.0,-x);
	hyperM=gsl_sf_hyperg_2F1(-1.0/2.0,2.0/3.0,5.0/3.0,-x);
      }
      x=1.0/x;
      if ((x < 1.0) && (x>1.0/30)) {
	
	hyperP=gsl_sf_hyperg_2F1(-1.0/6.0,0.5,5.0/6.0,-x);
	hyperP*=4*sqrt(x);
	hyperP+=-3.4494794123063873799*pow(x,2.0/3.0);
        
	hyperM=gsl_sf_hyperg_2F1(-7.0/6.0,-0.5,-1.0/6.0,-x);
	hyperM*=4.0/7.0/sqrt(x);
	hyperM+=pow(x,2.0/3.0)*(-1.4783483195598803057); //-(Gamma[-7/6]*Gamma[5/3])/(2*sqrt[Pi])
      }
      if (x<=1.0/30.0){
	hyperP=3.9999999999999996*sqrt(x) - 3.4494794123063865*pow(x,0.6666666666666666) + 0.3999999999999999*pow(x,1.5) -    0.13636363636363635*pow(x,2.5) + 0.07352941176470587*pow(x,3.5) - 0.04755434782608695*pow(x,4.5) +    0.033943965517241374*pow(x,5.5) - 0.02578125*pow(x,6.5) + 0.020436356707317072*pow(x,7.5) -    0.01671324384973404*pow(x,8.5) + 0.013997779702240564*pow(x,9.5) - 0.011945562847590041*pow(x,10.5) + 0.01035003662109375*pow(x,11.5) - 0.009080577904069926*pow(x,12.5);
	hyperM=0.5714285714285715/sqrt(x) + 2.000000000000001*sqrt(x) - 1.4783483195598794*pow(x,0.66666666666666666) +    0.10000000000000002*pow(x,1.5) - 0.022727272727272735*pow(x,2.5) + 0.009191176470588237*pow(x,3.5) -    0.004755434782608697*pow(x,4.5) + 0.002828663793103449*pow(x,5.5) - 0.0018415178571428578*pow(x,6.5) +    0.0012772722942073172*pow(x,7.5) - 0.0009285135472074472*pow(x,8.5) + 0.0006998889851120285*pow(x,9.5) -    0.0005429801294359111*pow(x,10.5) + 0.0004312515258789064*pow(x,11.5) - 0.00034925299631038194*pow(x,12.5);
      }
    }
    
    
    if (a > 0.2) 
      return sqrt(1.0 + (-1.0 + pow(a,-3))*Om)*(3.4494794123063873799*pow(-1.0 + 1.0/Om,0.666666666666666666666666666) + (hyperP*(4*pow(a,3)*(-1.0 + Om) - Om) - 7.0*pow(a,3)*hyperM*(-1.0 + Om))/(pow(a,5)*(-1.0+ Om) - pow(a,2)*Om));

    return (a*pow(1 - Om,1.5)*(1291467969*pow(a,12)*pow(-1 + Om,4) + 1956769650*pow(a,9)*pow(-1 + Om,3)*Om + 8000000000*pow(a,3)*(-1 + Om)*pow(Om,3) + 37490640625*pow(Om,4)))/(1.5625e10*pow(Om,5));    
}

float growthD(const float a) { // growth factor for LCDM
    return growthDtemp(a)/growthDtemp(1.0);
}


float Qfactor(const float a) { // Q\equiv a^3 H(a)/H0.
    return sqrt(Om/(a*a*a)+1.0-Om)*a*a*a;
}




float growthD2temp(const float a){
    float d= growthD(a);
    float omega=Om/(Om + (1.0 - Om)*a*a*a);
    return d*d*pow(omega, -1.0/143.);
}

float growthD2(const float a) {// Second order growth factor
  return growthD2temp(a)/growthD2temp(1.0); // **???
}


float growthD2v(const float a){ // explanation is in main()
    float d2= growthD2(a);
    float omega=Om/(Om + (1.0 - Om)*a*a*a);
    return Qfactor(a)*(d2/a)*2.0*pow(omega, 6.0/11.);
}

float decayD(float a){ // D_{-}, the decaying mode
    return sqrt(Om/(a*a*a)+1.0-Om);
}

double DprimeQ(double a,float nGrowth)
{ // returns Q*d(D_{+}^nGrowth*D_{-}^nDecay)/da, where Q=Qfactor(a)
  float nDecay=0.0;// not interested in decay modes in this code.
  float Nn=6.0*pow(1.0 - Om,1.5)/growthDtemp(1.0);
  return (pow(decayD(a),-1.0 + nDecay)*pow(growthD(a),-1.0 + nGrowth)*(nGrowth*Nn- (3.0*(nDecay + nGrowth)*Om*growthD(a))/(2.*a)));  
}

 

//
// Functions for our modified time-stepping (used when StdDA=0):
//

double gpQ(double a) { 
  return pow(a, nLPT);
}

double fun (double a, void * params) {
  double f;
  if (stdDA==0) f = gpQ(a)/Qfactor(a); 
  else f = 1.0/Qfactor(a);
  
  return f;
}

double fun2 (double a, void * params) {
  double f;
  if (stdDA==0) abort();
  else f = a/Qfactor(a);
  
  return f;
}

double fun2martin (double a, void * params) {
  double f;
  if (stdDA==0) abort();
  else f = Qfactor(a) / (a*a);
  
  return f;
}

/*     
      When StdDA=0, one needs to set nLPT.
         assumes time dep. for velocity = B a^nLPT
         nLPT is a real number. Sane values lie in the range (-4,3.5). Cannot be 0, but of course can be -> 0 (say 0.001).
         See Section A.3 of TZE.
*/

double Sq(double ai, double af, double aRef) {
  gsl_integration_workspace * w 
    = gsl_integration_workspace_alloc (5000);
  
  double result, error;
  double alpha=0;
  
  gsl_function F;
  F.function = &fun;
  F.params = &alpha;
  
  gsl_integration_qag (&F, ai, af, 0, 1e-5, 5000,6,
		       w, &result, &error); 
  
  gsl_integration_workspace_free (w);
     
  if (stdDA==0)
    return result/gpQ(aRef);
  return result;
}
     
double DERgpQ(double a) { // This must return d(gpQ)/da
  return nLPT*pow(a, nLPT-1);
}
     
double Sphi(double ai, double af, double aRef) {
  double result;
  if (stdDA==0) {
      result=(gpQ(af)-gpQ(ai))*aRef/Qfactor(aRef)/DERgpQ(aRef);
      return result;
  } else {
      gsl_integration_workspace * w 
        = gsl_integration_workspace_alloc (5000);
      
      double result, error;
      double alpha=0;
      
      gsl_function F;
      if (martinKick) {
          F.function = &fun2martin;
      } else{
          F.function = &fun2;
      }
      F.params = &alpha;
      
      gsl_integration_qag (&F, ai, af, 0, 1e-5, 5000,6,
                   w, &result, &error); 
      
      gsl_integration_workspace_free (w);
      return result; 
  }
  
  return result;
}


// Interpolate position and velocity for snapshot at a=aout
void set_nonstepping_initial(const float aout, const Particles * const particles, Snapshot* const snapshot)
{
                                                           timer_start(interp);
  
  const int np= particles->np_local;
  Particle * p= particles->p;

  ParticleMinimum* const po= snapshot->p;
  Om= snapshot->omega_m; assert(Om >= 0.0f);

  msg_printf(verbose, "Setting up inital snapshot at a= %4.2f (z=%4.2f).\n", aout, 1.0f/aout-1);

  //const float vfac=A/Qfactor(A); // RSD /h Mpc unit
  const float vfac= 100.0f/aout;   // km/s; H0= 100 km/s/(h^-1 Mpc)

  //const float AI=  particles->a_v;
  //const float A=   particles->a_x;
  //const float AF=  aout;

  const float Dv=DprimeQ(aout, 1.0); // dD_{za}/dy
  const float Dv2=growthD2v(aout);   // dD_{2lpt}/dy

  //const float da1= growthD(AF);
  //const float da2= growthD2(AF);

  //msg_printf(debug, "initial growth factor %e %e\n", da1, da2);
  msg_printf(debug, "initial velocity factor %5.3f %e %e\n", aout, vfac*Dv, vfac*Dv2);

#ifdef _OPENMP
  #pragma omp parallel for default(shared)  
#endif
  for(int i=0; i<np; i++) {
    if(subtractLPT) {
        p[i].v[0] = 0;
        p[i].v[1] = 0;
        p[i].v[2] = 0;
    } else {
        p[i].v[0] = (p[i].dx1[0]*Dv + p[i].dx2[0]*Dv2);
        p[i].v[1] = (p[i].dx1[1]*Dv + p[i].dx2[1]*Dv2);
        p[i].v[2] = (p[i].dx1[2]*Dv + p[i].dx2[2]*Dv2);
    }
    po[i].v[0] = vfac*(p[i].dx1[0]*Dv + p[i].dx2[0]*Dv2);
    po[i].v[1] = vfac*(p[i].dx1[1]*Dv + p[i].dx2[1]*Dv2);
    po[i].v[2] = vfac*(p[i].dx1[2]*Dv + p[i].dx2[2]*Dv2);

    po[i].x[0] = p[i].x[0];
    po[i].x[1] = p[i].x[1];
    po[i].x[2] = p[i].x[2];

    po[i].id = p[i].id;
  }

  snapshot->np_local= np;
  snapshot->np_total= particles->np_total;
  snapshot->np_average= particles->np_average;
  snapshot->a= aout;
  snapshot->qfactor = Qfactor(aout);
                                                           timer_stop(interp);
}

// Interpolate position and velocity for snapshot at a=aout
void stepping_set_snapshot(const double aout, double a_x, double a_v, Particles const * const particles, Snapshot* const snapshot)
{
                                                           timer_start(interp);
  const int np= particles->np_local;
  Particle const * const p= particles->p;
  float3* const f= particles->force;

  ParticleMinimum* const po= snapshot->p;
  Om= snapshot->omega_m; assert(Om >= 0.0f);

  msg_printf(verbose, "Setting up snapshot at a= %4.2f (z=%4.2f) <- %4.2f %4.2f.\n", aout, 1.0f/aout-1, a_x, a_v);

  //const float vfac=A/Qfactor(A); // RSD /h Mpc unit
  const float vfac= 100.0f/aout;   // km/s; H0= 100 km/s/(h^-1 Mpc)

  const float AI=  a_v;
  const float A=   a_x;
  const float AF=  aout;

  const float Om143= pow(Om/(Om + (1 - Om)*A*A*A), 1.0/143.0);
  const float dda= Sphi(AI, AF, A);
  const float growth1=growthD(A);

  //msg_printf(normal, "set snapshot %f from %f %f\n", aout, AI, A);
  //msg_printf(normal, "Growth factor of snapshot %f (a=%.3f)\n", growth1, A);
  msg_printf(normal, "Growth factor of snapshot %f (a=%.3f)\n", growthD(AF), AF);

  const float q1=1.5*Om*growth1;
  const float q2=1.5*Om*growth1*growth1*(1.0 + 7.0/3.0*Om143);

  const float Dv=DprimeQ(aout, 1.0); // dD_{za}/dy
  const float Dv2=growthD2v(aout);   // dD_{2lpt}/dy


  const float AC= a_v;
  const float dyyy=Sq(A, AF, AC);


  /*
  if(AF < A) {
    float dyyy_backward= Sq(aminus, aout, AC) - Sq(aminus, A, AC);
    msg_printf(debug, "dyyy drift backward %f ->%f = %e %e\n", 
	       A, AF, dyyy, dyyy_backward);
  }
  */

  msg_printf(debug, "velocity factor %e %e\n", vfac*Dv, vfac*Dv2);
  msg_printf(debug, "RSD factor %e\n", aout/Qfactor(aout)/vfac);


  const float da1= growthD(AF) - growthD(A);    // change in D_{1lpt}
  const float da2= growthD2(AF) - growthD2(A);  // change in D_{2lpt}

#ifdef _OPENMP
  #pragma omp parallel for default(shared)  
#endif
  for(int i=0; i<np; i++) {
    // Kick + adding back 2LPT velocity + convert to km/s
    float ax= -1.5*Om*f[i][0] - subtractLPT*(p[i].dx1[0]*q1 + p[i].dx2[0]*q2);
    float ay= -1.5*Om*f[i][1] - subtractLPT*(p[i].dx1[1]*q1 + p[i].dx2[1]*q2);
    float az= -1.5*Om*f[i][2] - subtractLPT*(p[i].dx1[2]*q1 + p[i].dx2[2]*q2);

    po[i].v[0] = vfac*(p[i].v[0] + ax*dda +
		       (p[i].dx1[0]*Dv + p[i].dx2[0]*Dv2)*subtractLPT);
    po[i].v[1] = vfac*(p[i].v[1] + ay*dda +
		       (p[i].dx1[1]*Dv + p[i].dx2[1]*Dv2)*subtractLPT);
    po[i].v[2] = vfac*(p[i].v[2] + az*dda +
		       (p[i].dx1[2]*Dv + p[i].dx2[2]*Dv2)*subtractLPT);

    // Drift
    po[i].x[0] = p[i].x[0] + p[i].v[0]*dyyy + 
                 subtractLPT*(p[i].dx1[0]*da1 + p[i].dx2[0]*da2);
    po[i].x[1] = p[i].x[1] + p[i].v[1]*dyyy +
                 subtractLPT*(p[i].dx1[1]*da1 + p[i].dx2[1]*da2);
    po[i].x[2] = p[i].x[2] + p[i].v[2]*dyyy + 
                 subtractLPT*(p[i].dx1[2]*da1 + p[i].dx2[2]*da2);

    po[i].id = p[i].id;
  }

  snapshot->np_local= np;
  snapshot->a= aout;
  snapshot->qfactor = Qfactor(aout);
                                                         timer_stop(interp);
}