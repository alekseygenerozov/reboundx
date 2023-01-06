/**
 * @file    spin.c
 * @brief   Add self-consistent spin, tidal and dynamical equations of motion for bodies with structure
 * @author  Tiger Lu <tiger.lu@yale.edu>, Dan Tamayo <tamayo.daniel@gmail.com>, Hanno, Rein <hanno.rein@utoronto.ca>
 *
 * @section     LICENSE
 * Copyright (c) 2015 Dan Tamayo, Hanno Rein
 *
 * This file is part of reboundx.
 *
 * reboundx is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * reboundx is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rebound.  If not, see <http://www.gnu.org/licenses/>.
 *
 * The section after the dollar signs gets built into the documentation by a script.  All lines must start with space * space like below.
 * Tables always must be preceded and followed by a blank line.  See http://docutils.sourceforge.net/docs/user/rst/quickstart.html for a primer on rst.
 * $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
 *
 * $Tides$       // Effect category (must be the first non-blank line after dollar signs and between dollar signs to be detected by script).
 *
 * ======================= ===============================================
 * Authors                 Tiger Lu, Hanno Rein, D. Tamayo, Sam Hadden, Gregory Laughlin
 * Implementation Paper    Lu et al., 2022 (in review).
 * Based on                `Eggleton et al. 1998 <https://ui.adsabs.harvard.edu/abs/1998ApJ...499..853E/abstract>`_.
 * C Example               :ref:`c_example_tides_spin_pseudo_synchronization`, :ref:`c_example_tides_spin_migration_driven_obliquity_tides`, :ref:`c_example_tides_spin_kozai`.
 * Python Example          `TidesSpinPseudoSynchronization.ipynb <https://github.com/dtamayo/reboundx/blob/master/ipython_examples/TidesSpinPseudoSynchronization.ipynb>`_.
 * ======================= ===============================================
 *
 * This effect consistently tracks both the spin and orbital evolution of bodies under constant-time lag tides raised on both the primary and on the orbiting bodies.
 * In all cases, we need to set masses for all the particles that will feel these tidal forces. Particles with only mass are point particles
 * Particles are assumed to have structure (i.e - physical extent & distortion from spin) if the following parameters are set: physical radius particles[i].r, potential Love number of degree 2 k2, and the spin frequency components sx, sy, sz.
 * If we wish to evolve a body's spin components, the fully dimensional moment of inertia moi must be set as well. If this parameter is not set, the spin components will be stationary.
 * Finally, if we wish to consider the effects of tides raised on a specific body, we must set the tidal dissipation parameter sigma as well.
 * See Lu et. al (in review) and Eggleton et. al (1998) above
 *
 * **Effect Parameters**
 *
 * None
 *
 * **Particle Parameters**
 *
 * ============================ =========== ==================================================================
 * Field (C type)               Required    Description
 * ============================ =========== ==================================================================
 * particles[i].r (float)       Yes         Physical radius (required for contribution from tides raised on the body).
 * k2 (float)                   Yes         Potential Love number of degree 2.
 * sx (float)                   Yes         x component of spin vector
 * sy (float)                   Yes         y component of spin vector
 * sz (float)                   Yes         z component of spin vector
 * moi (float)                  No          Moment of inertia
 * sigma (float)                No          Tidal Dissipation Parameter. If not set, defaults to 0
 * ============================ =========== ==================================================================
 *
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include "reboundx.h"

struct reb_vec3d rebx_calculate_spin_orbit_accelerations(struct reb_particle* source, struct reb_particle* target, const double G, const double k2, const double sigma, const double sx, const double sy, const double sz){
  // All quantities associated with SOURCE
  // This is the quadrupole potential/tides raised on the SOURCE
  const double ms = source->m;
  const double Rs = source->r;
  const double mt = target->m;
  const double mtot = ms + mt;
  const double mu_ij = ms * mt / mtot; // have already checked for 0 and inf
  const double big_a = k2 * (Rs * Rs * Rs * Rs * Rs);

  // distance vector FROM j TO i
  const double dx = source->x - target->x;
  const double dy = source->y - target->y;
  const double dz = source->z - target->z;
  const double d2 = dx * dx + dy * dy + dz * dz;
  const double dr = sqrt(d2);

  // Velocity vector: i to j
  const double dvx = source->vx - target->vx;
  const double dvy = source->vy - target->vy;
  const double dvz = source->vz - target->vz;
  //const double vel2 = dvx * dvx + dvy * dvy + dvz * dvz;
  //const double vr = sqrt(vel2);

  struct reb_vec3d tot_force = {0};

  if (k2 != 0.0){
    const double quad_prefactor = mt * big_a / mu_ij;
    const double omega_dot_d = sx * dx + sy * dy + sz * dz;
    const double omega_squared = sx * sx + sy * sy + sz * sz;

    const double t1 = 5. * omega_dot_d * omega_dot_d / (2. * (dr * dr * dr * dr * dr * dr * dr));
    const double t2 = omega_squared / (2. * (dr * dr * dr * dr * dr));
    const double t3 = omega_dot_d / (dr * dr * dr * dr * dr);
    const double t4 = 6. * G * mt / (dr * dr * dr * dr * dr * dr * dr * dr);

    tot_force.x = (quad_prefactor * ((t1 - t2 - t4) * dx - (t3 * sx)));
    tot_force.y = (quad_prefactor * ((t1 - t2 - t4) * dy - (t3 * sy)));
    tot_force.z = (quad_prefactor * ((t1 - t2 - t4) * dz - (t3 * sz)));

    if (sigma != 0.0){
      // EKH FRAMEWORK TIDAL
      const double d_dot_vel = dx*dvx + dy*dvy + dz*dvz;

      // first vector
      const double vec1_x = 3. * d_dot_vel * dx;
      const double vec1_y = 3. * d_dot_vel * dy;
      const double vec1_z = 3. * d_dot_vel * dz;

      // h vector - EKH
      const double hx = dy * dvz - dz * dvy;
      const double hy = dz * dvx - dx * dvz;
      const double hz = dx * dvy - dy * dvx;

      // h - r^2 Omega
      const double comp_2_x = hx - d2 * sx;
      const double comp_2_y = hy - d2 * sy;
      const double comp_2_z = hz - d2 * sz;

      // second vector
      const double vec2_x = comp_2_y * dz - comp_2_z * dy;
      const double vec2_y = comp_2_z * dx - comp_2_x * dz;
      const double vec2_z = comp_2_x * dy - comp_2_y * dx;

      const double prefactor = (-9. * sigma * mt * mt * big_a * big_a) / (2. * mu_ij * (d2 * d2 * d2 * d2 * d2));

      tot_force.x += (prefactor * (vec1_x + vec2_x));
      tot_force.y += (prefactor * (vec1_y + vec2_y));
      tot_force.z += (prefactor * (vec1_z + vec2_z));
    }
  }

  return tot_force;
}

static void rebx_spin_orbit_accelerations(struct reb_particle* source, struct reb_particle* target, const double G, const double k2, const double sigma, const double sx, const double sy, const double sz){

    // Input params all associated with source
    const double ms = source->m;
    const double mt = target->m;
    const double mtot = ms + mt;

    // check if ODE is set here
    struct reb_vec3d tot_force = rebx_calculate_spin_orbit_accelerations(source, target, G, k2, sigma, sx, sy, sz);

    target->ax -= ((ms / mtot) * tot_force.x);
    target->ay -= ((ms / mtot) * tot_force.y);
    target->az -= ((ms / mtot) * tot_force.z);

    source->ax += ((mt / mtot) * tot_force.x);
    source->ay += ((mt / mtot) * tot_force.y);
    source->az += ((mt / mtot) * tot_force.z);

}

static void rebx_spin_derivatives(struct reb_ode* const ode, double* const yDot, const double* const y, const double t){
    struct reb_simulation* sim = ode->ref;
    struct rebx_extras* const rebx = sim->extras;
    unsigned int Nspins = 0;
    const int N_real = sim->N - sim->N_var;
    for (int i=0; i<N_real; i++){
        struct reb_particle* pi = &sim->particles[i]; // target particle
        const double* k2 = rebx_get_param(rebx, pi->ap, "k2"); // This is slow
        const double* sigma = rebx_get_param(rebx, pi->ap, "sigma");
        const double* moi = rebx_get_param(rebx, pi->ap, "moi");

        // Particle MUST have k2 and moment of inertia to feel effects
        if (k2 != NULL && moi != NULL){

          // Possible to disregard sigma. Check for that here.
          double sigma_in = 0.0;
          if (sigma != NULL){
            sigma_in = *sigma;
          }
	       // Set initial spin accelerations to 0
          yDot[3*Nspins] = 0;
          yDot[3*Nspins + 1] = 0;
          yDot[3*Nspins + 2] = 0;

          const double sx = y[3*Nspins];
          const double sy = y[3*Nspins + 1];
          const double sz = y[3*Nspins + 2];

          for (int j=0; j<N_real; j++){
            if (i != j){
                struct reb_particle* pj = &sim->particles[j];

                // di - dj
                const double dx = pi->x - pj->x;
                const double dy = pi->y - pj->y;
                const double dz = pi->z - pj->z;

                const double mi = pi->m;
                const double mj = pj->m;
                const double mu_ij = (mi * mj) / (mi + mj);

                struct reb_vec3d tf = rebx_calculate_spin_orbit_accelerations(pi, pj, sim->G, *k2, sigma_in, sx, sy, sz);
                yDot[3*Nspins] += ((dy * tf.z - dz * tf.y) * (-mu_ij / *moi));
                yDot[3*Nspins + 1] += ((dz * tf.x - dx * tf.z) * (-mu_ij / *moi));
                yDot[3*Nspins + 2] += ((dx * tf.y - dy * tf.x) * (-mu_ij / *moi));
            }
          }
          Nspins += 1;
      }
    }
    if (ode->length != Nspins*3){
        reb_error(sim, "rebx_spin ODE is not of the expected length.\n");
        exit(1);
    }
}

static void rebx_spin_sync_pre(struct reb_ode* const ode, const double* const y0){
    struct reb_simulation* sim = ode->ref;
    struct rebx_extras* const rebx = sim->extras;
    unsigned int Nspins = 0;
    const int N_real = sim->N - sim->N_var;
    for (int i=0; i<N_real; i++){
        struct reb_particle* p = &sim->particles[i];
        const double* k2 = rebx_get_param(rebx, p->ap, "k2");
        if (k2 != NULL){
            const double* sx = rebx_get_param(rebx, p->ap, "sx");
            const double* sy = rebx_get_param(rebx, p->ap, "sy");
            const double* sz = rebx_get_param(rebx, p->ap, "sz");
            ode->y[3*Nspins] = *sx;
            ode->y[3*Nspins+1] = *sy;
            ode->y[3*Nspins+2] = *sz;
            Nspins += 1;
        }
    }

    if (ode->length != Nspins*3){
        reb_error(sim, "rebx_spin ODE is not of the expected length.\n");
        exit(1);
    }
}

static void rebx_spin_sync_post(struct reb_ode* const ode, const double* const y0){
    struct reb_simulation* sim = ode->ref;
    struct rebx_extras* const rebx = sim->extras;
    unsigned int Nspins = 0;
    const int N_real = sim->N - sim->N_var;
    for (int i=0; i<N_real; i++){
        struct reb_particle* p = &sim->particles[i];
        const double* k2 = rebx_get_param(rebx, p->ap, "k2");
        if (k2 != NULL){
            rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "sx", y0[3*Nspins]);
            rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "sy", y0[3*Nspins+1]);
            rebx_set_param_double(rebx, (struct rebx_node**)&p->ap, "sz", y0[3*Nspins+2]);
            Nspins += 1;
        }
    }
    if (ode->length != Nspins*3){
        reb_error(sim, "rebx_spin ODE is not of the expected length.\n");
        exit(0);
    }
}

void rebx_spin_initialize_ode(struct rebx_extras* const rebx, struct rebx_force* const effect){
    struct reb_simulation* sim = rebx->sim;
    unsigned int Nspins = 0;
    const int N_real = sim->N - sim->N_var;
    for (int i=0; i<N_real; i++){
        struct reb_particle* p = &sim->particles[i];
        // Only track spin if particle has moment of inertia and valid spin axis set
        double* moi = rebx_get_param(rebx, p->ap, "moi");
        double* sx = rebx_get_param(rebx, p->ap, "sx");
        double* sy = rebx_get_param(rebx, p->ap, "sy");
        double* sz = rebx_get_param(rebx, p->ap, "sz");
        if (moi != NULL && sx != NULL && sy != NULL && sz != NULL){
            Nspins += 1;
        }
    }

    if (Nspins > 0){
        struct reb_ode* spin_ode = reb_create_ode(sim, Nspins*3);
        spin_ode->ref = sim;
        spin_ode->derivatives = rebx_spin_derivatives;
        spin_ode->pre_timestep = rebx_spin_sync_pre;
        spin_ode->post_timestep = rebx_spin_sync_post;
        rebx_set_param_pointer(rebx, &effect->ap, "ode", spin_ode);
    }
}

void rebx_tides_spin(struct reb_simulation* const sim, struct rebx_force* const effect, struct reb_particle* const particles, const int N){
    struct rebx_extras* const rebx = sim->extras;
    const double G = sim->G;

    // check if ODE is initialized
    struct reb_ode** ode = sim->odes;
    if (ode == NULL){
      reb_warning(sim, "Spin axes are not being evolved. Call rebx_spin_initialize_ode to evolve\n");
    }

    for (int i=0; i<N; i++){
        struct reb_particle* source = &particles[i];
        // Particle must have a k2 set, otherwise we treat this body as a point particle
        const double* k2 = rebx_get_param(rebx, source->ap, "k2");
        const double* sigma = rebx_get_param(rebx, source->ap, "sigma");
        const double* sx = rebx_get_param(rebx, source->ap, "sx");
        const double* sy = rebx_get_param(rebx, source->ap, "sy");
        const double* sz = rebx_get_param(rebx, source->ap, "sz");

        // Particle needs all three spin components and k2 to feel additional forces
        if (sx != NULL && sy != NULL && sz != NULL && k2 != NULL){
          // Possible to disregard sigma. Check for that here.
          double sigma_in = 0.0;
          if (sigma != NULL){
            sigma_in = *sigma;
          }

          for (int j=0; j<N; j++){
              if (i==j){
                  continue;
              }
              struct reb_particle* target = &particles[j]; // j raises tides on i
              if (source->m == 0 || target->m == 0){
                  continue;
              }

              rebx_spin_orbit_accelerations(source, target, G, *k2, sigma_in, *sx, *sy, *sz);
          }
      }
    }
}

// Calculate potential of conservative piece of tidal interaction
static double rebx_calculate_spin_potential(struct reb_particle* source, struct reb_particle* target, const double G, const double k2){
    const double ms = source->m;
    const double mt = target->m;
    const double Rt = target->r;

    const double mratio = ms/mt; // have already checked for 0 and inf
    const double fac = mratio*k2*Rt*Rt*Rt*Rt*Rt;

    const double dx = target->x - source->x;
    const double dy = target->y - source->y;
    const double dz = target->z - source->z;
    const double dr2 = dx*dx + dy*dy + dz*dz;

    return -1./2.*G*ms*mt/(dr2*dr2*dr2)*fac;
}

double rebx_spin_potential(struct rebx_extras* const rebx){
    if (rebx->sim == NULL){
        rebx_error(rebx, ""); // rebx_error gives meaningful err
        return 0;
    }
    struct reb_simulation* const sim = rebx->sim;
    const int N_real = sim->N - sim->N_var;
    struct reb_particle* const particles = sim->particles;
    const double G = sim->G;
    double H=0.;

    for (int i=0; i<N_real; i++){
        struct reb_particle* source = &particles[i];
        // Particle must have a k2 and sigma set, otherwise we treat this body as a point particle
        const double* k2 = rebx_get_param(rebx, source->ap, "k2");
        const double* sigma = rebx_get_param(rebx, source->ap, "sigma");
        if (k2 == NULL || sigma == NULL || source->r == 0 || source->m == 0){
            continue;
        }
        for (int j=0; j<N_real; j++){
            if (i==j){
                continue;
            }
            struct reb_particle* target = &particles[j]; // planet raising the tides on the star
            if (source->m == 0 || target->m == 0){
                continue;
            }
            H += rebx_calculate_spin_potential(source, target, G, *k2);
        }
    }

    return H;
}

// TLu 11/8/22 HELPER FUNCS
double rebx_tides_calc_sigma_from_tau(struct rebx_extras* rebx, struct reb_particle* body, const double tau){
  struct reb_simulation* const sim = rebx->sim;
  const double G = sim->G;
  const double* k2 = rebx_get_param(rebx, body->ap, "k2");
  const double r = body->r;

  if (k2 != NULL && r != 0.0){
    return 4 * tau * G / (3 * r * r * r * r * r * (*k2));
  }

  else{
    rebx_error(rebx, "Could not set sigma because Love number and/or physical radius was not set for this particle\n");
    return 0;
  }
}

double rebx_tides_calc_sigma_from_Q(struct rebx_extras* rebx, struct reb_particle* body, struct reb_particle* primary, const double Q){
  struct reb_simulation* const sim = rebx->sim;
  const double G = sim->G;
  struct reb_orbit orb = reb_tools_particle_to_orbit(G, *body, *primary);
  const double r = body->r;
  const double n = orb.n;
  const double* k2 = rebx_get_param(rebx, body->ap, "k2");
  if (k2 != NULL && r != 0.0){
      return 2. * G / (3. * Q * r * r * r * r * r * (*k2) * (n));
  }
  else{
    rebx_error(rebx, "Could not calculate sigma because Love number and/or physical radius was not set for this particle\n");
    return 0;
  }
}
