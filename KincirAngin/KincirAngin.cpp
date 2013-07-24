#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#include <GL/gl.h>
#include "imageloader.h"
#include "vec3f.h"
#endif

static GLfloat spin, spin2 = 0.0;
float angle = 0;
float sudutnya = 30;
using namespace std;

float lastx, lasty;
GLint stencilBits;
static int viewx = 100;
static int viewy = 150;
static int viewz = 250;
GLuint _textureId;           //ID OpenGL untuk tekstur
GLuint texture[5];

float rot = 0;

//train 2D
//class untuk terain 2D
class Terrain {
private:
	int w; //Width
	int l; //Length
	float** hs; //Heights
	Vec3f** normals;
	bool computedNormals; //Whether normals is up-to-date
public:
	Terrain(int w2, int l2) {
		w = w2;
		l = l2;

		hs = new float*[l];
		for (int i = 0; i < l; i++) {
			hs[i] = new float[w];
		}

		normals = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals[i] = new Vec3f[w];
		}

		computedNormals = false;
	}

	~Terrain() {
		for (int i = 0; i < l; i++) {
			delete[] hs[i];
		}
		delete[] hs;

		for (int i = 0; i < l; i++) {
			delete[] normals[i];
		}
		delete[] normals;
	}

	int width() {
		return w;
	}

	int length() {
		return l;
	}

	//Sets the height at (x, z) to y
	void setHeight(int x, int z, float y) {
		hs[z][x] = y;
		computedNormals = false;
	}

	//Returns the height at (x, z)
	float getHeight(int x, int z) {
		return hs[z][x];
	}

	//Computes the normals, if they haven't been computed yet
	void computeNormals() {
		if (computedNormals) {
			return;
		}

		//Compute the rough version of the normals
		Vec3f** normals2 = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals2[i] = new Vec3f[w];
		}

		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum(0.0f, 0.0f, 0.0f);

				Vec3f out;
				if (z > 0) {
					out = Vec3f(0.0f, hs[z - 1][x] - hs[z][x], -1.0f);
				}
				Vec3f in;
				if (z < l - 1) {
					in = Vec3f(0.0f, hs[z + 1][x] - hs[z][x], 1.0f);
				}
				Vec3f left;
				if (x > 0) {
					left = Vec3f(-1.0f, hs[z][x - 1] - hs[z][x], 0.0f);
				}
				Vec3f right;
				if (x < w - 1) {
					right = Vec3f(1.0f, hs[z][x + 1] - hs[z][x], 0.0f);
				}

				if (x > 0 && z > 0) {
					sum += out.cross(left).normalize();
				}
				if (x > 0 && z < l - 1) {
					sum += left.cross(in).normalize();
				}
				if (x < w - 1 && z < l - 1) {
					sum += in.cross(right).normalize();
				}
				if (x < w - 1 && z > 0) {
					sum += right.cross(out).normalize();
				}

				normals2[z][x] = sum;
			}
		}

		//Smooth out the normals
		const float FALLOUT_RATIO = 0.5f;
		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum = normals2[z][x];

				if (x > 0) {
					sum += normals2[z][x - 1] * FALLOUT_RATIO;
				}
				if (x < w - 1) {
					sum += normals2[z][x + 1] * FALLOUT_RATIO;
				}
				if (z > 0) {
					sum += normals2[z - 1][x] * FALLOUT_RATIO;
				}
				if (z < l - 1) {
					sum += normals2[z + 1][x] * FALLOUT_RATIO;
				}

				if (sum.magnitude() == 0) {
					sum = Vec3f(0.0f, 1.0f, 0.0f);
				}
				normals[z][x] = sum;
			}
		}

		for (int i = 0; i < l; i++) {
			delete[] normals2[i];
		}
		delete[] normals2;

		computedNormals = true;
	}

	//Returns the normal at (x, z)
	Vec3f getNormal(int x, int z) {
		if (!computedNormals) {
			computeNormals();
		}
		return normals[z][x];
	}
};
//end class


void initRendering() {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);
		
}

//Loads a terrain from a heightmap.  The heights of the terrain range from
//-height / 2 to height / 2.
//load terain di procedure inisialisasi
Terrain* loadTerrain(const char* filename, float height) {
	Image* image = loadBMP(filename);
	Terrain* t = new Terrain(image->width, image->height);
	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			unsigned char color = (unsigned char) image->pixels[3 * (y
					* image->width + x)];
			float h = height * ((color / 255.0f) - 0.5f);
			t->setHeight(x, y, h);
		}
	}

	delete image;
	t->computeNormals();
	return t;
}

float _angle = 60.0f;
//buat tipe data terain
Terrain* _terrain;
Terrain* _terrainTanah;
Terrain* _terrainAir;

const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 1.0f };

const GLfloat light_ambient2[] = { 0.3f, 0.3f, 0.3f, 0.0f };
const GLfloat light_diffuse2[] = { 0.3f, 0.3f, 0.3f, 0.0f };

const GLfloat mat_ambient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat high_shininess[] = { 100.0f };

void cleanup() {
	delete _terrain;
	delete _terrainTanah;
}

//untuk di display
void drawSceneTanah(Terrain *terrain, GLfloat r, GLfloat g, GLfloat b) {
	//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	/*
	 glMatrixMode(GL_MODELVIEW);
	 glLoadIdentity();
	 glTranslatef(0.0f, 0.0f, -10.0f);
	 glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
	 glRotatef(-_angle, 0.0f, 1.0f, 0.0f);

	 GLfloat ambientColor[] = {0.4f, 0.4f, 0.4f, 1.0f};
	 glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientColor);

	 GLfloat lightColor0[] = {0.6f, 0.6f, 0.6f, 1.0f};
	 GLfloat lightPos0[] = {-0.5f, 0.8f, 0.1f, 0.0f};
	 glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor0);
	 glLightfv(GL_LIGHT0, GL_POSITION, lightPos0);
	 */
	float scale = 500.0f / max(terrain->width() - 1, terrain->length() - 1);
	glScalef(scale, scale, scale);
	glTranslatef(-(float) (terrain->width() - 1) / 2, 0.0f,
			-(float) (terrain->length() - 1) / 2);

	glColor3f(r, g, b);
	for (int z = 0; z < terrain->length() - 1; z++) {
		//Makes OpenGL draw a triangle at every three consecutive vertices
		glBegin(GL_TRIANGLE_STRIP);
		for (int x = 0; x < terrain->width(); x++) {
			Vec3f normal = terrain->getNormal(x, z);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z), z);
			normal = terrain->getNormal(x, z + 1);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z + 1), z + 1);
		}
		glEnd();
	}

}

unsigned int LoadTextureFromBmpFile(char *filename);

void dinding(float x1,float y1,float z1,float x2,float y2,float z2)
{
//depan
glTexCoord2f(0.0, 0.0);
glVertex3f(x1,y1,z1);
glTexCoord2f(0.0, 1.0);
glVertex3f(x2,y1,z1);
glTexCoord2f(1.0, 1.0);
glVertex3f(x2,y2,z1);
glTexCoord2f(1.0, 0.0);
glVertex3f(x1,y2,z1);
//atas
glTexCoord2f(0.0, 0.0);
glVertex3f(x1,y2,z1);
glTexCoord2f(0.0, 1.0);
glVertex3f(x2,y2,z1);
glTexCoord2f(1.0, 1.0);
glVertex3f(x2,y2,z2);
glTexCoord2f(1.0, 0.0);
glVertex3f(x1,y2,z2);
//belakang
glTexCoord2f(0.0, 0.0);
glVertex3f(x1,y2,z2);
glTexCoord2f(0.0, 1.0);
glVertex3f(x2,y2,z2);
glTexCoord2f(1.0, 1.0);
glVertex3f(x2,y1,z2);
glTexCoord2f(1.0, 0.0);
glVertex3f(x1,y1,z2);
//bawah
glTexCoord2f(0.0, 0.0);
glVertex3f(x1,y1,z2);
glTexCoord2f(1.0, 0.0);
glVertex3f(x2,y1,z2);
glTexCoord2f(1.0, 1.0);
glVertex3f(x2,y1,z1);
glTexCoord2f(0.0, 1.0);
glVertex3f(x1,y1,z1);
//samping kiri
glTexCoord2f(0.0, 0.0);
glVertex3f(x1,y1,z1);
glTexCoord2f(1.0, 0.0);
glVertex3f(x1,y2,z1);
glTexCoord2f(1.0, 1.0);
glVertex3f(x1,y2,z2);
glTexCoord2f(0.0, 1.0);
glVertex3f(x1,y1,z2);
//samping kanan
glTexCoord2f(0.0, 0.0);
glVertex3f(x2,y1,z1);
glTexCoord2f(1.0, 0.0);
glVertex3f(x2,y2,z1);
glTexCoord2f(1.0, 1.0);
glVertex3f(x2,y2,z2);
glTexCoord2f(0.0, 1.0);
glVertex3f(x2,y1,z2);
}


void papan()
{
     //papan atas
    glPushMatrix();
    glTranslatef(112.0, -7.0, 80.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(5.0, 1.5, 32.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(106.0, -7.0, 80.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(5.0, 1.5, 32.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(100.0, -7.0, 80.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(5.0, 1.5, 32.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(94.0, -7.0, 80.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(5.0, 1.5, 32.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(88.0, -7.0, 80.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(5.0, 1.5, 32.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(82.0, -7.0, 80.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(5.0, 1.5, 32.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //papan samping
    glPushMatrix();
    glTranslatef(97.0, -8.5, 95.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(35.0, 1.5, 1.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(97.0, -8.5, 65.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(35.0, 1.5, 1.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //papan ke air
    glPushMatrix();
    glTranslatef(103.0, -12.5, 94.5);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 10.5, 2.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(92.0, -12.5, 94.5);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 10.5, 2.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(81.0, -12.5, 94.5);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 10.5, 2.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(103.0, -12.5, 65.5);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 10.5, 2.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(92.0, -12.5, 65.5);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 10.5, 2.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(81.0, -12.5, 65.5);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 10.5, 2.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
}

void rumah()
{
     
    //glPushMatrix();
    //glTranslatef(-80.0, 0.0, -80.5);
    //glColor3f(0.9f, 0.4f, 0.7f);
    //glScalef(80.0, 30.5, 100.5);
    //glutSolidCube(1.0f);
   // glPopMatrix();
    
    //atap
    glPushMatrix();
    glTranslatef(-92.0, 15.0, -80.5);
    glRotatef(45,0,0,1);
    glColor3f(0.9f, 0.0f, 0.0f);
    glScalef(40.0, 40.0, 100.0);
    glutSolidCube(1.0f);
    glPopMatrix();

    
    glPushMatrix();
    glTranslatef(-65.5, 3.5, -80.5);
    glRotatef(70,0,0,1);
    glColor3f(0.9f, 0.0f, 0.0f);
    glScalef(40.0, 40.0, 100.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //pintu
    glPushMatrix();
    glTranslatef(-40.5, 4, -55.5);
    glRotatef(90,0,1,0);
    glColor3f(0.8f, 0.4f, 0.2f);
    glScalef(7.0, 11.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-40.5, 4, -55.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(9.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-40.5, 4, -47.5);
    glRotatef(90,0,1,0);
    glColor3f(0.8f, 0.4f, 0.2f);
    glScalef(7.0, 11.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-40.5, 4, -47.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(9.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-40.5, 4, -51.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(3.0, 1.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-40.5, 6, -51.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(3.0, 1.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //jendela
    glPushMatrix();
    glTranslatef(-40.5, 5, -85.5);
    glRotatef(90,0,1,0);
    glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(15.0, 8.0, 1.7);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-40.5, 5, -85.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(17.0, 10.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-40.5, 5, -85.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(1.0, 10.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-40.5, 5, -85.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(17.0, 1.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-40.5, 5, -105.5);
    glRotatef(90,0,1,0);
    glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(15.0, 8.0, 1.7);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-40.5, 5, -105.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(17.0, 10.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-40.5, 5, -105.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(1.0, 10.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-40.5, 5, -105.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(17.0, 1.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-90.5, 5, -30.5);
    glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(15.0, 8.0, 1.7);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-90.5, 5, -30.5);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(17.0, 10.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-90.5, 5, -30.5);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(1.0, 10.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-90.5, 5, -30.5);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(17.0, 1.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-70.5, 5, -30.5);
    glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(15.0, 8.0, 1.7);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-70.5, 5, -30.5);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(17.0, 10.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-70.5, 5, -30.5);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(1.0, 10.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-70.5, 5, -30.5);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(17.0, 1.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
}

void gedungkincir()
{
     //bawah
    glPushMatrix();
    glTranslatef(-80.0, -10.0, 40.5);
    glColor3f(0.5f, 0.5f, 0.5f);
    glScalef(70.0, 20.5, 70.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-80.0, -10.0, 40.5);
    glRotatef(15,0,1,0);
    glColor3f(0.5f, 0.5f, 0.5f);
    glScalef(70.0, 20.5, 70.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-80.0, -10.0, 40.5);
    glRotatef(20,0,1,0);
    glColor3f(0.5f, 0.5f, 0.5f);
    glScalef(70.0, 20.5, 70.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //gedungbwh
    glPushMatrix();
    glTranslatef(-80.0, 10.0, 40.5);
    glColor3f(0.5f, 0.0f, 0.0f);
    glScalef(28.0, 25.0, 55.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-80.0, 10.0, 40.5);
    glRotatef(90,0,1,0);
    glColor3f(0.5f, 0.0f, 0.0f);
    glScalef(28.0, 25.0, 55.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-80.0, 10.0, 40.5);
    glRotatef(45,0,1,0);
    glColor3f(0.5f, 0.0f, 0.0f);
    glScalef(28.0, 25.0, 55.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-80.0, 10.0, 40.5);
    glRotatef(135,0,1,0);
    glColor3f(0.5f, 0.0f, 0.0f);
    glScalef(28.0, 25.0, 55.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //pembatas
    glPushMatrix();
    glTranslatef(-80.0, 22.0, 40.5);
    glColor3f(0.3f, 0.3f, 0.3f);
    glScalef(60.0, 2.0, 60.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //gedung2
    glPushMatrix();
    glTranslatef(-80.0, 30.0, 40.5);
    glColor3f(0.5f, 0.0f, 0.0f);
    glScalef(20.8, 40.0, 50.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-80.0, 30.0, 40.5);
    glRotatef(90,0,1,0);
    glColor3f(0.5f, 0.0f, 0.0f);
    glScalef(20.8, 40.0, 50.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-80.0, 30.0, 40.5);
    glRotatef(45,0,1,0);
    glColor3f(0.5f, 0.0f, 0.0f);
    glScalef(20.8, 40.0, 50.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-80.0, 30.0, 40.5);
    glRotatef(135,0,1,0);
    glColor3f(0.5f, 0.0f, 0.0f);
    glScalef(20.8, 40.0, 50.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //gedung3
    glPushMatrix();
    glTranslatef(-80.0, 70.0, 40.5);
    glColor3f(0.5f, 0.0f, 0.0f);
    glScalef(16.5, 50.0, 40.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-80.0, 70.0, 40.5);
    glRotatef(90,0,1,0);
    glColor3f(0.5f, 0.0f, 0.0f);
    glScalef(16.5, 50.0, 40.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-80.0, 70.0, 40.5);
    glRotatef(45,0,1,0);
    glColor3f(0.5f, 0.0f, 0.0f);
    glScalef(16.5, 50.0, 40.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-80.0, 70.0, 40.5);
    glRotatef(135,0,1,0);
    glColor3f(0.5f, 0.0f, 0.0f);
    glScalef(16.5, 50.0, 40.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //gedungatas
    glPushMatrix();
    glTranslatef(-80.0, 95.0, 40.5);
    glRotatef(90,0,1,0);
    glRotatef(45,0,0,1);
    glColor3f(0.4f, 0.2f, 0.0f);
    glScalef(14.0, 14.0, 39.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-80.0, 94.0, 48.5);
    glRotatef(30,1,0,0);
    glColor3f(0.4f, 0.2f, 0.0f);
    glScalef(16.5, 15.0, 20.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-80.0, 94.0, 32.5);
    glRotatef(150,1,0,0);
    glColor3f(0.4f, 0.2f, 0.0f);
    glScalef(16.5, 15.0, 20.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //yang nempel ke baling
    glPushMatrix();
    glTranslatef(-55.0, 95.0, 41.5);
    glColor3f(0.9f, 0.8f, 0.8f);
    glScalef(15.0, 4.0, 4.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-55.0, 95.0, 41.5);
    glRotatef(45,1,0,0);
    glColor3f(0.9f, 0.8f, 0.8f);
    glScalef(15.0, 4.0, 4.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //pintu
    glPushMatrix();
    glTranslatef(-61.0, 5, 59.5);
    glRotatef(45,0,1,0);
    glColor3f(0.8f, 0.4f, 0.2f);
    glScalef(7.0, 11.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-61.0, 5, 59.5);
    glRotatef(45,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(9.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //jendela lantai 2
    glPushMatrix();
    glTranslatef(-63.0, 35, 58.0);
    glRotatef(45,0,1,0);
    glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(7.0, 8.0, 1.7);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-63.0, 35, 58.0);
    glRotatef(45,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(9.0, 10.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-63.0, 35, 58.0);
    glRotatef(45,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(1.0, 10.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-63.0, 35, 58.0);
    glRotatef(45,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(9.0, 1.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-63.0, 35, 23.0);
    glRotatef(135,0,1,0);
    glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(7.0, 8.0, 1.7);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-63.0, 35, 23.0);
    glRotatef(135,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(9.0, 10.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-63.0, 35, 23.0);
    glRotatef(135,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(1.0, 10.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-63.0, 35, 23.0);
    glRotatef(135,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(9.0, 1.0, 2.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //jendela atas
    glPushMatrix();
    glTranslatef(-60.5, 80, 40.5);
    glRotatef(90,0,1,0);
    glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(7.0, 7.0, 1.7);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-60.5, 80, 40.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(9.0, 9.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-60.5, 84.0, 40.5);
    glRotatef(90,0,1,0);
    glRotatef(45,0,0,1);
    glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(5.0, 5.0, 1.7);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-60.5, 84.0, 40.5);
    glRotatef(90,0,1,0);
    glRotatef(45,0,0,1);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(7.0, 7.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-60.5, 80.0, 40.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(1.0, 9.0, 2.1);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-60.5, 84, 40.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(9.0, 1.0, 2.1);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-60.5, 80, 40.5);
    glRotatef(90,0,1,0);
    glColor3f(0.7f, 0.3f, 0.1f);
    glScalef(9.0, 1.0, 2.1);
    glutSolidCube(1.0f);
    glPopMatrix();
}

void baling()
{
    glPushMatrix();
    glTranslatef(-50.0, 95.0, 41.5);
    glRotatef(sudutnya,1,0,0);
    glColor3f(0.8f, 0.0f, 0.8f);
    glScalef(2.5, 100.0, 2.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-50.0, 95.0, 41.5);
    glRotatef(sudutnya,1,0,0);
    glTranslatef(0.0, 30.0, -3.0);
    glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(1.5, 40.0, 7.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-50.0, 95.0, 41.5);
    glRotatef(sudutnya,1,0,0);
    glTranslatef(0.0, -30.0, 3.0);
    glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(1.5, 40.0, 7.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-50.0, 95.0, 41.5);
    glRotatef(sudutnya,1,0,0);
    glColor3f(0.8f, 0.0f, 0.8f);
    glScalef(2.5, 2.5, 100.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-50.0, 95.0, 41.5);
    glRotatef(sudutnya,1,0,0);
    glTranslatef(0.0, 3.0, 30.0);
    glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(1.5, 7.5, 40.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-50.0, 95.0, 41.5);
    glRotatef(sudutnya,1,0,0);
    glTranslatef(0.0, -3.0, -30.0);
    glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(1.5, 7.5, 40.0);
    glutSolidCube(1.0f);
    glPopMatrix();
}

void putar(int value)
{
    sudutnya += 5;
    if (sudutnya > 360){
        sudutnya -= 360;
    }

    glutPostRedisplay();
    glutTimerFunc(25, putar, 0);
}

void jembatan()
{
     //papan samping
    glPushMatrix();
    glTranslatef(97.0, -8.5, -100.0);
    glRotatef(160,0,0,1);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(60.0, 2.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(44.0, 1.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(55.0, 2.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-10.0, -8.5, -100.0);
    glRotatef(20,0,0,1);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(60.0, 2.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(97.0, -8.5, -140.0);
    glRotatef(160,0,0,1);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(60.0, 2.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(44.0, 1.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(55.0, 2.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-10.0, -8.5, -140.0);
    glRotatef(20,0,0,1);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(60.0, 2.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //papan bawah
   // glPushMatrix();
    //glTranslatef(97.0, -7.5, -120.0);
    //glRotatef(160,0,0,1);
    //glColor3f(0.3f, 0.3f, 0.7f);
    //glScalef(60.0, 2.0, 40.5);
    //glutSolidCube(1.0f);
    //glPopMatrix();
    
   // glPushMatrix();
    //glTranslatef(44.0, 2.5, -120.0);
    //glColor3f(0.3f, 0.3f, 0.7f);
    //glScalef(55.0, 2.0, 40.5);
   // glutSolidCube(1.0f);
   // glPopMatrix();
    
    //glPushMatrix();
  //  glTranslatef(-10.0, -7.5, -120.0);
    //glRotatef(20,0,0,1);
    //glColor3f(0.3f, 0.3f, 0.7f);
    //glScalef(60.0, 2.0, 40.5);
    //glutSolidCube(1.0f);
    //glPopMatrix();
    
    //railing tengah
    glPushMatrix();
    glTranslatef(85.0, 2.5, -100.0);
    glRotatef(160,0,0,1);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(35.0, 1.0, 1.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(44.0, 7.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(55.0, 1.0, 1.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(2.0, 2.5, -100.0);
    glRotatef(20,0,0,1);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(35.0, 1.0, 1.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(85.0, 2.5, -140.0);
    glRotatef(160,0,0,1);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(35.0, 1.0, 1.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(44.0, 7.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(55.0, 1.0, 1.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(2.0, 2.5, -140.0);
    glRotatef(20,0,0,1);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(35.0, 1.0, 1.0);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //railing atas
    glPushMatrix();
    glTranslatef(85.0, 7.5, -100.0);
    glRotatef(160,0,0,1);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(35.0, 2.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(44.0, 13.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(55.0, 2.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(2.0, 7.5, -100.0);
    glRotatef(20,0,0,1);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(35.0, 2.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(85.0, 7.5, -140.0);
    glRotatef(160,0,0,1);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(35.0, 2.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(44.0, 13.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(55.0, 2.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(2.0, 7.5, -140.0);
    glRotatef(20,0,0,1);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(35.0, 2.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    //railing tegak
    glPushMatrix();
    glTranslatef(101.0, -4.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 15.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(90.0, -0.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(80.0, 3.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(70.0, 7.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(57.0, 7.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(44.0, 7.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(31.0, 7.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(18.0, 7.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-13.0, -4.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 15.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-3.0, -0.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(7.0, 3.5, -100.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    
    glPushMatrix();
    glTranslatef(101.0, -4.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 15.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(90.0, -0.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(80.0, 3.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(70.0, 7.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(57.0, 7.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(44.0, 7.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(31.0, 7.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(18.0, 7.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-13.0, -4.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 15.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-3.0, -0.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(7.0, 3.5, -140.0);
    glColor3f(0.7f, 0.7f, 0.7f);
    glScalef(2.0, 13.0, 1.5);
    glutSolidCube(1.0f);
    glPopMatrix();
}

void jalan()
{
    glPushMatrix();
    glTranslatef(195.0, -2.5, 170.0);
	glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(5.0, 5.0, 50.5);
    glutSolidCube(0.5f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(195.0, -2.5, 120.0);
	glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(5.0, 5.0, 50.5);
    glutSolidCube(0.5f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(195.0, -2.5, 70.0);
	glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(5.0, 5.0, 50.5);
    glutSolidCube(0.5f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(195.0, -2.5, 20.0);
	glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(5.0, 5.0, 50.5);
    glutSolidCube(0.5f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(195.0, -2.5, -30.0);
	glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(5.0, 5.0, 50.5);
    glutSolidCube(0.5f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(195.0, -2.5, -80.0);
	glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(5.0, 5.0, 50.5);
    glutSolidCube(0.5f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(195.0, -2.5, -130.0);
	glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(5.0, 5.0, 50.5);
    glutSolidCube(0.5f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(195.0, -2.5, -180.0);
	glColor3f(0.9f, 0.9f, 0.9f);
    glScalef(5.0, 5.0, 50.5);
    glutSolidCube(0.5f);
    glPopMatrix();
}

void pohon()
{
     //pohon deket jalan
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(235.0, -2.5, 170.0);
    glRotatef(270, 1, 0, 0);
	glScaled(4,4,35);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(235.0, 25.5, 170.0);
	glScaled(20,20,20);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(235.0, -2.5, 130.0);
    glRotatef(270, 1, 0, 0);
	glScaled(4,4,35);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(235.0, 25.5, 130.0);
	glScaled(20,20,20);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(235.0, -2.5, 90.0);
    glRotatef(270, 1, 0, 0);
	glScaled(4,4,35);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(235.0, 25.5, 90.0);
	glScaled(20,20,20);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(235.0, -2.5, 50.0);
    glRotatef(270, 1, 0, 0);
	glScaled(4,4,35);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(235.0, 25.5, 50.0);
	glScaled(20,20,20);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(235.0, -2.5, 10.0);
    glRotatef(270, 1, 0, 0);
	glScaled(4,4,35);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(235.0, 25.5, 10.0);
	glScaled(20,20,20);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(235.0, -2.5, -30.0);
    glRotatef(270, 1, 0, 0);
	glScaled(4,4,35);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(235.0, 25.5, -30.0);
	glScaled(20,20,20);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(235.0, -2.5, -70.0);
    glRotatef(270, 1, 0, 0);
	glScaled(4,4,35);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(235.0, 25.5, -70.0);
	glScaled(20,20,20);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(235.0, -2.5, -110.0);
    glRotatef(270, 1, 0, 0);
	glScaled(4,4,35);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(235.0, 25.5, -110.0);
	glScaled(20,20,20);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(235.0, -2.5, -150.0);
    glRotatef(270, 1, 0, 0);
	glScaled(4,4,35);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(235.0, 25.5, -150.0);
	glScaled(20,20,20);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	//pohon deket sungai
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(130.0, -2.5, 120.0);
    glRotatef(270, 1, 0, 0);
	glScaled(4,4,35);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(130.0, 25.5, 120.0);
	glScaled(20,20,20);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(135.0, -2.5, 130.0);
    glRotatef(270, 1, 0, 0);
	glScaled(2,2,17);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(135.0, 12.5, 130.0);
	glScaled(10,10,10);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(145.0, -2.5, 125.0);
    glRotatef(270, 1, 0, 0);
	glScaled(3,3,25);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(145.0, 17.0, 125.0);
	glScaled(15,15,15);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(110.0, -2.5, -170.0);
    glRotatef(270, 1, 0, 0);
	glScaled(4,4,35);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(110.0, 25.5, -170.0);
	glScaled(20,20,20);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(105.0, -2.5, -160.0);
    glRotatef(270, 1, 0, 0);
	glScaled(2,2,17);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(105.0, 12.5, -160.0);
	glScaled(10,10,10);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	//pohon samping rumah
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(-60.0, -2.5, -160.0);
    glRotatef(270, 1, 0, 0);
	glScaled(4,4,35);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(-60.0, 25.5, -160.0);
	glScaled(20,20,20);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(-55.0, -2.5, -150.0);
    glRotatef(270, 1, 0, 0);
	glScaled(2,2,17);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(-55.0, 12.5, -150.0);
	glScaled(10,10,10);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.4f, 0.2f, 0.0f);
    glTranslatef(-45.0, -2.5, -155.0);
    glRotatef(270, 1, 0, 0);
	glScaled(3,3,25);
	glutSolidCone(0.5,1,5,3);
	glPopMatrix();
    
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(-45.0, 17.0, -155.0);
	glScaled(15,15,15);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
}

void batu()
{
    glPushMatrix();
    glColor3f(0.5f, 0.5f, 0.5f);
    glTranslatef(125.0, -4.5, 100.0);
	glScaled(10,10,10);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
	
	glPushMatrix();
    glColor3f(0.5f, 0.5f, 0.5f);
    glTranslatef(135.0, -2.5, 105.0);
	glScaled(5,5,5);
	glutSolidSphere(0.5,8,5);
	glPopMatrix();
}

void display(void) {
	glClearStencil(0); //clear the stencil buffer
	glClearDepth(1.0f);
	glClearColor(0.0, 0.6, 0.8, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); //clear the buffers
	glLoadIdentity();
	gluLookAt(viewx, viewy, viewz, 0.0, 0.0, 5.0, 0.0, 1.0, 0.0);

	glPushMatrix();
	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawSceneTanah(_terrain, 0.3f, 0.9f, 0.0f);
	glPopMatrix();

	glPushMatrix();

	drawSceneTanah(_terrainTanah, 0.4f, 0.4f, 0.4f);
	glPopMatrix();

	glPushMatrix();
	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawSceneTanah(_terrainAir, 0.0f, 0.2f, 0.5f);
	glPopMatrix();
	
    //teksture jalan
	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[0]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex3d(170,-1,190);
    glTexCoord2f(1.0, 0.0);
    glVertex3d(220,-1,190);
    glTexCoord2f(1.0, 5.0);
    glVertex3d(220,-1,-190);
    glTexCoord2f(0.0, 5.0);
    glVertex3d(170,-1,-190);
    glEnd();
    glPopMatrix();
    
    //teksture tembok
    //depan
   	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[2]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex3d(-40,15,-30);
    glTexCoord2f(1.0, 0.0);
    glVertex3d(-40,-10,-30);
    glTexCoord2f(1.0, 3.0);
    glVertex3d(-40,-10,-131);
    glTexCoord2f(0.0, 3.0);
    glVertex3d(-40,15,-131);
    glEnd();
    glPopMatrix();
     //samping
   	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[2]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex3d(-40,-10,-131);
    glTexCoord2f(2.0, 0.0);
    glVertex3d(-120,-10,-131);
    glTexCoord2f(2.0, 1.0);
    glVertex3d(-120,15,-131);
    glTexCoord2f(0.0, 1.0);
    glVertex3d(-40,15,-131);
    glEnd();
    glPopMatrix();
   	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[2]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex3d(-40,-10,-30);
    glTexCoord2f(2.0, 0.0);
    glVertex3d(-40,15,-30);
    glTexCoord2f(1.0, 2.0);
    glVertex3d(-120,15,-30);
    glTexCoord2f(0.0, 2.0);
    glVertex3d(-120,-10,-30);
    glEnd();
    glPopMatrix();
    
     //belakang
   	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[2]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex3d(-120,-10,-30);
    glTexCoord2f(1.0, 0.0);
    glVertex3d(-120,15,-30);
    glTexCoord2f(1.0, 3.0);
    glVertex3d(-120,15,-131);
    glTexCoord2f(0.0, 3.0);
    glVertex3d(-120,-10,-131);
    glEnd();
    glPopMatrix();
    
    //teksture Sungai
   	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[1]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex3d(-10,-10,190);
    glTexCoord2f(1.0, 0.0);
    glVertex3d(115,-10,190);
    glTexCoord2f(1.0, 5.0);
    glVertex3d(115,-10,-190);
    glTexCoord2f(0.0, 5.0);
    glVertex3d(-10,-10,-190);
    glEnd();
    glPopMatrix();
    
    //teksture jembatan
    glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[3]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex3d(17,2,-100);
    glTexCoord2f(1.0, 0.0);
    glVertex3d(70,2,-100);
    glTexCoord2f(1.0, 1.0);
    glVertex3d(70,2,-140);
    glTexCoord2f(0.0, 1.0);
    glVertex3d(17,2,-140);
    glEnd();
    glPopMatrix();
 
     glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[3]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex3d(-15,-10,-100);
    glTexCoord2f(1.0, 0.0);
    glVertex3d(17,2,-100);
    glTexCoord2f(1.0, 1.0);
    glVertex3d(17,2,-140);
    glTexCoord2f(0.0, 1.0);
    glVertex3d(-15,-10,-140);
    glEnd();
    glPopMatrix();

     glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[3]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex3d(70,2,-100);
    glTexCoord2f(1.0, 0.0);
    glVertex3d(102,-10,-100);
    glTexCoord2f(1.0, 1.0);
    glVertex3d(102,-10,-140);
    glTexCoord2f(0.0, 1.0);
    glVertex3d(70,2,-140);
    glEnd();
    glPopMatrix();
        
    
	jembatan();
    papan();
	rumah();
	gedungkincir();
	baling();
	jalan();
	pohon();
	batu();

	glutSwapBuffers();
	glFlush();
	rot++;
	angle++;

}

void init(void) {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glDepthFunc(GL_LESS);
	glEnable(GL_NORMALIZE);
	glEnable(GL_COLOR_MATERIAL);
	glDepthFunc(GL_LEQUAL);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_CULL_FACE);

	_terrain = loadTerrain("heightmap.bmp", 20);
	_terrainTanah = loadTerrain("heightmapTanah.bmp", 20);
	_terrainAir = loadTerrain("heightmapAir.bmp", 20);

	//binding texture
	Image* image1= loadBMP("jalan.bmp");
		if (image1 == NULL) {
        printf("Image was not returned from loadTexture\n");
        exit(0);
        }
        Image* image2= loadBMP("sungai.bmp");
		if (image2 == NULL) {
        printf("Image was not returned from loadTexture\n");
        exit(0);
        }
        Image* image3= loadBMP("tembok.bmp");
		if (image3 == NULL) {
        printf("Image was not returned from loadTexture\n");
        exit(0);
        }
        Image* image4= loadBMP("papan.bmp");
		if (image4 == NULL) {
        printf("Image was not returned from loadTexture\n");
        exit(0);
        }
	glGenTextures(5,texture);

//binding texture untuk membuat texture 2D
glBindTexture(GL_TEXTURE_2D, texture[0]);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexImage2D(GL_TEXTURE_2D, 0, 3, image1->width, image1->height, 0, GL_RGB,
GL_UNSIGNED_BYTE, image1->pixels);

glBindTexture(GL_TEXTURE_2D, texture[1]);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexImage2D(GL_TEXTURE_2D, 0, 3, image2->width, image2->height, 0, GL_RGB,
GL_UNSIGNED_BYTE, image2->pixels);

glBindTexture(GL_TEXTURE_2D, texture[2]);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexImage2D(GL_TEXTURE_2D, 0, 3, image3->width, image3->height, 0, GL_RGB,
GL_UNSIGNED_BYTE, image3->pixels);

glBindTexture(GL_TEXTURE_2D, texture[3]);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexImage2D(GL_TEXTURE_2D, 0, 3, image4->width, image4->height, 0, GL_RGB,
GL_UNSIGNED_BYTE, image4->pixels);

glEnable(GL_TEXTURE_2D);
glShadeModel(GL_FLAT);
}

static void kibor(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_HOME:
		viewy+=10;
		break;
	case GLUT_KEY_END:
		viewy-=10;
		break;
	case GLUT_KEY_UP:
		viewz-=10;
		break;
	case GLUT_KEY_DOWN:
		viewz+=10;
		break;

	case GLUT_KEY_RIGHT:
		viewx+=10;
		break;
	case GLUT_KEY_LEFT:
		viewx-=10;
		break;

	case GLUT_KEY_F1: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	case GLUT_KEY_F2: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient2);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse2);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	default:
		break;
	}
}

void keyboard(unsigned char key, int x, int y) {
	if (key == 'd') {

		spin = spin - 1;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'a') {
		spin = spin + 1;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'q') {
		viewz += 10;
	}
	if (key == 'e') {
		viewz-=10;
	}
	if (key == 's') {
		viewy-=10;
	}
	if (key == 'w') {
		viewy+=10;
	}
}

void reshape(int w, int h) {
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (GLfloat) w / (GLfloat) h, 0.1, 1000.0);
	glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_STENCIL | GLUT_DEPTH); //add a stencil buffer to the window
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Kincir Angin");
	init();

	glutDisplayFunc(display);
	glutIdleFunc(display);
	glutReshapeFunc(reshape);
	glutSpecialFunc(kibor);

	glutKeyboardFunc(keyboard);

	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);

	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);
	glColorMaterial(GL_FRONT, GL_DIFFUSE);

    glutTimerFunc(25, putar, 0);
	glutMainLoop();
	return 0;
}
