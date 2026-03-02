#ifndef OBJ_H
#define	OBJ_H

/** STRUCTURES **/

/****************************
 * Structure to store the   *
 * coordinates and texture coordinates of 3D points *
 ****************************/
typedef struct punto
{
float x, y, z, u,v;
} punto;

/****************************
 * Structure to store the   *
 * coordinates of 3D points *
 ****************************/
typedef struct {
     double x, y, z;
} point3;

/*****************************
 * Structure to store the    *
 * coordinates of 3D vectors *
 *****************************/
typedef struct {
     double x, y, z;
} vector3;

/****************************
 * Structure to store the   *
 * colors in RGB mode       *
 ****************************/
typedef struct {
     double r, g, b;
} color3;

/****************************
 * Structure to store       *
 * the list of matices      *
 ****************************/

typedef struct mlist
    {
    double m[16];
    struct mlist *hptr;
    } mlist;


/***************************
 * Light
 ***************************/
 
typedef struct light
{
int onoff;
int type;      // 0 -> directional, 1 -> positional, 2 -> spot light
color3 I;
double pos[3];    // positional or spot light
double campos[3];
double dir[3];   // directional or spot light
double camdir[3];
double aperture;   // cos(ang) if  0 --> any position is iluminated. 
                   //   if (not 0) only the cone is iluminated.
} light;


/****************************
 * Structure to store       *
 * objects' vertices         *
 ****************************/
typedef struct {
    point3 coord;                       /* coordinates,x, y, z */
    point3 camcoord;
    point3 proedcoord;
    double u,v;
    int num_faces;                    /* number of faces that share this vertex */
    double N[3];
    double Ncam[3];
    unsigned char rgb[3];
} vertex;

/****************************
 * Structure to store       *
 * objects' faces or        *
 * polygons                 *
 ****************************/
typedef struct {
    int num_vertices;                 /* number of vertices in the face */
    int *vertex_ind_table;                /* table with the index of each vertex */
    double N[3];
    double Ncam[3];
    unsigned char rgb[3];
} face;


/****************************
 * Structure to store a     *
 * pile of 3D objects       *
 ****************************/
struct object3d{
    int num_vertices;                 /* number of vertices in the object*/
    vertex *vertex_table;               /* table of vertices */
    int num_faces;                    /* number of faces in the object */
    face *face_table;                   /* table of faces */
    point3 min;                         /* coordinates' lower bounds */
    point3 max;                         /* coordinates' bigger bounds */
    mlist *mptr;
    color3 rgb;
    color3 ka;
    color3 kd;
    color3 ks;
    int ns;
    int texturaduna;
    struct object3d *hptr;              /* next element in the pile of objects */
};

typedef struct object3d object3d;


int read_wavefront(char * file_name, object3d * object_ptr);

#endif	/* OBJ_H */

