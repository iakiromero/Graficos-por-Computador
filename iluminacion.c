#include "iluminacion.h"

light_t g_lights[4];
int g_active_light = 0;
int g_interp_mode = 0;

static double clamp01(double a){ return a<0?0:(a>1?1:a); }

static vec3 v3(double x,double y,double z){ vec3 r={x,y,z}; return r; }
static vec3 add(vec3 a, vec3 b){ return v3(a.x+b.x,a.y+b.y,a.z+b.z); }
static vec3 sub(vec3 a, vec3 b){ return v3(a.x-b.x,a.y-b.y,a.z-b.z); }
static vec3 mul(vec3 a, vec3 b){ return v3(a.x*b.x,a.y*b.y,a.z*b.z); }
static vec3 scale(vec3 a, double s){ return v3(a.x*s,a.y*s,a.z*s); }
static double dot(vec3 a, vec3 b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
static double len(vec3 a){ return sqrt(dot(a,a)); }
static vec3 norm(vec3 a){ double l=len(a); if(l<1e-12) return v3(0,0,0); return v3(a.x/l,a.y/l,a.z/l); }

static vec3 mxv3(const double M[16], vec3 v) {
    // usa tu convención row-major con traslación en [3],[7],[11]
    return v3(
        M[0]*v.x + M[1]*v.y + M[2]*v.z,
        M[4]*v.x + M[5]*v.y + M[6]*v.z,
        M[8]*v.x + M[9]*v.y + M[10]*v.z
    );
}
static vec3 mxp3(const double M[16], vec3 p) {
    return v3(
        M[0]*p.x + M[1]*p.y + M[2]*p.z + M[3],
        M[4]*p.x + M[5]*p.y + M[6]*p.z + M[7],
        M[8]*p.x + M[9]*p.y + M[10]*p.z + M[11]
    );
}

void lights_init_defaults(void)
{
    // 0: Sol (direccional)
    g_lights[0].enabled = 1;
    g_lights[0].type = L_DIR;
    g_lights[0].color = v3(1,1,1);
    g_lights[0].dir_world = norm(v3(-1,-1,-1)); // hacia donde ilumina

    // 1: Bombilla (puntual)
    g_lights[1].enabled = 1;
    g_lights[1].type = L_POINT;
    g_lights[1].color = v3(1,1,1);
    g_lights[1].pos_world = v3(0,2,0);
    g_lights[1].kc=1.0; g_lights[1].kl=0.0; g_lights[1].kq=0.0;

    // 2: Foco objeto (spot) (pos/dir se actualiza desde el objeto)
    g_lights[2].enabled = 1;
    g_lights[2].type = L_SPOT;
    g_lights[2].color = v3(1,1,1);
    g_lights[2].pos_world = v3(0,0,0);
    g_lights[2].dir_world = v3(0,0,-1);
    g_lights[2].cosCutoff = cos(25.0*M_PI/180.0);
    g_lights[2].exponent = 20.0;
    g_lights[2].kc=1.0; g_lights[2].kl=0.0; g_lights[2].kq=0.0;

    // 3: Foco cámara (spot) (pos/dir se actualiza desde la cámara)
    g_lights[3].enabled = 1;
    g_lights[3].type = L_SPOT;
    g_lights[3].color = v3(1,1,1);
    g_lights[3].pos_world = v3(0,0,0);
    g_lights[3].dir_world = v3(0,0,-1);
    g_lights[3].cosCutoff = cos(20.0*M_PI/180.0);
    g_lights[3].exponent = 30.0;
    g_lights[3].kc=1.0; g_lights[3].kl=0.0; g_lights[3].kq=0.0;
}

void lights_update_view(const double V[16], vec3 cam_forward_view)
{
    // Transformamos pos/dir world -> view
    for(int i=0;i<4;i++){
        if(g_lights[i].type != L_DIR){
            g_lights[i].pos_view = mxp3(V, g_lights[i].pos_world);
        }
        if(g_lights[i].type == L_DIR || g_lights[i].type == L_SPOT){
            g_lights[i].dir_view = norm(mxv3(V, g_lights[i].dir_world));
        }
    }
    (void)cam_forward_view;
}

vec3 shade_point(point3d Pview, vec3 Nview, material_t mat)
{
    vec3 N = norm(v3(Nview.x,Nview.y,Nview.z));
    // cámara en view = origen
    vec3 V = norm(v3(-Pview.x, -Pview.y, -Pview.z));

    vec3 out = v3(0,0,0);

    for(int i=0;i<4;i++){
        if(!g_lights[i].enabled) continue;

        vec3 L;
        double att = 1.0;
        double spot = 1.0;

        if(g_lights[i].type == L_DIR){
            // si dir_view apunta hacia donde ilumina, desde el punto hacia la luz es -dir
            L = norm(scale(g_lights[i].dir_view, -1.0));
        } else {
            vec3 Pv = v3(Pview.x,Pview.y,Pview.z);
            vec3 toL = sub(g_lights[i].pos_view, Pv);
            double d = len(toL);
            L = (d<1e-9) ? v3(0,0,0) : scale(toL, 1.0/d);
            att = 1.0 / (g_lights[i].kc + g_lights[i].kl*d + g_lights[i].kq*d*d);
        }

        if(g_lights[i].type == L_SPOT){
            // comparamos ángulo entre el eje del foco (dir_view) y el vector desde foco a punto
            vec3 Pv = v3(Pview.x,Pview.y,Pview.z);
            vec3 fromLightToP = norm(sub(Pv, g_lights[i].pos_view));
            double c = dot(norm(g_lights[i].dir_view), fromLightToP);
            if(c < g_lights[i].cosCutoff) spot = 0.0;
            else spot = pow(c, g_lights[i].exponent);
        }

        double ndotl = dot(N,L);
        if(ndotl < 0) ndotl = 0;

        // Blinn-Phong
        vec3 H = norm(add(L,V));
        double ndoth = dot(N,H);
        if(ndoth < 0) ndoth = 0;
        double spec = pow(ndoth, mat.shininess);

        vec3 Ia = mul(mat.Ka, g_lights[i].color);
        vec3 Id = scale(mul(mat.Kd, g_lights[i].color), ndotl);
        vec3 Is = scale(mul(mat.Ks, g_lights[i].color), spec);

        vec3 contrib = add(Ia, scale(add(Id,Is), att*spot));
        out = add(out, contrib);
    }

    out.x = clamp01(out.x);
    out.y = clamp01(out.y);
    out.z = clamp01(out.z);
    return out;
}
