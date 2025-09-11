// painter.c
// Exemplo didático: Cena 3D com Algoritmo do Pintor, iluminação, sombreamento,
// mapeamento de textura e curva paramétrica (Bezier).
// Requisitos: OpenGL, GLU, GLUT
// Compile: gcc -o painter painter.c -lGL -lGLU -lglut

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define TEX_SIZE 128
#define PI 3.14159265358979323846

static int winW = 800, winH = 600;
static float angleX = 25.0f, angleY = -30.0f;
static float dist = 6.0f;
static int useDepthTest = 0; // 0 = use painter, 1 = z-buffer (toggle)

typedef struct {
    float v[3];
} Vec3;

typedef struct {
    int idx[4];   // indices of vertices (quad)
    float normal[3];
    float avgDepth; // for painter sorting
    float tex[4][2];
} Face;

typedef struct {
    Vec3 pos;
} Vertex;

// Simple cube mesh (8 vertices, 6 faces (as quads))
Vertex verts[8];
Face faces[6];

GLuint texID;

// Utility vec ops
static Vec3 sub3(Vec3 a, Vec3 b){ Vec3 r; r.v[0]=a.v[0]-b.v[0]; r.v[1]=a.v[1]-b.v[1]; r.v[2]=a.v[2]-b.v[2]; return r; }
static Vec3 cross3(Vec3 a, Vec3 b){ Vec3 r; r.v[0]=a.v[1]*b.v[2]-a.v[2]*b.v[1]; r.v[1]=a.v[2]*b.v[0]-a.v[0]*b.v[2]; r.v[2]=a.v[0]*b.v[1]-a.v[1]*b.v[0]; return r;}
static void normalize3(float v[3]){ float len = sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); if(len>1e-6f){ v[0]/=len; v[1]/=len; v[2]/=len; } }

// Create procedural checkerboard texture
void createCheckerTexture(){
    unsigned char data[TEX_SIZE][TEX_SIZE][3];
    for(int y=0;y<TEX_SIZE;y++){
        for(int x=0;x<TEX_SIZE;x++){
            int cx = (x/(TEX_SIZE/8))%2;
            int cy = (y/(TEX_SIZE/8))%2;
            int c = (cx^cy) ? 255 : 80;
            data[y][x][0] = data[y][x][1] = data[y][x][2] = (unsigned char)c;
        }
    }
    glGenTextures(1,&texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,TEX_SIZE,TEX_SIZE,0,GL_RGB,GL_UNSIGNED_BYTE,data);
}

// Build cube geometry with texture coordinates and normals
void buildCube(){
    // vertices unit cube centered at origin
    float s = 1.0f;
    float hx = s/2, hy = s/2, hz = s/2;
    // 0..7
    float coord[8][3] = {
        {-hx,-hy,-hz},{ hx,-hy,-hz},{ hx, hy,-hz},{-hx, hy,-hz},
        {-hx,-hy, hz},{ hx,-hy, hz},{ hx, hy, hz},{-hx, hy, hz}
    };
    for(int i=0;i<8;i++){ verts[i].pos.v[0]=coord[i][0]; verts[i].pos.v[1]=coord[i][1]; verts[i].pos.v[2]=coord[i][2]; }
    // faces as quads (winding CCW looking from outside)
    int faceIdx[6][4] = {
        {0,1,2,3}, // back -z
        {4,7,6,5}, // front +z
        {0,4,5,1}, // bottom -y
        {3,2,6,7}, // top +y
        {1,5,6,2}, // right +x
        {0,3,7,4}  // left -x
    };
    for(int f=0;f<6;f++){
        for(int k=0;k<4;k++) faces[f].idx[k]=faceIdx[f][k];
        // tex coords simple mapping
        faces[f].tex[0][0]=0; faces[f].tex[0][1]=0;
        faces[f].tex[1][0]=1; faces[f].tex[1][1]=0;
        faces[f].tex[2][0]=1; faces[f].tex[2][1]=1;
        faces[f].tex[3][0]=0; faces[f].tex[3][1]=1;
        // compute normal in object space
        Vec3 a = verts[faces[f].idx[0]].pos;
        Vec3 b = verts[faces[f].idx[1]].pos;
        Vec3 c = verts[faces[f].idx[2]].pos;
        Vec3 u = sub3(b,a), v = sub3(c,a);
        Vec3 n = cross3(u,v);
        float nn[3] = {n.v[0], n.v[1], n.v[2]}; normalize3(nn);
        faces[f].normal[0]=nn[0]; faces[f].normal[1]=nn[1]; faces[f].normal[2]=nn[2];
    }
}

// Transform a point by modelview (4x4) matrix, store eye-space z for depth sorting
float transformZ(const float M[16], const Vec3 *p){
    // eye-space z = M_2* (x,y,z,1)
    float x=p->v[0], y=p->v[1], z=p->v[2];
    float ez = M[2]*x + M[6]*y + M[10]*z + M[14]*1.0f;
    return ez;
}

// Painter: compute average depth of each face in eye-space, sort, and render back-to-front
int cmpFace(const void *a, const void *b){
    const Face *fa = (const Face*)a;
    const Face *fb = (const Face*)b;
    if (fa->avgDepth < fb->avgDepth) return 1;
    if (fa->avgDepth > fb->avgDepth) return -1;
    return 0;
}

// Draw cube using painter algorithm
void drawCubePainter(){
    // get MODELVIEW matrix to transform verts into eye space
    float M[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, M);
    // compute avg depth per face
    Face sorted[6];
    for(int f=0;f<6;f++) sorted[f]=faces[f];
    for(int f=0;f<6;f++){
        float sumz=0;
        for(int k=0;k<4;k++){
            Vec3 *p = &verts[sorted[f].idx[k]].pos;
            sumz += transformZ(M,p);
        }
        sorted[f].avgDepth = sumz/4.0f;
    }
    qsort(sorted, 6, sizeof(Face), cmpFace);
    // Render back-to-front without depth test (painter)
    if(!useDepthTest) glDisable(GL_DEPTH_TEST);
    else glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texID);
    for(int f=0;f<6;f++){
        Face *F = &sorted[f];
        glBegin(GL_QUADS);
        // set normal (in object space) - transform by normal matrix is skipped (approx ok with uniform scale)
        glNormal3fv(F->normal);
        for(int k=0;k<4;k++){
            int vi = F->idx[k];
            glTexCoord2fv(F->tex[k]);
            glVertex3fv(verts[vi].pos.v);
        }
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
    if(!useDepthTest) glEnable(GL_DEPTH_TEST); // restore
}

// Evaluate cubic Bezier curve in 3D for parameter t in [0,1]
Vec3 bezier3(const Vec3 cp[4], float t){
    float u = 1.0f - t;
    float b0 = u*u*u;
    float b1 = 3*u*u*t;
    float b2 = 3*u*t*t;
    float b3 = t*t*t;
    Vec3 r;
    r.v[0] = b0*cp[0].v[0] + b1*cp[1].v[0] + b2*cp[2].v[0] + b3*cp[3].v[0];
    r.v[1] = b0*cp[0].v[1] + b1*cp[1].v[1] + b2*cp[2].v[1] + b3*cp[3].v[1];
    r.v[2] = b0*cp[0].v[2] + b1*cp[1].v[2] + b2*cp[2].v[2] + b3*cp[3].v[2];
    return r;
}

// Draw parametric Bezier curve (with lighting disabled to show emissive color)
void drawBezierCurve(){
    // control points in object space
    Vec3 cp[4] = {
        {{-1.5f, -1.0f, 0.5f}},
        {{-0.5f,  1.5f, -0.5f}},
        {{ 0.5f, -1.5f,  1.0f}},
        {{ 1.5f,  1.0f,  0.0f}}
    };
    glDisable(GL_LIGHTING);
    glLineWidth(3.0f);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<=100;i++){
        float t = i/100.0f;
        Vec3 p = bezier3(cp, t);
        glColor3f(1.0f, 0.4f, 0.0f); // emissive curve color
        glVertex3fv(p.v);
    }
    glEnd();
    // draw control polygon
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glColor3f(0.2f, 0.6f, 1.0f);
    for(int i=0;i<4;i++) glVertex3fv(cp[i].v);
    glEnd();
    glEnable(GL_LIGHTING);
}

// GLUT display callback
void display(){
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    // camera transform (simple trackball)
    glTranslatef(0.0f, 0.0f, -dist);
    glRotatef(angleX, 1,0,0);
    glRotatef(angleY, 0,1,0);

    // Lighting: place light in world space
    GLfloat light_pos[] = {2.0f, 3.0f, 2.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

    // Draw cube with painter's algorithm
    glPushMatrix();
      // apply some model transform so the cube and curve overlap in depth
      glTranslatef(-0.2f, 0.0f, 0.0f);
      glRotatef(25.0f, 1,1,0);
      drawCubePainter();
    glPopMatrix();

    // Draw a second textured quad (as floor) — also sorted via painter
    glPushMatrix();
      glTranslatef(0.0f,-1.2f,0.0f);
      glScalef(3.0f,0.01f,3.0f);
      // simple floor as a cube scaled thin
      drawCubePainter();
    glPopMatrix();

    // Draw parametric curve in front of cube
    drawBezierCurve();

    glutSwapBuffers();
}

// reshape callback
void reshape(int w, int h){
    winW = w; winH = h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)w/(double)h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

// simple keyboard controls
void keyboard(unsigned char key, int x, int y){
    switch(key){
        case 27: exit(0); break;
        case 'd': dist += 0.2f; break;
        case 'a': dist -= 0.2f; if(dist<1.0f) dist=1.0f; break;
        case 'z': angleY -= 5.0f; break;
        case 'x': angleY += 5.0f; break;
        case 's': angleX -= 5.0f; break;
        case 'w': angleX += 5.0f; break;
        case 't': useDepthTest = !useDepthTest; printf("Depth test %s\n", useDepthTest?"ON (z-buffer)":"OFF (painter)"); break;
    }
    glutPostRedisplay();
}

// init GL state
void initGL(){
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH); // Gouraud shading
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    // material
    GLfloat mat_diff[] = {0.8f,0.8f,0.8f,1.0f};
    GLfloat mat_spec[] = {0.6f,0.6f,0.6f,1.0f};
    GLfloat mat_shin[] = {50.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diff);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_spec);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shin);

    createCheckerTexture();
    buildCube();

    glEnable(GL_DEPTH_TEST); // for other parts; painter mode will disable when needed
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
}

// simple idle to animate slowly if wanted
void idle(){
    // small automatic rotation
    angleY += 0.05f;
    if(angleY > 360.0f) angleY -= 360.0f;
    glutPostRedisplay();
}

int main(int argc, char **argv){
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("Algoritmo do Pintor - OpenGL C Example");
    initGL();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutIdleFunc(idle);
    printf("Controles: w/s (tilt), z/x (yaw), a/d (zoom), t (toggle painter/z-buffer), ESC exit\n");
    glutMainLoop();
    return 0;
}
