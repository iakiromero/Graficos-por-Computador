#ifndef ILUMINACION_H
#define ILUMINACION_H

#include <math.h>

typedef struct { double x,y,z; } vec3;
typedef struct { double x,y,z; } point3d;

typedef enum { L_DIR=0, L_POINT=1, L_SPOT=2 } light_type;

typedef struct {
    int enabled;
    light_type type;
    vec3 color;       // [0..1]
    vec3 pos_world;   // point/spot
    vec3 dir_world;   // dir/spot (dirección "hacia donde ilumina")
    // precomputado en view
    vec3 pos_view;
    vec3 dir_view;

    double kc, kl, kq;      // atenuación
    double cosCutoff;       // spot
    double exponent;        // spot
} light_t;

typedef struct {
    vec3 Ka, Kd, Ks;
    double shininess;
} material_t;

extern light_t g_lights[4];
extern int g_active_light;   // TAB
extern int g_interp_mode;    // i: 0 flat, 1 gouraud

void lights_init_defaults(void);
void lights_update_view(const double V[16], vec3 cam_forward_view); // V = inverse_rigid(M_kam)
vec3 shade_point(point3d Pview, vec3 Nview, material_t mat);

#endif
