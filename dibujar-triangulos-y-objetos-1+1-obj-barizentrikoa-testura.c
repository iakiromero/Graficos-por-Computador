//	Program developed by
//	
//	Informatika Fakultatea
//	Euskal Herriko Unibertsitatea
//	http://www.ehu.eus/if
//
// to compile it: gcc dibujar-triangulos-y-objetos.c -lGL -lGLU -lglut
//
// 
//


#include <GL/glut.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
//#include "cargar-triangulo.h"
#include "obj.h"
#include "iluminacion.h"


// testuraren informazioa
// información de textura

extern int load_ppm(char *file, unsigned char **bufferptr, int *dimxptr, int * dimyptr);
unsigned char *bufferra;
int dimx,dimy,dimentsioa;

int indexx;
//hiruki *triangulosptr;
object3d *foptr;
object3d *sel_ptr;
int denak;
int lineak;
int objektuak;
int kamera;
char aldaketa;
int ald_lokala;
int objektuaren_ikuspegia;
int paralelo;
char fitxiz[100];
int atzearpegiakmarraztu;
double M_kam[16];
static int g_light_mode = 0;
#define UNDO_MAX 256

typedef enum { UNDO_OBJ=0, UNDO_CAM=1, UNDO_LIGHT=2 } undo_kind;

typedef struct {
    undo_kind kind;
    object3d *obj;   // si kind==UNDO_OBJ
    int light_idx;   // si kind==UNDO_LIGHT
    double M_prev[16]; // para objeto o cámara
    light_t  L_prev;     // para luz
} undo_entry;

static undo_entry undo_stack[UNDO_MAX];
static int undo_top = 0;
material_t g_mat_default ={
    .Ka = {0.10, 0.10, 0.10}, 
    .Kd = {0.70, 0.70, 0.70}, 
    .Ks = {0.20, 0.20, 0.20}, 
    .shininess = 32.0
};
extern int g_interp_mode;

void mxm(double *m, double*m1, double *m2)
{
    double r[16];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            r[i*4 + j] = 0.0;
            for (int k = 0; k < 4; ++k) {
                r[i*4 + j] += m1[i*4 + k] * m2[k*4 + j];
            }
        }
    }
    for (int i = 0; i < 16; ++i) m[i] = r[i];
}


// TODO
void kamerari_aldaketa_sartu_ezk(double *m)
{
    double r[16];
    mxm(r, m, M_kam);                 // M_kam = m * M_kam
    for (int i=0;i<16;i++) M_kam[i]=r[i];
}

// TODO
void kamerari_aldaketa_sartu_esk(double m[16])
{
     double r[16];
    mxm(r, M_kam, m);                 // M_kam = M_kam * m
    for (int i=0;i<16;i++) M_kam[i]=r[i];
}

// TODO
void objektuari_aldaketa_sartu_ezk(double m[16])
{
    if (!sel_ptr || !sel_ptr->mptr) return;
    double r[16];
    mxm(r, m, sel_ptr->mptr->m);      // M = m * M   (global)
    for (int i=0;i<16;i++) sel_ptr->mptr->m[i]=r[i];
}

// TODO
void objektuari_aldaketa_sartu_esk(double m[16])
{
    if (!sel_ptr || !sel_ptr->mptr) return;
    double r[16];
    mxm(r, sel_ptr->mptr->m, m);      // M = M * m   (local)
    for (int i=0;i<16;i++) sel_ptr->mptr->m[i]=r[i];

}



// TODO given u,v get the color pointer
unsigned char *color_textura(float u, float v)
{
int indx,indy;
char * lag;

static unsigned char fallback[3] = {255, 255, 255};
//printf("texturan...%x\n",bufferra);
//TODO get the desplacement for indx and indy
// negative values?
// values greater than 1?
  // Controlar bordes: clamp entre 0 y 1
if(bufferra == 0 || dimx ==0 || dimy ==0)
    {
    // no hay textura
    return fallback;
    }
if (u < 0.0f) u = 0.0f;
if (u > 1.0f) u = 1.0f;
if (v < 0.0f) v = 0.0f;
if (v > 1.0f) v = 1.0f;

// Pasar de coordenadas normalizadas a coordenadas de la imagen
indx = (int)(u * (dimx - 1));
indy = (int)(v * (dimy - 1));

// Importante: muchas veces v=0 está abajo y v=1 arriba,
// así que podemos invertir la Y si la textura sale al revés
    if (indx < 0) indx = 0;
    if (indx >= dimx) indx = dimx - 1;
    if (indy < 0) indy = 0;
    if (indy >= dimy) indy = dimy - 1;

indy = dimy - 1 - indy;
lag = (unsigned char *)bufferra;
//printf("irtetera %x\n",lag[indy*dimx+indx]);
return(lag+3*(indy*dimx+indx));
}



void print_matrizea16(double *m)
{
int i;

for (i = 0;i<4;i++)
   printf("%lf, %lf, %lf, %lf\n",m[i*4],m[i*4+1],m[i*4+2], m[i*4+3]);
}

static void ident16(double M[16]) {
    for (int i=0;i<16;i++) M[i]=0.0;
    M[0]=M[5]=M[10]=M[15]=1.0;
}

static void invert_rigid(double inv[16], const double M[16])
{
    inv[0] = M[0]; inv[1] = M[4]; inv[2] = M[8];  inv[3] = 0.0;
    inv[4] = M[1]; inv[5] = M[5]; inv[6] = M[9];  inv[7] = 0.0;
    inv[8] = M[2]; inv[9] = M[6]; inv[10]= M[10]; inv[11]=0.0;
    inv[12]=0.0;   inv[13]=0.0;   inv[14]=0.0;    inv[15]=1.0;

    double tx = M[3], ty = M[7], tz = M[11];
    inv[3]  = -(inv[0]*tx + inv[1]*ty + inv[2]*tz);
    inv[7]  = -(inv[4]*tx + inv[5]*ty + inv[6]*tz);
    inv[11] = -(inv[8]*tx + inv[9]*ty + inv[10]*tz);
}



// TODO  res = m * v 
// v bektoreari m matrizea bidertu eta res erakusleak adierazten duen bektorean jaso. 
// v bektorearen eta res emaitzaren laugarren osagaia 0 dela suposatzen du.
void mxv(double *res, double *m, double *v)
{
    res[0] = (float)(m[0]*v[0]  + (float)m[1]*v[1]  + (float)m[2]*v[2]);
    res[1] = (float)(m[4]*v[0]  + (float)m[5]*v[1]  + (float)m[6]*v[2]);
    res[2] = (float)(m[8]*v[0]  + (float)m[9]*v[1]  + (float)m[10]*v[2]);
}

static void ident(double M[16]) {
    for (int i = 0; i < 16; ++i) M[i] = 0.0;
    M[0]=M[5]=M[10]=M[15]=1.0;
}

static void trans_mat(double M[16], double tx, double ty, double tz) {
    ident(M);
    M[3]  = tx;
    M[7]  = ty;
    M[11] = tz;
}

static void rotX_mat(double M[16], double ang_deg) {
    ident(M);
    double a = ang_deg * M_PI / 180.0;
    double c = cos(a), s = sin(a);
    M[5]=c;  M[6]=-s;
    M[9]=s;  M[10]=c;
}

static void rotY_mat(double M[16], double ang_deg) {
    ident(M);
    double a = ang_deg * M_PI / 180.0;
    double c = cos(a), s = sin(a);
    M[0]=c;  M[2]=s;
    M[8]=-s; M[10]=c;
}

static void rotZ_mat(double M[16], double ang_deg) {
    ident(M);
    double a = ang_deg * M_PI / 180.0;
    double c = cos(a), s = sin(a);
    M[0]=c;  M[1]=-s;
    M[4]=s;  M[5]=c;
}




// TODO  pptr = m * p 
// ppuntuari m matrizea bidertu eta pptr erakulseak adierazten duen puntuan jaso. 
// p puntuaren laugarren osagaia 1 dela suposatzen du.
// matrizearen laugarren lerroaren arabera emaitzaren laugarren osagaia, w, ez bada 1, orduan bere baliokidea itzuli behar du: x/w, y/w eta z/w
void mxp(point3 *pptr, double m[16], point3 p)
{
    double x = m[0]*p.x + m[1]*p.y + m[2]*p.z + m[3];
    double y = m[4]*p.x + m[5]*p.y + m[6]*p.z + m[7];
    double z = m[8]*p.x + m[9]*p.y + m[10]*p.z + m[11];
    double w = m[12]*p.x + m[13]*p.y + m[14]*p.z + m[15];

    if (fabs(w) > 1e-12 && fabs(w - 1.0) > 1e-12) {
        x /= w; y /= w; z /= w;
    }
    pptr->x = x; pptr->y = y; pptr->z = z;
}


// TODO objektuaren erpinek eta bektore normalek kameraren erreferentzi-sisteman dituzten koordenatuak lortu

void kam_ikuspegia_lortu(object3d *optr)
{
    printf("FUnciona1");
    if(!optr) return;
    if(optr->num_vertices <=0|| optr->vertex_table ==NULL ) return;
    if(optr->num_faces <=0|| optr->face_table == NULL)return;
    int i;
    printf("FUnciona1");
    double V[16];
    invert_rigid(V, M_kam);

    double Mobj[16];
    ident16(Mobj);
    if (optr->mptr != 0)
        for (int k=0;k<16;k++) Mobj[k] = optr->mptr->m[k];

    double MV[16];
    mxm(MV, V, Mobj);

    double ortho_scale = 1.0;
    double nearZ = 0.1, farZ = 50.0;

    double fov_deg = 60.0;
    double f = 1.0 / tan((fov_deg * M_PI / 180.0) * 0.5);

    for (i = 0; i < optr->num_vertices; i++)
    {
        point3 pcam;
        mxp(&pcam, MV, optr->vertex_table[i].coord);
        optr->vertex_table[i].camcoord = pcam;


        double nx = optr->vertex_table[i].N[0]; 
        double ny = optr->vertex_table[i].N[1]; 
        double nz = optr->vertex_table[i].N[2]; 

        double nxc = MV[0]*nx + MV[1]* ny + MV[2]*nz;  
        double nyc = MV[4]*nx + MV[5]* ny + MV[6]*nz;   
        double nzc = MV[8]*nx + MV[9]* ny + MV[10]*nz;   

        double l = sqrt(nxc*nxc + nyc*nyc + nzc*nzc);
        if(l > 1e-12){ nxc/=l; nyc/=l; nzc/=l;}  

        optr->vertex_table[i].Ncam[0] =(float)(nxc);
        optr->vertex_table[i].Ncam[1] = (float)(nyc);
        optr->vertex_table[i].Ncam[2] =(float)(nzc);

        point3 pndc = pcam;

        if (paralelo) {
            pndc.x = pcam.x / ortho_scale;
            pndc.y = pcam.y / ortho_scale;
        } else {
            double z = -pcam.z;
            if (z < 1e-6) z = 1e-6;
            pndc.x = (pcam.x * f) / z;
            pndc.y = (pcam.y * f) / z;
        }

        double zpos = -pcam.z;
        double zdepth = (zpos - nearZ) / (farZ - nearZ);
        if (zdepth < 0.0) zdepth = 0.0;
        if (zdepth > 1.0) zdepth = 1.0;
        pndc.z = zdepth;

        optr->vertex_table[i].proedcoord = pndc;
    }
    printf("FUnciona2");
    for (i = 0; i < optr->num_faces; i++) {
        point3 centro = {0,0,0};
        face *f = &optr->face_table[i];

        for (int j = 0; j < f->num_vertices; j++){
            int v_idx = f->vertex_ind_table[j];
            centro.x += optr->vertex_table[v_idx].coord.x;  
            centro.y += optr->vertex_table[v_idx].coord.y; 
            centro.z += optr->vertex_table[v_idx].coord.z; 
        }   
        centro.x /= f->num_vertices;
        centro.y /= f->num_vertices;
        centro.z /= f->num_vertices;

        point3 pcam;
        mxp(&pcam, MV, centro);
        //optr->face_table[i].camcoord = pcam; 

        double fx = optr->face_table[i].N[0]; 
        double fy = optr->face_table[i].N[1]; 
        double fz = optr->face_table[i].N[2]; 
        printf("FUnciona3");
        double fxc = MV[0]*fx + MV[1]* fy + MV[2]*fz;  
        double fyc = MV[4]*fx + MV[5]* fy + MV[6]*fz;   
        double fzc = MV[8]*fx + MV[9]* fy + MV[10]*fz;   
        printf("FUnciona4");
        double l = sqrt(fxc*fxc + fyc*fyc + fzc*fzc);
        if(l > 1e-12){ fxc/=l; fyc/=l; fzc/=l;}  
        printf("FUnciona5");
        optr->face_table[i].Ncam[0] =(float)(fxc);
        optr->face_table[i].Ncam[1] = (float)(fyc);
        optr->face_table[i].Ncam[2] =(float)(fzc);
    }
}




void modelview_lortu(double *m1, double *m2)
{
}

void mesa_lortu(double* M) 
{
}

static unsigned char to_u8(double x01){
    if(x01 < 0.0) x01 = 0.0;
    if(x01 > 1.0) x01 = 1.0;

    return (unsigned char)(x01 * 255.0 +0.5);
} 

void argien_kalkulua_egin(object3d *optr, int ti)
{
    if(!optr)return;
    if(ti <0|| ti >= optr->num_faces) return;

    face *fc = &optr->face_table[ti];
    if(!fc|| fc->num_vertices < 3 ) return;

    material_t mat = g_mat_default;
    mat.Ka = (vec3){optr->ka.r , optr->ka.g, optr->ka.b};
    mat.Kd = (vec3){optr->kd.r , optr->kd.g, optr->kd.b};
    mat.Ks = (vec3){optr->ks.r , optr->ks.g, optr->ks.b};
    mat.shininess = (optr->ns > 0) ? (double)optr->ns : g_mat_default.shininess;

    if(mat.Kd.x == 0 && mat.Kd.y == 0 && mat.Kd.z == 0) mat = g_mat_default;

    if(g_interp_mode == 0)
   {

    double cx = 0, cy =0, cz = 0;
    for (int k=0; k< fc->num_vertices; k++){
        int vi = fc->vertex_ind_table[k];
        point3 pc = optr->vertex_table[vi].camcoord;
        cx += pc.x;
        cy += pc.y;
        cz += pc.z;  
    } 
    double inv = 1.0 / (double)fc->num_vertices;
    point3d Pc ={ cx*inv, cy*inv, cz*inv};

    vec3 Nf ={ fc->Ncam[0], fc->Ncam[1], fc->Ncam[2]};

    vec3 I = shade_point(Pc, Nf, mat);   
    
    fc->rgb[0] = to_u8(I.x); 
    fc->rgb[1] = to_u8(I.y); 
    fc->rgb[2] = to_u8(I.z); 

    for (int k=0; k< fc->num_vertices; k++){
        int vi = fc->vertex_ind_table[k];
        optr->vertex_table[vi].rgb[0] = fc->rgb[0];
        optr->vertex_table[vi].rgb[1] = fc->rgb[1];
        optr->vertex_table[vi].rgb[2] = fc->rgb[2];     
    } 
   }   
   else{
    for (int k=0; k< fc->num_vertices; k++){
        int vi = fc->vertex_ind_table[k];
        point3 pc = optr->vertex_table[vi].camcoord;
        point3d Pv ={pc.x, pc.y, pc.z};

        vec3 Nv ={ optr->vertex_table[vi].Ncam[0],  
                    optr->vertex_table[vi].Ncam[1],
                    optr->vertex_table[vi].Ncam[2]
        }; 
        vec3 I = shade_point(Pv, Nv, mat);

        optr->vertex_table[vi].rgb[0] = to_u8(I.x);
        optr->vertex_table[vi].rgb[1] = to_u8(I.y);
        optr->vertex_table[vi].rgb[2] = to_u8(I.z);
    } 
    int v0 = fc->vertex_ind_table[0];
    fc->rgb[0] = optr->vertex_table[v0].rgb[0];  
    fc->rgb[1] = optr->vertex_table[v0].rgb[1];
    fc->rgb[2] = optr->vertex_table[v0].rgb[2];  
   } 

}



/* ti is the face index
** i1 if the face has more than 3 vertices there will be more than 1 triangle, 
** i1 indicates the ith triangle of the face:
**  a.- The first vertex and the next two vertices form the first triangle, so
        0, 1, 2 are the indices of the vertices and i1 will be 1
    b.- the first vertex and the third and fourth vertices form the next triangle. So
        0, 2, 3 are the indices and consequently 1i will be 2 (second triangle of the 
        face)
    c.- 0, 3, 4 form the next triangle and so i1= 3...
** atzeaurpegiada indicates that the face is a backface. Depending of the state of the aplication tha face will be drawn in red or it will not be drawn
*/

void barycentric(float x, float y, point3 A, point3 B, point3 C,
                 float *alpha, float *beta, float *gamma)
{
    float detT = (B.y - C.y)*(A.x - C.x) + (C.x - B.x)*(A.y - C.y);

    *alpha = ((B.y - C.y)*(x - C.x) + (C.x - B.x)*(y - C.y)) / detT;
    *beta  = ((C.y - A.y)*(x - C.x) + (A.x - C.x)*(y - C.y)) / detT;
    *gamma = 1.0f - *alpha - *beta;
}
static void normalize3(double n[3]) {
    double l = sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
    if (l < 1e-12) return;
    n[0]/=l; n[1]/=l; n[2]/=l;
}

static void cross3(double a[3], double b[3], double r[3]) {
    r[0] = a[1]*b[2] - a[2]*b[1];
    r[1] = a[2]*b[0] - a[0]*b[2];
    r[2] = a[0]*b[1] - a[1]*b[0];
}

void obj_normalak_kalkulatu(object3d *optr)
{
    if (!optr) return;

    // 1) inicializa normales de vértice a 0
    for (int i=0;i<optr->num_vertices;i++) {
        optr->vertex_table[i].N[0]=0;
        optr->vertex_table[i].N[1]=0;
        optr->vertex_table[i].N[2]=0;
        optr->vertex_table[i].num_faces = 0;
    }

    // 2) normal por cara + acumulación a vértices
    for (int f=0; f<optr->num_faces; f++) {
        face *fc = &optr->face_table[f];
        if (fc->num_vertices < 3) continue;

        int i0 = fc->vertex_ind_table[0];
        int i1 = fc->vertex_ind_table[1];
        int i2 = fc->vertex_ind_table[2];

        point3 p0 = optr->vertex_table[i0].coord;
        point3 p1 = optr->vertex_table[i1].coord;
        point3 p2 = optr->vertex_table[i2].coord;

        double e1[3] = {p1.x-p0.x, p1.y-p0.y, p1.z-p0.z};
        double e2[3] = {p2.x-p0.x, p2.y-p0.y, p2.z-p0.z};
        double n[3];
        cross3(e1,e2,n);
        normalize3(n);

        fc->N[0]=n[0]; fc->N[1]=n[1]; fc->N[2]=n[2];

        // acumular a todos los vértices de la cara
        for (int k=0;k<fc->num_vertices;k++) {
            int vi = fc->vertex_ind_table[k];
            optr->vertex_table[vi].N[0] += n[0];
            optr->vertex_table[vi].N[1] += n[1];
            optr->vertex_table[vi].N[2] += n[2];
            optr->vertex_table[vi].num_faces += 1;
        }
    }

    // 3) normal final por vértice = normalizada
    for (int i=0;i<optr->num_vertices;i++) {
        normalize3(optr->vertex_table[i].N);
    }

    // 4) materiales por defecto si vienen a 0
    // (si tus objs ya traen ka/kd/ks/ns, puedes quitar esto)
    if (optr->ns == 0) optr->ns = 32;
    if (optr->kd.r==0 && optr->kd.g==0 && optr->kd.b==0) {
        optr->ka = (color3){0.10,0.10,0.10};
        optr->kd = (color3){0.70,0.70,0.70};
        optr->ks = (color3){0.20,0.20,0.20};
    }
}


void dibujar_triangulo(object3d *optr, int ti, int i1, int atzeaurpegiada)
{
point3 *pgoiptr, *pbeheptr, *perdiptr;
float x, y, z, u, v, nx, ny;
float luz, l12, l23, l13, d1v23, d2v13, d3v12;
float lerrotartea,pixeldist;
float alfa, beta, gamma;
float goieragina, beheeragina, erdieragina;
float *luzeptr,*erdiptr, *motzptr;
float  luzeluz, paraleloarenluzera;
int luzeanparalelokop, barnekop;
int lerrokop,i,j;
int ind0, ind1, ind2, indg, inde, indb;
point3 *p1ptr, *p2ptr, *p3ptr;
double *Nkam;
double aldaketa,baldaketa;
float x1,x2, y1;
unsigned char r,g,b;
unsigned char *colorv;
float xmin, xmax, ymin, ymax;
int pixeles_dibujados = 0;

    ind0 = optr->face_table[ti].vertex_ind_table[0];
    ind1 = optr->face_table[ti].vertex_ind_table[i1];
    ind2 = optr->face_table[ti].vertex_ind_table[i1+1];

    p1ptr = &(optr->vertex_table[ind0].proedcoord);
    p2ptr = &(optr->vertex_table[ind1].proedcoord);
    p3ptr = &(optr->vertex_table[ind2].proedcoord);

    r = optr->face_table[ti].rgb[0];
    g = optr->face_table[ti].rgb[1];
    b = optr->face_table[ti].rgb[2];

    // Comprobar si los tres puntos son colineales (en pantalla)
    float detT = (p2ptr->y - p3ptr->y)*(p1ptr->x - p3ptr->x) +
                 (p3ptr->x - p2ptr->x)*(p1ptr->y - p3ptr->y);
    if (fabs(detT) < 1e-6) {
        glBegin(GL_LINE_STRIP);
        glColor3ub(r,g,b);
        glVertex3f(p1ptr->x, p1ptr->y, p1ptr->z);
        glVertex3f(p2ptr->x, p2ptr->y, p2ptr->z);
        glVertex3f(p3ptr->x, p3ptr->y, p3ptr->z);
        glEnd();
        return;
    }

    // --- ILUMINACIÓN: calcular intensidades en view-space ---
    // puntos en cámara
    point3d P1v = { optr->vertex_table[ind0].camcoord.x,
                    optr->vertex_table[ind0].camcoord.y,
                    optr->vertex_table[ind0].camcoord.z };
    point3d P2v = { optr->vertex_table[ind1].camcoord.x,
                    optr->vertex_table[ind1].camcoord.y,
                    optr->vertex_table[ind1].camcoord.z };
    point3d P3v = { optr->vertex_table[ind2].camcoord.x,
                    optr->vertex_table[ind2].camcoord.y,
                    optr->vertex_table[ind2].camcoord.z };

    vec3 N1v = { optr->vertex_table[ind0].Ncam[0],
                 optr->vertex_table[ind0].Ncam[1],
                 optr->vertex_table[ind0].Ncam[2] };
    vec3 N2v = { optr->vertex_table[ind1].Ncam[0],
                 optr->vertex_table[ind1].Ncam[1],
                 optr->vertex_table[ind1].Ncam[2] };
    vec3 N3v = { optr->vertex_table[ind2].Ncam[0],
                 optr->vertex_table[ind2].Ncam[1],
                 optr->vertex_table[ind2].Ncam[2] };
    vec3 Nf = { optr->face_table[ti].Ncam[0],
                optr->face_table[ti].Ncam[1],
                optr->face_table[ti].Ncam[2] };
    material_t mat = g_mat_default;
    mat.Ka = (vec3){ optr->ka.r, optr->ka.g, optr->ka.b };
    mat.Kd = (vec3){ optr->kd.r, optr->kd.g, optr->kd.b };
    mat.Ks = (vec3){ optr->ks.r, optr->ks.g, optr->ks.b };
    mat.shininess = (optr->ns > 0) ? (double)optr->ns : g_mat_default.shininess;

    // Si el .obj no trae material y queda todo a 0, cae al default
    if (mat.Kd.x==0 && mat.Kd.y==0 && mat.Kd.z==0) {
        mat = g_mat_default;
    }
    vec3 Iflat = {1,1,1}, I1={1,1,1}, I2={1,1,1}, I3={1,1,1};

    if (!atzeaurpegiada) {
        if (g_interp_mode == 0) {
            point3d Pc = {(P1v.x+P2v.x+P3v.x)/3.0, (P1v.y+P2v.y+P3v.y)/3.0, (P1v.z+P2v.z+P3v.z)/3.0};
            Iflat = shade_point(Pc, Nf, mat);
        } else {
            I1 = shade_point(P1v, N1v, mat);
            I2 = shade_point(P2v, N2v, mat);
            I3 = shade_point(P3v, N3v, mat);
        }
    }

    // Dibujar los 3 vértices como puntos
    glBegin(GL_POINTS);
    if (!atzeaurpegiada) {
        if (optr->texturaduna) {
            colorv = color_textura(optr->vertex_table[ind0].u, optr->vertex_table[ind0].v);
            glColor3ub(colorv[0],colorv[1],colorv[2]);
        } else glColor3ub(r,g,b);
    }
    glVertex3f(p1ptr->x, p1ptr->y, p1ptr->z);

    if (!atzeaurpegiada) {
        if (optr->texturaduna) {
            colorv = color_textura(optr->vertex_table[ind1].u, optr->vertex_table[ind1].v);
            glColor3ub(colorv[0],colorv[1],colorv[2]);
        } else glColor3ub(r,g,b);
    }
    glVertex3f(p2ptr->x, p2ptr->y, p2ptr->z);

    if (!atzeaurpegiada) {
        if (optr->texturaduna) {
            colorv = color_textura(optr->vertex_table[ind2].u, optr->vertex_table[ind2].v);
            glColor3ub(colorv[0],colorv[1],colorv[2]);
        } else glColor3ub(r,g,b);
    }
    glVertex3f(p3ptr->x, p3ptr->y, p3ptr->z);
    glEnd();


    if (lineak == 1) 
    {// Dibujar los bordes del triángulo
        glBegin(GL_LINE_LOOP);
        glColor3ub(r,g,b);
        glVertex3f(p1ptr->x, p1ptr->y, p1ptr->z);
        glVertex3f(p2ptr->x, p2ptr->y, p2ptr->z);
        glVertex3f(p3ptr->x, p3ptr->y, p3ptr->z);
        glEnd();
        return;

    }
    //  Relleno del triángulo usando baricéntricas

    pixeldist = 2.0f / (float)dimentsioa;

    xmin = fminf(fminf(p1ptr->x, p2ptr->x), p3ptr->x);
    xmax = fmaxf(fmaxf(p1ptr->x, p2ptr->x), p3ptr->x);
    ymin = fminf(fminf(p1ptr->y, p2ptr->y), p3ptr->y);
    ymax = fmaxf(fmaxf(p1ptr->y, p2ptr->y), p3ptr->y);

    glBegin(GL_POINTS);
    for (float y = ymin; y <= ymax; y += pixeldist) {
        for (float x = xmin; x <= xmax; x += pixeldist) {
            float alpha, beta, gamma;
            barycentric(x, y, *p1ptr, *p2ptr, *p3ptr, &alpha, &beta, &gamma);

            if (alpha >= 0 && beta >= 0 && gamma >= 0) {
                if (optr->texturaduna) {
                    float u = alpha*optr->vertex_table[ind0].u +
                              beta *optr->vertex_table[ind1].u +
                              gamma*optr->vertex_table[ind2].u;
                    float v = alpha*optr->vertex_table[ind0].v +
                              beta *optr->vertex_table[ind1].v +
                              gamma*optr->vertex_table[ind2].v;
                    colorv = color_textura(u, v);
                    vec3 I = Iflat;
                    if (g_interp_mode == 1) {
                        I.x = alpha*I1.x + beta*I2.x + gamma*I3.x;
                        I.y = alpha*I1.y + beta*I2.y + gamma*I3.y;
                        I.z = alpha*I1.z + beta*I2.z + gamma*I3.z;
                        if (I.x<0) I.x=0; if (I.x>1) I.x=1;
                        if (I.y<0) I.y=0; if (I.y>1) I.y=1;
                        if (I.z<0) I.z=0; if (I.z>1) I.z=1;
                    }

                    unsigned char rr = (unsigned char)fmin(255.0, colorv[0] * I.x);
                    unsigned char gg = (unsigned char)fmin(255.0, colorv[1] * I.y);
                    unsigned char bb = (unsigned char)fmin(255.0, colorv[2] * I.z);

                    glColor3ub(rr, gg, bb);
                } else {
                    vec3 I = Iflat;
                    if (g_interp_mode == 1) {
                    I.x = alpha*I1.x + beta*I2.x + gamma*I3.x;
                    I.y = alpha*I1.y + beta*I2.y + gamma*I3.y;
                    I.z = alpha*I1.z + beta*I2.z + gamma*I3.z;
                    if (I.x<0) I.x=0; if (I.x>1) I.x=1;
                    if (I.y<0) I.y=0; if (I.y>1) I.y=1;
                    if (I.z<0) I.z=0; if (I.z>1) I.z=1;
                    }
                    unsigned char rr = (unsigned char)fmin(255.0, r * I.x);
                    unsigned char gg = (unsigned char)fmin(255.0, g * I.y);
                    unsigned char bb = (unsigned char)fmin(255.0, b * I.z);
                    glColor3ub(rr,gg,bb);

                   // glColor3ub(255,255,255); // Color blanco
                }

                float z = alpha*p1ptr->z + beta*p2ptr->z + gamma*p3ptr->z;
                glVertex3f(x, y, z);

                pixeles_dibujados++;
            }
        }
    }
    glEnd();
     

 return;
}



void dibujar_poligono(object3d *optr, int ti)
{
int i, ind0, ind1, ind2;
int atzeaurpegiada;
double * Nkam;

if (!optr) return;
if ( ti < 0 || ti >= optr->num_faces) return;

face f = optr->face_table[ti];

if(f.num_vertices < 3) return;

for(i = 0; i < f.num_vertices; i++){
    int vi = f.vertex_ind_table[i];
    if(vi < 0|| vi >= optr->num_vertices){
        printf("dibujar_poligono: cara %d inidce fuera de rango (%d)\n ", ti, vi);
        return;
    }  
} 
// lehenengo hiru erpinekin kalkulatuko dut ikusgaitasuna.
ind0 =f.vertex_ind_table[0];
ind1 = f.vertex_ind_table[1];
ind2 = f.vertex_ind_table[2];

// TODO erabaki marraztu behar den ala ez: ikuste bolumenetik kanpokoak ez marraztu

// TODO poligonoaren kolorea zein da? oraingoz zuria.
optr->face_table[ti].rgb[0] = 255;
optr->face_table[ti].rgb[1] = 255;
optr->face_table[ti].rgb[2] = 255;
atzeaurpegiada = 0;
// TODO atze-aurpegia?
point3 pcam = optr->vertex_table[ind0].camcoord;
double nx = optr->face_table[ti].Ncam[0];
double ny = optr->face_table[ti].Ncam[1];
double nz = optr->face_table[ti].Ncam[2];

// vector desde punto a cámara = -pcam
double facing = nx*(-pcam.x) + ny*(-pcam.y) + nz*(-pcam.z);

// si facing <= 0 -> cara de espaldas (puede que tengas que invertir signo según winding)
atzeaurpegiada = (facing <= 0.0) ? 1 : 0;
// atzeaurpegiada = ...
if ((!atzearpegiakmarraztu)&&atzeaurpegiada)
    {
    // Back culling...
    return;
    }
if (optr->texturaduna == 0) 
        {
        // TODO erpin bakoitzaren kolorea kalkulatu: argien, kameraren eta objektuaren orientazioaren arabera.
        argien_kalkulua_egin(optr, ti);
        }
// honaino iritsi bada bere triangelu guztiak marraztu behar ditut
for (i = 1; i<(optr->face_table[ti].num_vertices-1); i++)  // triangeluka marraztu: 4 erpinekin bi triangelu, bostekin 3...
    dibujar_triangulo(optr, ti, i, atzeaurpegiada);
}




static void marraztu(void)
{
    printf("marraztu inicio\n ");
    fflush(stdout);
float u,v;
int i,j;
object3d *auxptr;
double Fokudir[3];


  // marrazteko objektuak behar dira
  // no se puede dibujar sin objetos
if (foptr ==0){ 
    printf("marraztu foptr\n ");
     return;
     
}

// clear viewport...
if (objektuak == 1) glClear( GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT );
    else 
      {
      if (denak == 0) glClear( GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT );
      }
//TODO Ikuslearen edo kameraren erreferentzia-sistemara pasatzen duen matrizea lortu

//TODO argiek kameraren ikuspegian duten informazioa eguneratu (bai kokapenak, bai direkzioak)
double V[16];
printf("marraztu antes de invertir\n ");
invert_rigid(V, M_kam);
printf("marraztu despues de invertir\n ");

g_lights[3].pos_world = (vec3){ M_kam[3], M_kam[7], M_kam[11] };
g_lights[3].dir_world = (vec3){ -M_kam[2], -M_kam[6], -M_kam[10] };
// forward de cámara en view suele ser (0,0,-1)
lights_update_view(V, (vec3){0,0,-1});
printf("marraztu despues de update\n ");

if (objektuak == 1)
    {
        printf("marraztu objetuak\n ");
    if (denak == 1)
        {
            printf("marraztu denak\n ");
        //printf("objektuak marraztera\n");
        for (auxptr =foptr; auxptr != 0; auxptr = auxptr->hptr)
            {
                printf("marraztu auxptrn ");
            // TODO objektua kamerak nola ikusten duen adierazi objektuaren egituan bertan.
            kam_ikuspegia_lortu(auxptr);
            //printf("objektua kameraren ikuspegian daukat\n");
            for (i =0; i < auxptr->num_faces; i++)
                {
                    printf("marraztu dibujar poligono\n ");
                dibujar_poligono(auxptr,i);
                }
            }
        }
      else
        {
            printf("marraztu no denak\n ");
            if (!sel_ptr){
            printf("marraztu  sel_ptr 3\n ");
        } 
        // TODO objektua kamerak nola ikusten duen adierazi objektuaren egituan bertan.
        kam_ikuspegia_lortu(sel_ptr);
        printf("marraztudespues kam\n ");
        for (i =0; i < sel_ptr->num_faces; i++)
            {
            dibujar_poligono(sel_ptr,i);
            }
        }
    }
  else
    {
        printf("marraztu  no objeto\n ");
        if (!sel_ptr){
            sel_ptr = foptr;
            indexx = 0;
            printf("marraztu  sel_ptr 3\n ");
            if (!sel_ptr ){ 
                return;
            } 
        } 
        printf("marraztu  fallo ??\n ");
        if (indexx < 0) indexx = 0;
        if(indexx >=sel_ptr->num_faces){
            indexx = sel_ptr->num_faces -1;
        } 
        // TODO objektua kamerak nola ikusten duen adierazi objektuaren egituan bertan.
        // AQUI es donde falla en la funcion Kam_ikuspegia_lortu
        printf("marraztu  fallo 2\n ");
        kam_ikuspegia_lortu(sel_ptr);
         printf("marraztu  fallo 2\n ");
        dibujar_poligono(sel_ptr,indexx);
        printf("marraztu  fin\n ");
    }
glFlush();
}



void read_from_file(char *fitx, object3d **fptrptr)
{
int i,retval;
object3d *optr;

    printf("%s fitxategitik datuak hartzera\n",fitx);
    optr = (object3d *)malloc(sizeof(object3d));
    optr->rgb.r = 0;
    //int read_wavefront(char * file_name, object3d * object_ptr) {
    retval = read_wavefront(fitx, optr);
    if (retval != 0) 
         {
         printf("%s fitxategitik datuak hartzerakoan arazoak izan ditut\n",fitxiz);
         free(optr);
         }
       else
         {
        
         //printf("objektuaren matrizea...\n");
         optr->mptr = (mlist *)malloc(sizeof(mlist));
         for (i=0; i<16; i++) optr->mptr->m[i] =0;
         optr->mptr->m[0] = 1.0;
         optr->mptr->m[5] = 1.0;
         optr->mptr->m[10] = 1.0;
         optr->mptr->m[15] = 1.0;
         optr->mptr->hptr = 0;
         
         //printf("objektu edo kamera zerrendara doa informazioa...\n");
         optr->hptr = *fptrptr;
         *fptrptr = optr;
         //printf("normalak kalkulatzera\n");
         //for (i = 0; i< optr->num_vertices; i++) printf("%lf %lf %lf\n", optr->vertex_table[i].coord.x,optr->vertex_table[i].coord.y,optr->vertex_table[i].coord.z);
         obj_normalak_kalkulatu(optr);
         sel_ptr = optr;
         if (optr->texturaduna && (bufferra ==0))
            {
            // we put the information of the texture in the buffer pointed by bufferra. The dimensions of the texture are loaded into dimx and dimy
            retval = load_ppm("testura.ppm", &bufferra, &dimx, &dimy);
            if (retval !=1) 
                {
                printf("Ez dago testuraren fitxategia (testura.ppm)\n");
                optr->texturaduna=0;
                //exit(-1);
                }
            }
         }
     //printf("datuak irakurrita\n");
}

static void push_undo_obj(object3d *optr)
{
    if (!optr || !optr->mptr) return;
    if (undo_top >= UNDO_MAX) return;

    undo_stack[undo_top].kind = UNDO_OBJ;
    undo_stack[undo_top].obj = optr;
    for (int i=0;i<16;i++) undo_stack[undo_top].M_prev[i] = optr->mptr->m[i];
    undo_top++;
}

static void push_undo_cam(void)
{
    if (undo_top >= UNDO_MAX) return;

    undo_stack[undo_top].kind = UNDO_CAM;
    undo_stack[undo_top].obj = NULL;
    for (int i=0;i<16;i++) undo_stack[undo_top].M_prev[i] = M_kam[i];
    undo_top++;
}

static void push_undo_light(int idx)
{
    if (idx<0 || idx>=4) return;
    if (undo_top >= UNDO_MAX) return;

    undo_stack[undo_top].kind = UNDO_LIGHT;
    undo_stack[undo_top].light_idx = idx;
    undo_stack[undo_top].L_prev = g_lights[idx];
    undo_top++;
}


void undo()
{
    if (undo_top <= 0) {
        printf("UNDO: ez dago ezer desegiteko\n");
        return;
    }

    undo_top--;
    undo_entry *e = &undo_stack[undo_top];

    if (e->kind == UNDO_OBJ) {
        if (e->obj && e->obj->mptr) {
            for (int i=0;i<16;i++) e->obj->mptr->m[i] = e->M_prev[i];
        }
    } else if (e->kind == UNDO_CAM) {
        for (int i=0;i<16;i++) M_kam[i] = e->M_prev[i];
    } else { // UNDO_LIGHT
        int idx = e->light_idx;
        if (idx>=0 && idx<4) g_lights[idx] = e->L_prev;
    }

    glutPostRedisplay();
}



void x_aldaketa(int dir)
{
    if (kamera == 0) push_undo_obj(sel_ptr);
    else if (kamera == 1) push_undo_cam();
    else if (kamera == 2) push_undo_light(g_active_light);

    double M[16];
    double s = (dir ? 1.0 : -1.0);
    if (kamera == 0) { // OBJETO
        if (!sel_ptr || !sel_ptr->mptr) return;

        if (aldaketa == 'r') rotX_mat(M, 5.0 * s);
        else                 trans_mat(M, 0.02 * s, 0.0, 0.0);

        if (ald_lokala) objektuari_aldaketa_sartu_esk(M);
        else            objektuari_aldaketa_sartu_ezk(M);
    }
    else if (kamera == 1) { // CÁMARA
        if (aldaketa == 'r') rotY_mat(M, 5.0 * s);          // mirar izq/der típico
        else                 trans_mat(M, 0.02 * s, 0, 0);

        if (ald_lokala) kamerari_aldaketa_sartu_esk(M);
        else            kamerari_aldaketa_sartu_ezk(M);
    }
}



void y_aldaketa(int dir)
{
    if (kamera == 0) push_undo_obj(sel_ptr);
    else if (kamera == 1) push_undo_cam();
    else if (kamera == 2) push_undo_light(g_active_light);


    double M[16];
    double s = (dir ? 1.0 : -1.0);

    if (kamera == 0) { // OBJETO
        if (!sel_ptr || !sel_ptr->mptr) return;

        if (aldaketa == 'r') rotY_mat(M, 5.0 * s);
        else                 trans_mat(M, 0.0, 0.02 * s, 0.0);

        if (ald_lokala) objektuari_aldaketa_sartu_esk(M);
        else            objektuari_aldaketa_sartu_ezk(M);
    }
    else if (kamera == 1) { // CÁMARA
        if (aldaketa == 'r') rotX_mat(M, 5.0 * s);          // mirar arriba/abajo
        else                 trans_mat(M, 0, 0.02 * s, 0);

        if (ald_lokala) kamerari_aldaketa_sartu_esk(M);
        else            kamerari_aldaketa_sartu_ezk(M);
    }

}


void z_aldaketa(int dir)
{
    if (kamera == 0) push_undo_obj(sel_ptr);
    else if (kamera == 1) push_undo_cam();
    else if (kamera == 2) push_undo_light(g_active_light);
    double M[16];
    double s = (dir ? 1.0 : -1.0);

    if (kamera == 0) { // OBJETO
        if (!sel_ptr || !sel_ptr->mptr) return;

        if (aldaketa == 'r') rotZ_mat(M, 5.0 * s);
        else                 trans_mat(M, 0.0, 0.0, 0.02 * s);

        if (ald_lokala) objektuari_aldaketa_sartu_esk(M);
        else            objektuari_aldaketa_sartu_ezk(M);
    }
    else if (kamera == 1) { // CÁMARA
         if (ald_lokala == 1) {
        // VUELO: z = avanzar/retroceder SIEMPRE
        trans_mat(M, 0, 0, 0.05 * s);
        kamerari_aldaketa_sartu_esk(M); // local
    } else {
        // ANÁLISIS:
        if (aldaketa == 't') {
            trans_mat(M, 0, 0, 0.05 * s); // dolly
            kamerari_aldaketa_sartu_esk(M);
        } else {
            rotZ_mat(M, 5.0 * s); // ROLL
            kamerari_aldaketa_sartu_esk(M);
        }
    }            kamerari_aldaketa_sartu_ezk(M);
    }
}

static void look_at_pose(double M[16], vec3 eye, vec3 target, vec3 up)
{
    // forward = normalize(target-eye)
    vec3 f = {target.x-eye.x, target.y-eye.y, target.z-eye.z};
    double fl = sqrt(f.x*f.x+f.y*f.y+f.z*f.z); if(fl<1e-9) fl=1;
    f.x/=fl; f.y/=fl; f.z/=fl;

    // right = normalize(cross(f, up))
    vec3 r = { f.y*up.z - f.z*up.y, f.z*up.x - f.x*up.z, f.x*up.y - f.y*up.x };
    double rl = sqrt(r.x*r.x+r.y*r.y+r.z*r.z); if(rl<1e-9) rl=1;
    r.x/=rl; r.y/=rl; r.z/=rl;

    // true up = cross(r, f)
    vec3 u = { r.y*f.z - r.z*f.y, r.z*f.x - r.x*f.z, r.x*f.y - r.y*f.x };

    // Pose cámara->mundo con “mira” hacia -Z: colZ = -forward
    vec3 z = {-f.x,-f.y,-f.z};

    M[0]=r.x; M[1]=u.x; M[2]=z.x; M[3]=eye.x;
    M[4]=r.y; M[5]=u.y; M[6]=z.y; M[7]=eye.y;
    M[8]=r.z; M[9]=u.z; M[10]=z.z; M[11]=eye.z;
    M[12]=0; M[13]=0; M[14]=0; M[15]=1;
}
vec3 object_center_world(object3d *optr){
    vec3 c={0,0,0};
    if(!optr|| optr->num_vertices <=0 ) return c;

    for (int i = 0; i < optr->num_vertices; i++){
        c.x += optr->vertex_table[i].coord.x; 
        c.y += optr->vertex_table[i].coord.y; 
        c.z += optr->vertex_table[i].coord.z; 
    } 
    double inv = 1.0/ optr->num_vertices;
    c.x  *= inv;
    c.y *= inv;
    c.z *= inv;
    return c;
}

void kamera_objektuari_begira()
{
    if(!sel_ptr) return;
    vec3 C = object_center_world(sel_ptr);

    // mantén el radio actual si quieres:
    vec3 E = {M_kam[3], M_kam[7], M_kam[11]};
    vec3 d = {E.x-C.x, E.y-C.y, E.z-C.z};
    double r = sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
    if(r < 0.5) r = 2.5;

    // coloca la cámara en el eje Z del mundo a distancia r
    vec3 eye = { C.x, C.y, C.z + r };
    look_at_pose(M_kam, eye, C, (vec3){0,1,0});
}


void print_egoera()
{
if (kamera == 0)
    {
    if (ald_lokala == 1) printf("\nobjektua aldatzen ari zara, (aldaketa lokala)\n");
        else printf("\nobjektua aldatzen ari zara, (aldaketa globala)\n");
    }
if (kamera == 1) 
    {
    if (ald_lokala == 1) printf("\nkamera aldatzen ari zara hegaldi moduan\n");
        else printf("\nkamera aldatzen ari zara analisi-moduan\n");
    }
if (kamera == 2)
    {
    printf("\nargiak aldatzen ari zara\n");
    }
if (aldaketa=='t') printf("Traslazioa dago aktibatuta\n");
    else printf("Biraketak daude aktibatuta\n");
if (objektuaren_ikuspegia) printf("objektuaren ikuspuntua erakusten ari zara (`C` sakatu kamerarenera pasatzeko)\n");
}

// This function will be called whenever the user pushes one key
static void teklatua (unsigned char key, int x, int y)
{
int retval;
int i;
FILE *obj_file;

switch(key)
	{
	case 13: 
            if(sel_ptr == 0){
                sel_ptr = foptr;
                indexx = 0;
            } 
	        if (foptr != 0)  // objekturik ez badago ezer ez du egin behar
	                         // si no hay objeto que no haga nada
	            {
	            indexx ++;  // azkena bada lehenengoa bihurtu
		                // pero si es el último? hay que controlarlo!
		    if (indexx == sel_ptr->num_faces) 
		        {
		        indexx = 0;
		        if ((denak == 1) && (objektuak == 0))
		            {
		            glClear( GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT );
		            glFlush();
		            }
		        }
		    }
		break;
	case 'd':
		if (denak == 1) denak = 0;
		    else denak = 1;
		break;
	case 'o':
		if (objektuak == 1) objektuak = 0;
		    else objektuak = 1;
		break;
	case 'c':
	        if (kamera ==0) // objektua aldatzen ari naiz.
	            {
	            if (objektuaren_ikuspegia == 1) // objektuaren ikuspegian banago argiak aldatzera pasa naiteke, ez kamera aldatzera.
	                {
	                kamera =2;
	                printf("argiak aldatzera zoaz (objektuaren ikuspegian zaude)\n");
	                }
		        else 
		            {
		            kamera = 1;
		            ald_lokala = 1;  // hegaldi moduan jarriko naiz
	                 printf("kamera aldatzera zoaz (hegaldi moduan zaude)\n");
		            }
	            }
	          else
		    {
		    if (kamera == 1) 
		            {
		            kamera = 2;
	                printf("argia aldatzera zoaz \n");
	                }
		        else 
		            {
		            kamera = 0;
		            ald_lokala = 1;  // hegaldi moduan jarriko naiz
	                    printf("objektua aldatzera zoaz (aldaketa lokala daukazu) \n");
		            }
		    } 
		break;
	case 'C':
		if (objektuaren_ikuspegia == 1) objektuaren_ikuspegia = 0;
		    else 
		        {
		        objektuaren_ikuspegia = 1;
		        if (kamera == 1) 
		            {
		            kamera=0;
	                }
		        ald_lokala = 1;
		        printf("objektuaren ikuspegian sartu zara eta aldaketak era lokalean eragingo dituzu \n");
		        }
		break;
	case 'l':
		if (lineak == 1) lineak = 0;
		    else lineak = 1;
		break;
	case 't':
	        aldaketa = 't';
	        printf("traslazioak\n");
		break;
	case 'r':
		aldaketa = 'r';
		printf("biraketak\n");
		break;
	case 'n':
		if (atzearpegiakmarraztu == 1) 
		    {
		    atzearpegiakmarraztu = 0;
		    printf("atze-aurpegiak ez dira marraztuko\n");
		    }
		  else 
		    {
		    atzearpegiakmarraztu = 1;
		    printf("atze aurpegiak gorriz marraztuko dira\n");
		    }
		break;
	case 'g':
	        if (objektuaren_ikuspegia == 0) // objektuaren ikuspegian aldaketa beti lokala izango da.
	            {
	            if (ald_lokala == 1)
	                    {
	                    ald_lokala = 0;
	                    if ((kamera==1) &&(sel_ptr!=0)) 
	                        {
	                        kamera_objektuari_begira();
	                        //print_matrizea16(M_kam);
	                        }
	                    }
	                else ald_lokala = 1;
	            }
	          else printf("objektuaren ikuspegian aldaketak beti lokalak dira\n");
		break;
    case 'x':
                x_aldaketa(1);
                break;
    case 'y':
                y_aldaketa(1);
                break;
    case 'z':
                z_aldaketa(1);
                break;
    case 'X':
                x_aldaketa(0);
                break;
    case 'Y':
                y_aldaketa(0);
                break;
    case 'Z':
                z_aldaketa(0);
                break;
    case 'u':
                undo();
                break;
	case 'p':
		if (paralelo == 1) 
		    {
		    printf("perspektiban agertu behar du\n");
		    paralelo = 0;
		    }
		    else
		    {
		    printf("paraleloan agertu behar du\n");
		     paralelo = 1;
		    }
		break;
    case 'i':
        g_interp_mode = !g_interp_mode; // 0 flat, 1 gouraud
        break;
    case 'k':
        if (foptr != 0) {
            sel_ptr = sel_ptr->hptr;
            if (sel_ptr == 0) sel_ptr = foptr;
            indexx = 0;
            if ((ald_lokala == 0) && (kamera==1)) kamera_objektuari_begira();
        }
        break;
    case '0': 
        push_undo_light(0);
        g_lights[0].enabled = !g_lights[0].enabled; 
        break;   
	case '1':
		// Eguzkia piztu/itzali
        push_undo_light(1);
        g_lights[1].enabled = !g_lights[1].enabled; 
		break;
	case '2':
		// Bonbilla piztu/itzali
        push_undo_light(2);
        g_lights[2].enabled = !g_lights[2].enabled; 
		break;
	case '3':
        push_undo_light(3);
		// objektuaren fokua piztu/itzali
        g_lights[3].enabled = !g_lights[3].enabled;
		break;
	case '+':
        push_undo_light(0);
		if (kamera == 2 && g_lights[g_active_light].type == L_SPOT) 
		    {// Fokuen irekiera handitu
                double deg = acos(g_lights[g_active_light].cosCutoff) * 180.0/M_PI;
                deg += 2.0;
                if (deg > 80.0) deg = 80.0;
                g_lights[g_active_light].cosCutoff = cos(deg*M_PI/180.0);
            
		    }
		  else 
		    {
		    if (kamera == 1)
		        {// kameraren ikuste-bolumena handitu
		        }
		    }
		break;
	case '-':
        push_undo_light(0);
		if (kamera == 2 && g_lights[g_active_light].type == L_SPOT) 
		    {// Fokuen irekiera txikitu
                double deg = acos(g_lights[g_active_light].cosCutoff) * 180.0/M_PI;
                deg -= 2.0;
                if (deg < 5.0) deg = 5.0;
                g_lights[g_active_light].cosCutoff = cos(deg*M_PI/180.0);
		    }
		  else 
		    {
		    if (kamera == 1)
		        {// kameraren ikuste-bolumena txikitu
		        }
		    }
		break;
	case 'f':
	        /*Ask for file*/
	        printf("idatzi fitxategi izena\n"); 
	        scanf("%s", &(fitxiz[0]));
	        read_from_file(fitxiz,&foptr);
	        indexx = 0;
	        if ((ald_lokala==0)&&(kamera==1) &&(sel_ptr!=0)) kamera_objektuari_begira();
                break;
    case 9: /* <TAB> */
            if (foptr != 0) // objekturik gabe ez du ezer egin behar
                            // si no hay objeto no hace nada
                {
                sel_ptr = sel_ptr->hptr;
                /*The selection is circular, thus if we move out of the list we go back to the first element*/
                if (sel_ptr == 0) sel_ptr = foptr;
                indexx =0; // the selected polygon is the first one
                if ((ald_lokala == 0)&&(kamera==1))
                    {
                    //kamera objektuari begira jarri behar da!!
                    kamera_objektuari_begira();
                    }
                }
            break;
	case 27:  // <ESC>
		exit( 0 );
		break;
	default:
		printf("%d %c\n", key, key );
	}
print_egoera();
// The screen must be drawn to show the new triangle
glutPostRedisplay();
}


void viewportberria (int zabal, int garai)
{
if (zabal < garai)  dimentsioa = zabal;
    else  dimentsioa = garai;
glViewport(0,0,dimentsioa,dimentsioa);
printf("linea kopuru berria = %d\n",dimentsioa);
}
       
int main(int argc, char** argv)
{
    int retval,i;


	printf(" Triangeluak: barneko puntuak eta testura\n Triángulos con puntos internos y textura \n");
	printf("Press <ESC> to finish\n");
	glutInit(&argc,argv);
	glutInitDisplayMode ( GLUT_RGB|GLUT_DEPTH );
	dimentsioa = 500;
	glutInitWindowSize ( dimentsioa, dimentsioa );
	glutInitWindowPosition ( 100, 100 );
	glutCreateWindow( "KBG/GO praktika" );

	glutDisplayFunc( marraztu );
	glutKeyboardFunc( teklatua );
	glutReshapeFunc( viewportberria);
	/* we put the information of the texture in the buffer pointed by bufferra. The dimensions of the texture are loaded into dimx and dimy
        retval = load_ppm("testura.ppm", &bufferra, &dimx, &dimy);
        if (retval !=1) 
            {
            printf("Ez dago testuraren fitxategia (testura.ppm)\n");
            exit(-1);
            }
             */ 
    //bufferra =0; dimx=0;dimy=0;
	glClearColor( 0.0f, 0.0f, 0.7f, 1.0f );
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_DEPTH_TEST); // activar el test de profundidad (Z-buffer)
        glDepthFunc(GL_GREATER);  glClearDepth(0.0);// Handiena marraztu
        //glMatrixMode(GL_PROJECTION);
        //glLoadIdentity();
        //glOrtho(-1.0, 1.0, -1.0, 1.0, 1.0, -1.0);
        //glMatrixMode(GL_MODELVIEW);
        denak = 1;
        lineak =0;
        objektuak = 1;
        kamera = 0;
        foptr = 0;
        sel_ptr = 0;
        aldaketa = 'r';
        ald_lokala =1;
        objektuaren_ikuspegia =0;
        paralelo = 0;
        atzearpegiakmarraztu = 1;
        ident16(M_kam);
        M_kam[3] = 0.0;
        M_kam[7] = 0.2;
        M_kam[11] = 5.0;
        // TODO kamera hasieratu kamera (0,0,2.5) kokapenean dago hasieran
        
        // TODO Argiak hasieratu
        lights_init_defaults();
        g_mat_default.Ka = (vec3){0.10,0.10,0.10};
        g_mat_default.Kd = (vec3){0.70,0.70,0.70};
        g_mat_default.Ks = (vec3){0.20,0.20,0.20};
        g_mat_default.shininess = 32.0;
        if (argc>1) read_from_file(argv[1],&foptr);
            else 
            {
            read_from_file("abioia-1+1.obj",&foptr);
            if (sel_ptr != 0) 
                { sel_ptr->mptr->m[3] = -1.0;
                }   
            read_from_file("abioia-1+1.obj",&foptr);
            if (sel_ptr != 0) 
                { sel_ptr->mptr->m[3] = 1.0;
                }   
            read_from_file("abioia-1+1.obj",&foptr);
            if (sel_ptr != 0) 
                { 
                sel_ptr->mptr->m[7] = -0.4;
                sel_ptr->mptr->m[11] = 0.5;
                }   
            read_from_file("abioia-1+1.obj",&foptr);
            if (sel_ptr != 0) 
                { sel_ptr->mptr->m[11] = -0.7;
                }        
            read_from_file("abioia-1+1.obj",&foptr);
            if (sel_ptr != 0) 
                { sel_ptr->mptr->m[7] = 0.7;
                }                  
            read_from_file("z-1+1.obj",&foptr);
            if(!foptr){
                printf( "Error: foptr es NULL tras read_from_file\n ");
                exit(1);
            } 
            sel_ptr = foptr;
            printf("objeto cargado, sel_ptr = %p\n ", (void*)sel_ptr);
            }
            
        print_egoera();
	glutMainLoop();
    printf(" Carga OK \n");

	return 0;   
}
