/* Copyright (C) 2003 David Roundy  
%
%  This program is free software; you can redistribute it and/or modify
%  it under the terms of the GNU General Public License as published by
%  the Free Software Foundation; either version 2, or (at your option)
%  any later version.
%
%  This program is distributed in the hope that it will be useful,
%  but WITHOUT ANY WARRANTY; without even the implied warranty of
%  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
%  GNU General Public License for more details.
%
%  You should have received a copy of the GNU General Public License
%  along with this program; if not, write to the Free Software Foundation,
%  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "dactyl.h"
#include "harminv.h"

#define EP(e,r,z) ((e)[(z)+(r)*(nz+1)])
#define DOCMP for (int cmp=0;cmp<2;cmp++)

#define RE(f,r,z) ((f)[0][(z)+(r)*(nz+1)])
#define IM(f,r,z) ((f)[1][(z)+(r)*(nz+1)])

#define CM(f,r,z) ((f)[cmp][(z)+(r)*(nz+1)])
#define IT(f,r,z) (cmp?((f)[0][(z)+(r)*(nz+1)]):(-(f)[1][(z)+(r)*(nz+1)]))

#define BAND(b,r,t) ((b)[(r)+(t)*nr])

const double pi = 3.141592653589793238462643383276;

bandsdata::bandsdata() {
  maxbands = -1;
  tstart = nr = z = 0;
  tend = -1;
  hr = hp = hz = er = ep = ez = NULL;
}

bandsdata::~bandsdata() {
  delete[] hr;
  delete[] hp;
  delete[] hz;
  delete[] er;
  delete[] ep;
  delete[] ez;
}

int src::find_last_source(int sofar) {
  if (peaktime + cutoff > sofar) sofar = (int)peaktime + cutoff;
  if (next == NULL) return sofar;
  return next->find_last_source(sofar);
}

void fields::prepare_for_bands(int z, int ttot, double fmax, double qmin) {
  int last_source = 0;
  if (sources != NULL)
    last_source = sources->find_last_source();
  bands.tstart = (int) (last_source + a*qmin/fmax/c);
  bands.tend = ttot-1;

  if (z >= nz) {
    printf("Specify a lower z for your band structure! (%d > %d)\n",
           z, nz);
    exit(1);
  }
  bands.z = z;
  bands.nr = nr;

  // Set fmin properly...
  double epsmax = 1;
  for (int r=0;r<nr;r++) {
    for (z=0;z<nz;z++) {
      if (EP(ma->eps,r,z) > epsmax) epsmax = EP(ma->eps,r,z);
    }
  }
  const double cutoff_freq = 1.84*c/(2*pi)/nr/epsmax;
  bands.fmin = sqrt(cutoff_freq*cutoff_freq + k*k*c*c);
  bands.fmin = cutoff_freq;
  bands.qmin = qmin;
  // Set fmax and determine how many timesteps to skip over...
  bands.fmax = fmax*(c*inva);
  {
    // for when there are too many data points...
    double decayconst = bands.fmax/qmin*8.0;
    double smalltime = 1./(decayconst + bands.fmax);
    bands.scale_factor = (int)(0.16*smalltime);
    if (bands.scale_factor < 1) bands.scale_factor = 1;
    printf("scale_factor is %d (%lg,%lg)\n",
           bands.scale_factor, bands.fmax, decayconst);
  }

  if (bands.tend <= bands.tstart) {
    printf("Oi, we don't have any time to take a fourier transform!\n");
    printf("FT start is %d and end is %d\n", bands.tstart, bands.tend);
    exit(1);
  }
  bands.ntime = (1+(bands.tend-bands.tstart)/bands.scale_factor);
  bands.a = a;
  bands.inva = inva;
  bands.hr = new cmplx[nr*bands.ntime];
  for (int r=0;r<nr;r++)
    for (int t=0;t<bands.ntime;t++)
      BAND(bands.hr,r,t) = 0.0;
  bands.hp = new cmplx[nr*bands.ntime];
  for (int r=0;r<nr;r++)
    for (int t=0;t<bands.ntime;t++)
      BAND(bands.hp,r,t) = 0.0;
  bands.hz = new cmplx[nr*bands.ntime];
  for (int r=0;r<nr;r++)
    for (int t=0;t<bands.ntime;t++)
      BAND(bands.hz,r,t) = 0.0;
  bands.er = new cmplx[nr*bands.ntime];
  for (int r=0;r<nr;r++)
    for (int t=0;t<bands.ntime;t++)
      BAND(bands.er,r,t) = 0.0;
  bands.ep = new cmplx[nr*bands.ntime];
  for (int r=0;r<nr;r++)
    for (int t=0;t<bands.ntime;t++)
      BAND(bands.ep,r,t) = 0.0;
  bands.ez = new cmplx[nr*bands.ntime];
  if (bands.ez == NULL) {
    printf("Unable to allocate bandstructure array!\n");
    exit(1);
  }
  for (int r=0;r<nr;r++)
    for (int t=0;t<bands.ntime;t++)
      BAND(bands.ez,r,t) = 0.0;
}

void fields::record_bands() {
  if (t > bands.tend || t < bands.tstart) return;
  if (t % bands.scale_factor != 0) return;
  int thet = (t-bands.tstart)/bands.scale_factor;
  if (thet >= bands.ntime) return;
  for (int r=0;r<nr;r++) {
    cmplx *fun = bands.hr;
    BAND(fun,r,thet) =
      cmplx(RE(hr,r,bands.z), IM(hr,r,bands.z));
    BAND(bands.hp,r,thet) =
      cmplx(RE(hp,r,bands.z), IM(hp,r,bands.z));
    BAND(bands.hz,r,thet) =
      cmplx(RE(hz,r,bands.z), IM(hz,r,bands.z));
    BAND(bands.er,r,thet) =
      cmplx(RE(er,r,bands.z), IM(er,r,bands.z));
    BAND(bands.ep,r,thet) =
      cmplx(RE(ep,r,bands.z), IM(ep,r,bands.z));
    BAND(bands.ez,r,thet) =
      cmplx(RE(ez,r,bands.z), IM(ez,r,bands.z));
  }
}

#define HARMOUT(o,r,n,f) ((o)[(r)+(n)*nr+(f)*nr*maxbands])

double fields::get_band(int nn, double fmax, int maxbands) {
  // Need to reimplement this...
  return 0;
}

extern "C" {
  extern void zgesvd_(char *, char *, int *m, int *n, 
                     cmplx *A, int*lda, double *S, cmplx *U, int*ldu,
                     cmplx *VT,int*ldvt,
                     cmplx *WORK, int *lwork,double *RWORK, int*);
}

inline int min(int a, int b) { return (a<b)? a : b; }
inline int max(int a, int b) { return (a>b)? a : b; }

void bandsdata::get_fields(cmplx *eigen, double *f, double *d,
                           int nbands, int n) {
  // eigen has dimensions of nr*maxbands*6
  // n here is the total time
  if (nbands == 1) {
    nbands = 2;
    f[1] = 0;
    d[1] = 15;
  }
  cmplx *A = new cmplx[nbands*n];
  double unitconvert = (2*pi)*c*scale_factor*inva;
  double lowpass_time = 1.0/(scale_factor*fmax);
  printf("Lowpass time is %lg\n", lowpass_time);
  int passtime = (int)(lowpass_time*4);
  for (int i=0;i<nbands;i++) {
    printf("Native freq[%d] is %lg.\n", i, unitconvert*f[i]);
    for (int t=0;t<n;t++) {
      A[i*n+t] = 0;
      for (int tt=max(0,t-passtime);tt<min(n,t+passtime);tt++) {
        A[i*n+t] += exp(cmplx(-unitconvert*d[i]*tt,
                              -unitconvert*f[i]*tt))*
          exp(-(t-passtime)*(t-passtime)/(2.0*lowpass_time*lowpass_time))
          /lowpass_time;
      }
    }
  }
  int one = 1, zero=0, worksize = nbands*n, dummy;
  double *S = new double[nbands];
  cmplx *VT = new cmplx[nbands*nbands];
  cmplx *V = new cmplx[nbands*nbands];
  cmplx *work = new cmplx[worksize];
  double *rwork = new double[5*nbands];
  zgesvd_("O","A",&n,&nbands,A,&n,S,NULL,&one,VT,&nbands,
          work,&worksize,rwork,&dummy);
  // Unhermitian conjugate V
  for (int j=0;j<nbands;j++) {
    printf("S[%d] value is %lg.\n", j, S[j]);
    for (int i=0;i<nbands;i++) {
      V[i*nbands+j] = conj( VT[j*nbands+i]);
    }
  }
  for (int r=0;r<nr;r++) {
    for (int whichf=0;whichf<6;whichf++) {
      cmplx *bdata;
      switch (whichf) {
      case 0: bdata = er; break;
      case 1: bdata = ep; break;
      case 2: bdata = ez; break;
      case 3: bdata = hr; break;
      case 4: bdata = hp; break;
      case 5: bdata = hz; break;
      }
      for (int j=0;j<nbands;j++) {
        cmplx sofar = 0;
        for (int i=0;i<nbands;i++) {
          cmplx sum=0;
          if (S[i] > 1e-6) {
            for (int t=0;t<n;t++) sum += A[i*n+t]*BAND(bdata,r,t);
            sofar += sum*V[i*nbands+j]/S[i];
          }
        }
        HARMOUT(eigen,r,j,whichf) = sofar;
      }
    }
  }

  // Following check is useful to see how much energy you're putting into
  // each mode, but unnecesary for the computation itself.
  for (int i=0;i<nbands;i++) {
    printf("Considering freq[%d] %10lg :  ", i, f[i]);
    double this_energy = 0.0;
    for (int t=0;t<n;t++) {
      for (int r=0;r<nr;r++) {
        for (int whichf=0;whichf<6;whichf++) {
          this_energy += abs(HARMOUT(eigen,r,i,whichf))*
            abs(HARMOUT(eigen,r,i,whichf))*exp(-2*unitconvert*d[i]*t);
        }
      }
    }
    printf("Energy is %lg.\n", sqrt(this_energy));
  }

  delete[] VT;
  delete[] V;
  delete[] S;
  delete[] rwork;
  delete[] work;
  delete[] A;
}

void fields::output_bands(FILE *o, const char *name, int maxbands) {
  const double pi = 3.14159;

  bands.maxbands = maxbands;
  double *tf = new double[maxbands];
  double *td = new double[maxbands];
  cmplx *ta = new cmplx[maxbands];
  double *heref = new double[maxbands];
  double *hered = new double[maxbands];
  cmplx *herea = new cmplx[maxbands];
  const int ntime = bands.ntime;

  cmplx *eigen = new cmplx[nr*maxbands*6];
  cmplx *simple_data = new cmplx[ntime];
  if (!eigen || !simple_data) {
    printf("Error allocating...\n");
    exit(1);
  }

  for (int r=0;r<nr;r++) {
    for (int whichf = 0; whichf < 6; whichf++) {
      for (int n=0;n<maxbands;n++) {
        HARMOUT(eigen,r,n,whichf) = 0;
      }
    }
  }
  int *refnum = new int[maxbands];
  int *refr = new int[maxbands], *refw = new int[maxbands], numref = 0;
  double *reff = new double[maxbands], *refd = new double[maxbands];
  cmplx *refa = new complex<double>[maxbands];
  cmplx *refdata = new complex<double>[maxbands*ntime];
  for (int r=0;r<nr;r+=bands.scale_factor/c*2) {
    cmplx *bdata;
    for (int whichf = 0; whichf < 6; whichf++) {
      printf("Looking at (%d.%d)\n", r, whichf);
      switch (whichf) {
      case 0: bdata = bands.er; break;
      case 1: bdata = bands.ep; break;
      case 2: bdata = bands.ez; break;
      case 3: bdata = bands.hr; break;
      case 4: bdata = bands.hp; break;
      case 5: bdata = bands.hz; break;
      }
      int not_empty = 0;
      for (int t=0;t<ntime;t++) {
        simple_data[t] = BAND(bdata,r,t);
        if (simple_data[t] != 0.0) not_empty = 1;
      }
      if (not_empty) {
        if (numref == 0) { // Have no reference bands so far...
          numref = bands.get_freqs(simple_data, ntime, refa, reff, refd);
          printf("Starting out with %d modes!\n", numref);
          for (int n=0;n<numref;n++) {
            HARMOUT(eigen,r,n,whichf) = refa[n];
            refr[n] = r;
            refw[n] = whichf;
            printf("Here's a mode (%10lg,%10lg) (%10lg,%10lg) -- %d (%d.%d)\n",
                   reff[n], refd[n], real(refa[n]),imag(refa[n]), n, r, whichf);
            for (int t=0;t<ntime;t++)
              refdata[t+n*ntime] = BAND(bdata,r,t);
          }
        } else {
          for (int n=0;n<numref;n++) {
            //printf("Comparing with (%d.%d)\n", refr[n], refw[n]);
            int num_match = bands.get_both_freqs(refdata+n*ntime,simple_data,ntime,
                                                 ta, herea, tf, td);
            //printf("See %d modes at (%d.%d)\n", num_match, r, whichf);
            int best_match=-1;
            double err_best = 1e300;
            for (int i=0;i<num_match;i++) {
              double errf = (abs(tf[i]-reff[n])+abs(td[i]-refd[n]))/abs(reff[n]);
              double erra = abs(ta[i]-refa[n])/abs(refa[n]);
              double err = sqrt(errf*errf + erra*erra);
              if (err > 10*errf) err = 10*errf;
              if (err < err_best) {
                best_match = i;
                err_best = err;
              }
            }
            //printf("Setting amp to %lg (vs %lg)\n",
            //       abs(herea[best_match]), abs(ta[best_match]));
            if (err_best > 0.02 && err_best < 1e299) {
              printf("OOOOOOOOO\n");
              printf("---------\n");
              if (err_best > 0.02) {
                printf("Didn't find a nice frequency! (%lg) (%d.%d) vs (%d.%d)\n",
                       err_best, r, whichf, refr[n], refw[n]);
              } else {
                printf("Found a nice frequency! (%lg) (%d.%d) vs (%d.%d)\n",
                       err_best, r, whichf, refr[n], refw[n]);
              }
              printf("Ref %d: %10lg %10lg\t(%10lg ,%10lg)\n",
                     n, reff[n], refd[n], refa[n]);
              for (int i=0;i<num_match;i++) {
                printf("%5d: %10lg %10lg\t(%10lg ,%10lg)\n",
                       i, tf[i], td[i], ta[i]);
              } 
              printf("---------\n");
              printf("OOOOOOOOO\n");
            } else if (err_best < 0.02) {
              HARMOUT(eigen,r,n,whichf) = herea[best_match];
              if (abs(herea[best_match]) > abs(refa[n])) { // Change reference...
                printf("Changing reference %d to (%d.%d)\n", n, r, whichf);
                printf("Freq goes from %lg to %lg.\n", reff[n], tf[best_match]);
                printf("best_err is %lg\n", err_best);
                printf("amp (%lg,%lg) (%lg,%lg)\n", 
                       real(refa[n]),imag(refa[n]),
                       real(ta[best_match]), imag(ta[best_match]));
                reff[n] = tf[best_match];
                refd[n] = td[best_match];
                refa[n] = herea[best_match];
                refr[n] = r;
                refw[n] = whichf;
                for (int t=0;t<ntime;t++)
                  refdata[t+n*ntime] = BAND(bdata,r,t);
              }
            }
          }
          int num_here = bands.get_freqs(simple_data, ntime, herea, heref, hered);
          if (num_here > numref || 1) {
            // It looks like we see a new mode at this point...
            for (int i=0;i<num_here;i++) refnum[i] = -1;
            for (int n=0;n<numref;n++) {
              int best_match=-1;
              double err_best = 1e300;
              double best_erra;
              cmplx mya = HARMOUT(eigen,r,n,whichf);
              for (int i=0;i<num_here;i++) {
                double errf =
                  (abs(heref[i]-reff[n])+abs(hered[i]-refd[n]))/abs(reff[n]);
                double erra = abs(herea[i]-mya)/abs(refa[n]);
                double err = sqrt(errf*errf + erra*erra);
                if (err > 10*errf) err = 10*errf;
                //printf("heref[%d] %lg vs reff[%d] %lg gives %lg %lg -- %lg\n",
                //       i, heref[i], n, reff[n], errf, erra, abs(mya));
                if (err < err_best && refnum[i] == -1) {
                  best_match = i;
                  err_best = err;
                  best_erra = erra;
                }
              }
              if (err_best < 0.25) {
                refnum[best_match] = n;
                //printf("Got a best err of %lg (%lg) on an f of %lg %d\n",
                //       err_best, best_erra, reff[n], n);
              } else {
                //printf("Got a best err of %lg (%lg) on an f of %lg %d\n",
                //       err_best, best_erra, reff[n], n);
              }
            }
            for (int i=0;i<num_here;i++) {
              if (refnum[i] == -1) { // New mode!!! Change reference...
                reff[numref] = heref[i];
                refd[numref] = hered[i];
                printf("Found one more mode! (%10lg,%10lg) -- %d (%d.%d)\n",
                       heref[i], refd[numref], numref, r, whichf);
                refa[numref] = herea[i];
                refa[numref] = HARMOUT(eigen,r,numref,whichf);
                refr[numref] = r;
                refw[numref] = whichf;
                for (int t=0;t<ntime;t++) 
                  refdata[t+numref*ntime] = BAND(bdata,r,t);
                numref++;
                if (numref > maxbands-2) numref = maxbands-2;
              }
            }
          }
        }
      }
    }
  }
  delete[] tf;
  delete[] td;
  delete[] ta;
  delete[] herea;
  delete[] heref;
  delete[] hered;

  // Sort by frequency...
  for (int i = 1; i < numref; i++) {
    for (int j=i; j>0;j--) {
      if (reff[j]<reff[j-1]) {
        double t1 = reff[j], t2 = refd[j];
        reff[j] = reff[j-1];
        refd[j] = refd[j-1];
        reff[j-1] = t1;
        refd[j-1] = t2;
      }
    }
  }

  bands.get_fields(eigen,reff,refd,numref,ntime);

  for (int i = 0; i < numref; ++i) {
    //printf("Nice mode with freq %lg %lg\n", freqs[i], decays[i]);
    // k m index freq decay Q
    fprintf(o, "%s %lg %d %d %lg %lg %lg\n", name,
            k, m, i, fabs(reff[i]), refd[i],
            (2*pi)*fabs(reff[i]) / (2 * refd[i]));
    for (int r=0;r<nr;r++) {
      fprintf(o, "%s-fields %lg %d %d %lg", name, k, m, i, r*inva);
      for (int whichf = 0; whichf < 6; whichf++) {
        fprintf(o, " %lg %lg", real(HARMOUT(eigen,r,i,whichf)),
                imag(HARMOUT(eigen,r,i,whichf)));
      }
      fprintf(o, "\n");
    }
  } 
  delete[] reff;
  delete[] refd;
  delete[] eigen;
}

int bandsdata::get_both_freqs(cmplx *data1, cmplx *data2, int n,
                              cmplx *amps1, cmplx *amps2, 
                              double *freqs, double *decays) {
  double phi = (rand()%1000)/1000.0;
  int numfound = 0;
  double mag1 = 0, mag2 = 0;
  for (int i=0;i<n;i++)
    mag1 += norm(data1[i]); // norm(a) is actually sqr(abs(a))
  for (int i=0;i<n;i++)
    mag2 += norm(data2[i]); // norm(a) is actually sqr(abs(a))
  do {
    complex<double> shift = polar(1.0,phi);
    complex<double> unshift = polar(1.0,-phi);
    //if (phi != 0.0) printf("CHANGING PHI! (1.0,%lg)\n",phi);
    cmplx *plus = new complex<double>[n];
    cmplx *minus = new complex<double>[n];
    cmplx *Ap = new complex<double>[n];
    cmplx *Am = new complex<double>[n];
    double *fp = new double[maxbands];
    double *fm = new double[maxbands];
    double *dp = new double[maxbands];
    double *dm = new double[maxbands];
    int plusboring = 1;
    int minusboring = 1;
    for (int i=0;i<n;i++) {
      plus[i] = data1[i]+shift*data2[i];
      minus[i] = data1[i]-shift*data2[i];
      if (plus[i] != 0) plusboring = 0;
      if (minus[i] != 0) minusboring = 0;
    }
    if (!plusboring && !minusboring) {
      int numplus = get_freqs(plus, n, Ap, fp, dp);
      int numminus = get_freqs(minus, n, Am, fm, dm);
      if (numplus == numminus) {
        // Looks like we agree on the number of bands...
        numfound = numplus;
        //printf("Mags: %10lg/%10lg\n", mag1, mag2);
        //for (int i=0;i<numfound;i++) {
        //  printf("%10lg/%10lg %10lg/%10lg\t(%11lg,%11lg)/(%11lg,%11lg)\n",
        //         fp[i], fm[i], dp[i], dm[i],
        //         0.5*(Ap[i]+Am[i]), 0.5*(Ap[i]-Am[i])*unshift);
        //}
        for (int i=0;i<numfound;i++) {
          freqs[i] = 0.5*(fp[i]+fm[i]);
          if (0.5*(fp[i]-fm[i]) > 0.1*freqs[i]) {
            printf("We've got some weird frequencies: %lg and %lg\n",
                   fp[i], fm[i]);
          }
          decays[i] = 0.5*(dp[i]+dm[i]);
          amps1[i] = 0.5*(Ap[i]+Am[i]);
          amps2[i] = 0.5*(Ap[i]-Am[i])*unshift;
        }
      }
    }
    delete[] plus;
    delete[] minus;
    delete[] Ap;
    delete[] Am;
    delete[] fp;
    delete[] fm;
    delete[] dp;
    delete[] dm;
    phi += 0.1;
  } while (numfound == 0 && 0);
  return numfound;
}

int bandsdata::get_freqs(cmplx *data, int n,
                         cmplx *amps, double *freqs, double *decays) {
  // data, amps and freqs are size n arrays.
  harminv_data hd = 
    harminv_data_create(n, data, fmin*scale_factor, 
                        fmax*scale_factor, maxbands);

  int prev_nf, cur_nf;
  harminv_solve(hd);
  prev_nf = cur_nf = harminv_get_num_freqs(hd);

  /* keep re-solving as long as spurious solutions are eliminated */
  do {
    prev_nf = cur_nf;
    harminv_solve_again(hd);
    cur_nf = harminv_get_num_freqs(hd);
  } while (cur_nf < prev_nf);
  if (cur_nf > prev_nf)
    fprintf(stderr,
            "harminv: warning, number of solutions increased from %d to %d!\n",
            prev_nf, cur_nf);
  
  cmplx *tmpamps = harminv_compute_amplitudes(hd);

  freqs[0] = a*harminv_get_freq(hd, 0)/c/scale_factor;
  decays[0] = a*harminv_get_decay(hd, 0)/c/scale_factor;
  for (int i = 1; i < harminv_get_num_freqs(hd); ++i) {
    freqs[i] = a*harminv_get_freq(hd, i)/c/scale_factor;
    decays[i] = a*harminv_get_decay(hd, i)/c/scale_factor;
    for (int j=i; j>0;j--) {
      if (freqs[j]<freqs[j-1]) {
        double t1 = freqs[j], t2 = decays[j];
        cmplx a = tmpamps[j];
        tmpamps[j] = tmpamps[j-1];
        freqs[j] = freqs[j-1];
        decays[j] = decays[j-1];
        freqs[j-1] = t1;
        decays[j-1] = t2;
        tmpamps[j-1] = a;
      }
    }
  }
  int num = harminv_get_num_freqs(hd);
  // Now get rid of any spurious low frequency solutions...
  int orignum = num;
  for (int i=0;i<orignum;i++) {
    if (freqs[0] < a/c*fmin*.9) {
      for (int j=0;j<num-1;j++) {
        freqs[j]=freqs[j+1];
        decays[j]=decays[j+1];
        tmpamps[j]=tmpamps[j+1];
      }
      num--;
    }
  }
  // Now get rid of any spurious transient solutions...
  for (int i=num-1;i>=0;i--) {
    if (pi*fabs(freqs[i]/decays[i]) < qmin) {
      num--;
      //printf("Trashing a spurious low Q solution with freq %lg %lg\n",
      //       freqs[i], decays[i]);
      for (int j=i;j<num;j++) {
        freqs[j] = freqs[j+1];
        decays[j] = decays[j+1];
        tmpamps[j] = tmpamps[j+1];
      }
    }
  }
  for (int i = 0; i < num; ++i) {
    amps[i] = tmpamps[i];
  }
  free(tmpamps);
  harminv_data_destroy(hd);
  return num;
}