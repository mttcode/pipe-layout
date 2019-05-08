///////////////////////////////////////////////////////////////////////////////
// main.cpp
// ==========
// Pipe Layout
//
// AUTOR: Martin Timko
//
// Copyright(c) 2011-2012
///////////////////////////////////////////////////////////////////////////////


#include <gl/glew.h>
#include <gl/glut.h>
#include <opencsg.h>
#include "displaylistPrimitive.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <windows.h>

#define PI 3.14159265f


std::vector<OpenCSG::Primitive*> prim;
std::vector<OpenCSG::Primitive*> prim_pipe[100];

GLuint id_prim[100];

float r;
float xpos, ypos, zpos, xrot, yrot;
float stat_xpos, stat_ypos, stat_zpos, stat_xrot, stat_yrot;

int lastx, lasty, countCones, snapCounter;
float clr_grid_R, clr_grid_G, clr_grid_B;
float d_prim;
float dx, dy, dz, alfa_1, alfa_2;
float x_1f[100], y_1f[100], z_1f[100], r_1f[100], x_2f[100], y_2f[100], z_2f[100], r_2f[100];

int windowWidth, windowHeight;
int mousePosX, mousePosY;
int windowPosX, windowPosY;

bool csg_on_all, csg_on, grid_on, cross_on, camera_on, hud_1_on, hud_2_on;

char errLogTexture[500];
char errLogSnapshot[500];


enum 
{ 
	EXIT,
	ALGO_AUTOMATIC, GF_STANDARD, GF_DC, GF_OQ, SCS_STANDARD, SCS_DC, SCS_OQ
};



///////////////////////////////////////////////////////////////////////////////
// Hlavicka grafickeho formatu TGA
///////////////////////////////////////////////////////////////////////////////
#pragma pack(push,1)
struct TGAHeader
{
	unsigned char m_iIdentificationFieldSize;
	unsigned char m_iColourMapType;
	unsigned char m_iImageTypeCode;
	unsigned short m_iColorMapOrigin;
	unsigned short m_iColorMapLength;
	unsigned char m_iColourMapEntrySize;
	unsigned short m_iX_Origin;
	unsigned short m_iY_Origin;
	unsigned short m_iWidth;
	unsigned short m_iHeight;
	unsigned char m_iBPP;
	unsigned char m_ImageDescriptorByte;
};
#pragma pack(pop)



///////////////////////////////////////////////////////////////////////////////
// Vytvorenie suboru TGA
///////////////////////////////////////////////////////////////////////////////
bool writeTga(const char* filename, const unsigned w, const unsigned h, const unsigned bpp, const unsigned char* pixels) 
{
	bool rgba = false;

	switch(bpp)
	{
	case 4: case 32:
		rgba = true;
		break;

	case 3: case 24:
		rgba = false;
		break;

	default:
		return false;
	}

	FILE* fp = fopen(filename, "wb");

	if(!fp) 
	{
		return false;
	}

	TGAHeader header;

	memset(&header,0, sizeof(TGAHeader));

	header.m_iImageTypeCode = 2;

	header.m_iWidth = w;
	header.m_iHeight = h;

	header.m_iBPP = rgba ? 32 : 24;

	fwrite(&header, sizeof(TGAHeader), 1, fp);

	unsigned int total_size = w * h * (3 + (rgba ? 1:0));
	unsigned int this_pixel_start = 0;

	for( ; this_pixel_start != total_size; this_pixel_start += 3 ) 
	{
		const unsigned char* pixel = pixels + this_pixel_start;

		fputc(pixel[2], fp);
		fputc(pixel[1], fp);
		fputc(pixel[0], fp);

		if (rgba) 
		{
			fputc(pixel[3], fp);
			++this_pixel_start;
		}
	}

	fclose(fp);

	return true;		  
}



///////////////////////////////////////////////////////////////////////////////
// Prevod frame bufera na obrazok TGA
///////////////////////////////////////////////////////////////////////////////
bool saveScreenGrab(const char* filename) 
{
	unsigned sw = glutGet(GLUT_WINDOW_WIDTH);
	unsigned sh = glutGet(GLUT_WINDOW_HEIGHT);
	unsigned bpp = glutGet(GLUT_WINDOW_RGBA) ? 4 : 3;
	GLenum format = (bpp == 4) ? GL_RGBA : GL_RGB;

	unsigned char* pdata = new unsigned char[sw*sh*bpp];

	glReadBuffer(GL_FRONT);
	glReadPixels(0, 0, sw, sh, format, GL_UNSIGNED_BYTE, pdata);

	bool ret = writeTga(filename, sw, sh, bpp, pdata);

	delete [] pdata;

	return ret;
}



///////////////////////////////////////////////////////////////////////////////
// Vymazanie poloziek zobrazovacieho zoznamu zakladnych telies
///////////////////////////////////////////////////////////////////////////////
void clearPrimitives(void)
{
	for (std::vector<OpenCSG::Primitive*>::const_iterator d = prim.begin(); d != prim.end(); ++d) 
	{
		OpenCSG::DisplayListPrimitive* p = static_cast<OpenCSG::DisplayListPrimitive*>(*d);
		glDeleteLists(1, p->getDisplayListId());
		delete p;
	}
	prim.clear();
}


/*
///////////////////////////////////////////////////////////////////////////////
// Vymazanie poloziek zobrazovacieho zoznamu skopirovanych telies
///////////////////////////////////////////////////////////////////////////////
void clearPrimitivesPipe(void)
{
	for (std::vector<OpenCSG::Primitive*>::const_iterator d = prim_pipe.begin(); d != prim_pipe.end(); ++d) 
	{
		OpenCSG::DisplayListPrimitive* p = static_cast<OpenCSG::DisplayListPrimitive*>(*d);
		glDeleteLists(1, p->getDisplayListId());
		delete p;
	}
	prim_pipe.clear();
}

*/

///////////////////////////////////////////////////////////////////////////////
// Prevod RGB hodnoty na jednotkovu hodnotu
///////////////////////////////////////////////////////////////////////////////
float convertRGB(int rgb)
{
	return (1/(float)255) * rgb;
}



///////////////////////////////////////////////////////////////////////////////
// Struktura pre nacitanie textury
///////////////////////////////////////////////////////////////////////////////
struct Image
{
	unsigned long sizeX;
	unsigned long sizeY;
	char *data;
};

typedef struct Image Image;



///////////////////////////////////////////////////////////////////////////////
// Nacitanie textury
///////////////////////////////////////////////////////////////////////////////
int imageLoad(char *filename, Image *image)
{
	FILE *file;
	unsigned long size, i;
	unsigned short int planes;
	unsigned short int bpp;
	char temp;

	if((file = fopen(filename, "rb")) == NULL)
	{
		sprintf(errLogTexture, "File not found.\n");
		return 0;
	}

	fseek(file, 18, SEEK_CUR);

	if((i = fread(&image->sizeX, 4, 1, file)) != 1)
	{
		sprintf(errLogTexture, "Error reading width.\n");
		return 0;
	}

	if((i = fread(&image->sizeY, 4, 1, file)) != 1)
	{
		sprintf(errLogTexture, "Error reading height.\n");
		return 0;
	}

	size = image->sizeX * image->sizeY * 3;


	if((fread(&planes, 2, 1, file)) != 1)
	{
		sprintf(errLogTexture, "Error reading planes.\n");
		return 0;
	}

	if(planes != 1)
	{
		sprintf(errLogTexture, "Planes from %s is not 1: [%d]\n", filename, planes);
		return 0;
	}

	if((i = fread(&bpp, 2, 1, file)) != 1)
	{
		sprintf(errLogTexture, "Error reading bpp.\n");
		return 0;
	}

	if(bpp != 24)
	{
		sprintf(errLogTexture, "Bpp from %s is not 24: [%d]\n", filename, bpp);
		return 0;
	}

	fseek(file, 24, SEEK_CUR);

	image->data = (char *) malloc(size);

	if(image->data == NULL)
	{
		sprintf(errLogTexture, "Error allocating memory for rgb image.\n");
		return 0;
	}

	if((i = fread(image->data, size, 1, file)) != 1)
	{
		sprintf(errLogTexture, "Error reading image data.\n");
		return 0;
	}

	for(i = 0; i < size; i += 3)
	{
		temp  = image->data[i];
		image->data[i] = image->data[i+2];
		image->data[i+2] = temp;
	}

	return 1;
}



///////////////////////////////////////////////////////////////////////////////
// Nacitanie textury zo suboru
///////////////////////////////////////////////////////////////////////////////
Image *loadTexture(char *texture_name)
{
	Image *img;

	img = (Image *) malloc(sizeof(Image));

	if(img == NULL)
	{
		sprintf(errLogTexture, "Error allocating space for image.\n");
	}

	char texChar[50];
	sprintf(texChar, ".\\textures\\%s", texture_name);

	if(!imageLoad( texChar, img))
	{
		sprintf(errLogTexture, "Error load texture.\n");		
	}

	FILE *fw;

	fw = fopen(".\\textures\\load_tex.log", "w");

	if(strlen(errLogTexture) == 0)
	{
		fprintf(fw, "- no errors -");
	}
	else
	{                    
		fprintf(fw, "%s", errLogTexture);
	}

	fclose(fw);

	return img;
}



///////////////////////////////////////////////////////////////////////////////
// Vytvorenie objektu kuzela alebo valca
///////////////////////////////////////////////////////////////////////////////
void Cone(GLdouble radius_1, GLdouble radius_2, GLdouble height, GLint slices, GLint stacks) 
{
	GLUquadricObj* qobj = gluNewQuadric();

	glRotatef(-90, 1.0, 0.0, 0.0);

	gluQuadricDrawStyle(qobj, GLU_FILL);
	gluQuadricNormals(qobj, GLU_SMOOTH);
	gluQuadricTexture(qobj, GLU_TRUE);

	gluCylinder(qobj, radius_1, radius_2, height, slices, stacks);

	glScalef(-1.0, 1.0, -1.0);
	gluDisk(qobj, 0.0, radius_1, slices, stacks);
	glScalef(-1.0, 1.0, -1.0);
	glTranslatef(0.0, 0.0, height);
	gluDisk(qobj, 0.0, radius_2, slices, stacks);

	gluDeleteQuadric(qobj);    
}



///////////////////////////////////////////////////////////////////////////////
// Vytvorenie predlzeneho objektu kuzela alebo valca
///////////////////////////////////////////////////////////////////////////////
void ConePlus(GLdouble radius_1, GLdouble radius_2, GLdouble height, GLint slices, GLint stacks) 
{
	GLUquadricObj* qobj = gluNewQuadric();

	glTranslatef(0.0, -0.5,0.0);
	glRotatef(-90, 1.0, 0.0, 0.0);

	gluCylinder(qobj, radius_1, radius_2, height, slices, stacks);

	gluQuadricDrawStyle(qobj, GLU_FILL);
	gluQuadricNormals(qobj, GLU_SMOOTH);

	glScalef(-1.0, 1.0, -1.0);
	gluDisk(qobj, 0.0, radius_1, slices, stacks);
	glScalef(-1.0, 1.0, -1.0);
	glTranslatef(0.0, 0.0, height);
	gluDisk(qobj, 0.0, radius_2, slices, stacks);

	gluDeleteQuadric(qobj);    
}



///////////////////////////////////////////////////////////////////////////////
// Umiestnenie predlzeneho objektu kuzela alebo valca v priestore
///////////////////////////////////////////////////////////////////////////////
void setConePlus(int index, float x_1, float z_1, float y_1, float r_1, float x_2, float z_2, float y_2, float r_2)
{
	d_prim = sqrt( (x_2 - x_1)*(x_2 - x_1) + (y_2 - y_1)*(y_2 - y_1) + (z_2 - z_1)*(z_2 - z_1) ) + 1;

	id_prim[index] = glGenLists(1);
	glNewList(id_prim[index], GL_COMPILE);

	dx = x_2 - x_1;
	dy = y_2 - y_1;
	dz = z_2 - z_1;

	if(dz < 0)
	{
		float zz = dz*(-1);
		dz = zz;
	}

	if(dy < 0)
	{
		float yy = dy*(-1);
		dy = yy;
	}

	if(dx < 0)
	{
		float xx = dx*(-1);
		dx = xx;
	}


	if(x_1 == x_2)
	{
		alfa_1 = 0;

		glPushMatrix();

		glTranslatef(x_1, y_1, z_1);


		if(z_2 < z_1 && dy != 0)
		{
			alfa_2 = 360 - ((atan(dz / dy))*180)/PI;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			ConePlus(r_1, r_2, d_prim, 20, 20); 
		}
		else if(z_2 > z_1 && dy != 0)
		{
			alfa_2 = ((atan(dz / dy))*180)/PI;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			ConePlus(r_1, r_2, d_prim, 20, 20);    
		}
		else if(z_2 == z_1)
		{
			alfa_2 = 0;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			ConePlus(r_1, r_2, d_prim, 20, 20);  
		}
		else if(z_1 > z_2 && dy == 0)
		{
			alfa_2 = -90;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			ConePlus(r_1, r_2, d_prim, 20, 20);    
		}
		else if(z_1 < z_2 && dy == 0)
		{
			alfa_2 = 90;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			ConePlus(r_1, r_2, d_prim, 20, 20);    
		}

	}
	else if(z_1 == z_2)
	{
		alfa_2 = 0;

		glPushMatrix();

		glTranslatef(x_1, y_1, z_1);

		if(x_2 < x_1 && dy != 0)
		{
			alfa_1 = ((atan(dx / dy))*180)/PI;

			glRotatef(alfa_1, 0.0, 0.0, 1.0);
			ConePlus(r_1, r_2, d_prim, 20, 20); 
		}
		else if(x_2 > x_1 && dy != 0)
		{
			alfa_1 = 360 - ((atan(dx / dy))*180)/PI;

			glRotatef(alfa_1, 0.0, 0.0, 1.0);
			ConePlus(r_1, r_2, d_prim, 20, 20);  
		}
		else if(x_2 == x_1)
		{
			alfa_2 = 0;

			glRotatef(alfa_2, 0.0, 0.0, 1.0);
			ConePlus(r_1, r_2, d_prim, 20, 20);     
		}
		else if(x_1 > x_2 && dy == 0)
		{
			alfa_1 = 90;

			glRotatef(alfa_1, 0.0, 0.0, 1.0);
			ConePlus(r_1, r_2, d_prim, 20, 20);     
		}
		else if(x_1 < x_2 && dy == 0)
		{
			alfa_1 = -90;

			glRotatef(alfa_1, 0.0, 0.0, 1.0);
			ConePlus(r_1, r_2, d_prim, 20, 20);     
		}

	}
	else if(y_1 == y_2)
	{

		glPushMatrix();

		glTranslatef(x_1, y_1, z_1);

		if(x_2 < x_1)
		{
			float alfa_1_1 = 90;
			alfa_1 = -((atan(dz / dx))*180)/PI;

			glRotatef(alfa_1, 0.0, 1.0, 0.0);
			glRotatef(alfa_1_1, 0.0, 0.0, 1.0);

			ConePlus(r_1, r_2, d_prim, 20, 20); 
		}
		else if(x_2 > x_1)
		{
			float alfa_1_1 = -90;
			alfa_1 = -((atan(dz / dx))*180)/PI;

			glRotatef(alfa_1, 0.0, 1.0, 0.0);
			glRotatef(alfa_1_1, 0.0, 0.0, 1.0);

			ConePlus(r_1, r_2, d_prim, 20, 20); 
		}

	}
	else
	{

		glPushMatrix();

		glTranslatef(x_1, y_1, z_1);


		if(x_2 < x_1)
		{
			alfa_1 = ((atan(dx / dy))*180)/PI;

			glRotatef(alfa_1, 0.0, 0.0, 1.0);
		}    
		else if(x_2 > x_1)
		{
			alfa_1 = -((atan(dx / dy))*180)/PI;

			glRotatef(alfa_1, 0.0, 0.0, 1.0);    
		}


		if(z_2 < z_1)
		{
			alfa_2 = -((atan(dz / dy))*180)/PI;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			ConePlus(r_1, r_2, d_prim, 20, 20);  
		}    
		else if(z_2 > z_1)
		{
			alfa_2 = ((atan(dz / dy))*180)/PI;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			ConePlus(r_1, r_2, d_prim, 20, 20);  
		}

	}

	glPopMatrix();
	glEndList();

}



///////////////////////////////////////////////////////////////////////////////
// Umiestnenie objektu kuzela alebo valca v priestore
///////////////////////////////////////////////////////////////////////////////
void setCone(int index, float x_1, float z_1, float y_1, float r_1, float x_2, float z_2, float y_2, float r_2)
{
	d_prim = sqrt( (x_2 - x_1)*(x_2 - x_1) + (y_2 - y_1)*(y_2 - y_1) + (z_2 - z_1)*(z_2 - z_1) );

	id_prim[index] = glGenLists(1);
	glNewList(id_prim[index], GL_COMPILE);

	dx = x_2 - x_1;
	dy = y_2 - y_1;
	dz = z_2 - z_1;

	if(dz < 0)
	{
		float zz = dz*(-1);
		dz = zz;
	}

	if(dy < 0)
	{
		float yy = dy*(-1);
		dy = yy;
	}

	if(dx < 0)
	{
		float xx = dx*(-1);
		dx = xx;
	}


	if(x_1 == x_2)
	{
		alfa_1 = 0;


		glPushMatrix();

		glTranslatef(x_1, y_1, z_1);


		if(z_2 < z_1 && dy != 0)
		{
			alfa_2 = 360 - ((atan(dz / dy))*180)/PI;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			Cone(r_1, r_2, d_prim, 20, 20); 
		}
		else if(z_2 > z_1 && dy != 0)
		{
			alfa_2 = ((atan(dz / dy))*180)/PI;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			Cone(r_1, r_2, d_prim, 20, 20);      
		}
		else if(z_2 == z_1)
		{
			alfa_2 = 0;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			Cone(r_1, r_2, d_prim, 20, 20);    
		}
		else if(z_1 > z_2 && dy == 0)
		{
			alfa_2 = -90;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			Cone(r_1, r_2, d_prim, 20, 20);   
		}
		else if(z_1 < z_2 && dy == 0)
		{
			alfa_2 = 90;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			Cone(r_1, r_2, d_prim, 20, 20);   
		}

	}
	else if(z_1 == z_2)
	{
		alfa_2 = 0;

		glPushMatrix();

		glTranslatef(x_1, y_1, z_1);

		if(x_2 < x_1 && dy != 0)
		{
			alfa_1 = ((atan(dx / dy))*180)/PI;

			glRotatef(alfa_1, 0.0, 0.0, 1.0);
			Cone(r_1, r_2, d_prim, 20, 20);
		}
		else if(x_2 > x_1 && dy != 0)
		{
			alfa_1 = 360 - ((atan(dx / dy))*180)/PI;

			glRotatef(alfa_1, 0.0, 0.0, 1.0);
			Cone(r_1, r_2, d_prim, 20, 20);  
		}
		else if(x_2 == x_1)
		{
			alfa_2 = 0;

			glRotatef(alfa_2, 0.0, 0.0, 1.0);
			Cone(r_1, r_2, d_prim, 20, 20);    
		}
		else if(x_1 > x_2 && dy == 0)
		{
			alfa_1 = 90;

			glRotatef(alfa_1, 0.0, 0.0, 1.0);
			Cone(r_1, r_2, d_prim, 20, 20);    
		}
		else if(x_1 < x_2 && dy == 0)
		{
			alfa_1 = -90;

			glRotatef(alfa_1, 0.0, 0.0, 1.0);
			Cone(r_1, r_2, d_prim, 20, 20);   
		}

	}
	else if(y_1 == y_2)
	{

		glPushMatrix();

		glTranslatef(x_1, y_1, z_1);

		if(x_2 < x_1)
		{
			float alfa_1_1 = 90;
			alfa_1 = -((atan(dz / dx))*180)/PI;

			glRotatef(alfa_1, 0.0, 1.0, 0.0);
			glRotatef(alfa_1_1, 0.0, 0.0, 1.0);

			Cone(r_1, r_2, d_prim, 20, 20); 
		}
		else if(x_2 > x_1)
		{
			float alfa_1_1 = -90;
			alfa_1 = -((atan(dz / dx))*180)/PI;

			glRotatef(alfa_1, 0.0, 1.0, 0.0);
			glRotatef(alfa_1_1, 0.0, 0.0, 1.0);

			Cone(r_1, r_2, d_prim, 20, 20); 
		}

	}
	else
	{

		glPushMatrix();

		glTranslatef(x_1, y_1, z_1);


		if(x_2 < x_1)
		{
			alfa_1 = ((atan(dx / dy))*180)/PI;

			glRotatef(alfa_1, 0.0, 0.0, 1.0);
		}    
		else if(x_2 > x_1)
		{
			alfa_1 = -((atan(dx / dy))*180)/PI;

			glRotatef(alfa_1, 0.0, 0.0, 1.0);    
		}


		if(z_2 < z_1)
		{
			alfa_2 = -((atan(dz / dy))*180)/PI;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			Cone(r_1, r_2, d_prim, 20, 20);   
		}    
		else if(z_2 > z_1)
		{
			alfa_2 = ((atan(dz / dy))*180)/PI;

			glRotatef(alfa_2, 1.0, 0.0, 0.0);
			Cone(r_1, r_2, d_prim, 20, 20);  
		}

	}

	glPopMatrix();
	glEndList();

}





///////////////////////////////////////////////////////////////////////////////
// Inicializacia stavovych premennych, nastavenia materialov a svetiel
///////////////////////////////////////////////////////////////////////////////
void init(float clr_background_R, float clr_background_G, float clr_background_B, 
    float lgt1Ambient_R, float lgt1Ambient_G, float lgt1Ambient_B, float lgt1Diffuse_R, 
    float lgt1Diffuse_G, float lgt1Diffuse_B, float lgt1Specular_R, float lgt1Specular_G, 
    float lgt1Specular_B, float lgt0Ambient_R, float lgt0Ambient_G, float lgt0Ambient_B, 
    float lgt0Diffuse_R, float lgt0Diffuse_G, float lgt0Diffuse_B, float lgt0Specular_R, 
    float lgt0Specular_G, float lgt0Specular_B, float lgt0PosX, float lgt0PosY, float lgt0PosZ, 
    float lgt1PosX, float lgt1PosY, float lgt1PosZ, float clrAmbientM_R, float clrAmbientM_G, 
    float clrAmbientM_B, float clrDiffuseM_R, float clrDiffuseM_G, float clrDiffuseM_B, 
    float clrSpecularM_R, float clrSpecularM_G, float clrSpecularM_B, float clrEmissionM_R, 
    float clrEmissionM_G, float clrEmissionM_B,
    int edtShininess, int edtOpacity, int lgt0On, int lgt1On,
    char *texture_name)
{
    
    GLuint texture[1];
    
	glClearColor(clr_background_R, clr_background_G, clr_background_B, 1.0);
	glClearDepth(1.0f);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glutWarpPointer(windowWidth / 2, windowHeight / 2);


	GLfloat light0_ambient[] = { lgt0Ambient_R, lgt0Ambient_G, lgt0Ambient_B, 1.0};
	GLfloat light0_diffuse[] = { lgt0Diffuse_R, lgt0Diffuse_G, lgt0Diffuse_B, 1.0};
	GLfloat light0_specular[] = { lgt0Specular_R, lgt0Specular_G, lgt0Specular_B, 1.0};

	GLfloat light1_ambient[] = { lgt1Ambient_R, lgt1Ambient_G, lgt1Ambient_B, 1.0};
	GLfloat light1_diffuse[] = { lgt1Diffuse_R, lgt1Diffuse_G, lgt1Diffuse_B, 1.0};
	GLfloat light1_specular[] = { lgt1Specular_R, lgt1Specular_G, lgt1Specular_B, 1.0};

	GLfloat light0_position[] = { lgt0PosX, lgt0PosY, lgt0PosZ, 0.0};
	GLfloat light1_position[] = { lgt1PosX, lgt1PosY, lgt1PosZ, 0.0};

	GLfloat ambientColorMaterial[] = {clrAmbientM_R, clrAmbientM_G, clrAmbientM_B, edtOpacity};
	GLfloat diffuseColorMaterial[] = {clrDiffuseM_R, clrDiffuseM_G, clrDiffuseM_B, edtOpacity};
	GLfloat specularColorMaterial[] = {clrSpecularM_R, clrSpecularM_G, clrSpecularM_B, edtOpacity};
	GLfloat emissionColorMaterial[] = {clrEmissionM_R, clrEmissionM_G, clrEmissionM_B, edtOpacity};

	GLfloat mShininess[] = { edtShininess };

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColorMaterial);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuseColorMaterial);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColorMaterial);
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emissionColorMaterial);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mShininess);


	glLightfv(GL_LIGHT0, GL_AMBIENT, light0_ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light0_specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light0_position);


	if(lgt0On)
	{
		glEnable(GL_LIGHT0);  
	}
	else
	{
		glDisable(GL_LIGHT0);    
	}

	glLightfv(GL_LIGHT1, GL_AMBIENT, light1_ambient);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
	glLightfv(GL_LIGHT1, GL_SPECULAR, light1_specular);
	glLightfv(GL_LIGHT1, GL_POSITION, light1_position);


	if(lgt1On)
	{
		glEnable(GL_LIGHT1);  
	}
	else
	{
		glDisable(GL_LIGHT1);    
	}

	glEnable(GL_LIGHTING);
	glEnable(GL_NORMALIZE);

	glEnable(GL_DEPTH_TEST);


	Image *imgTex = loadTexture(texture_name);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D, texture[0]);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexImage2D(GL_TEXTURE_2D, 0, 3, imgTex->sizeX, imgTex->sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, imgTex->data);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_TEXTURE);

	OpenCSG::setOption(OpenCSG::AlgorithmSetting, OpenCSG::Automatic);
	OpenCSG::setOption(OpenCSG::OffscreenSetting, OpenCSG::AutomaticOffscreenType);
}



///////////////////////////////////////////////////////////////////////////////
// Inicializacia pocitadiel, hodnot kamery a zobrazovacich prepinacov
///////////////////////////////////////////////////////////////////////////////
void initConst(void)
{
	r = 0;
	xpos = 0, ypos = 0, zpos = 0, xrot = 0, yrot = 0;
	countCones = 0, snapCounter = 0;

	csg_on_all = false, csg_on = false, grid_on = true, cross_on = true, camera_on = false, hud_1_on = true, hud_2_on = true;
}



///////////////////////////////////////////////////////////////////////////////
// Nastavenie pozicie a rotacie kamery
///////////////////////////////////////////////////////////////////////////////
void camera()
{
	glRotatef(xrot, 1.0, 0.0, 0.0);
	glRotatef(yrot, 0.0, 1.0, 0.0);
	glTranslated(-xpos, -ypos, -zpos);
}



///////////////////////////////////////////////////////////////////////////////
// Vytvorenie mriezky v priestore
///////////////////////////////////////////////////////////////////////////////
void grid(void)
{
	int rows = 100;
	int columns = 100;

	glTranslatef(-50, 0, -50);
	glDisable(GL_LINE_SMOOTH);

	glBegin(GL_LINES);

	for(int i = 0; i <= rows; i++)
	{
		glColor3f(clr_grid_R, clr_grid_G, clr_grid_B);
		glVertex3f(0, 0, i);
		glVertex3f(columns, 0, i);
	}

	for( int i = 0; i <= columns; i++) 
	{
		glColor3f(clr_grid_R, clr_grid_G, clr_grid_B);
		glVertex3f(i, 0, 0);
		glVertex3f(i, 0, rows);
	}

	glEnd();
}




///////////////////////////////////////////////////////////////////////////////
// Vytvorenie suradnicoveho kriza so stredom v pociatku
///////////////////////////////////////////////////////////////////////////////
void cross(void)
{
	glEnable(GL_LINE_SMOOTH);
	glLineWidth(1.0);

	glBegin(GL_LINES);

	glColor3f(1, 0, 0);
	glVertex3f(0, 0, 0);
	glVertex3f(1, 0, 0);
	glVertex3f(0.7, 0.1, 0);
	glVertex3f(1, 0, 0);
	glVertex3f(0.7, -0.1, 0);
	glVertex3f(1, 0, 0);
	glVertex3f(0.7, 0, 0.1);
	glVertex3f(1, 0, 0);
	glVertex3f(0.7, 0, -0.1);
	glVertex3f(1, 0, 0);

	glColor3f(0, 1, 0);
	glVertex3f(0, 0, 0);
	glVertex3f(0, 1, 0);
	glVertex3f(0.1, 0.7, 0);
	glVertex3f(0, 1, 0);
	glVertex3f(-0.1, 0.7, 0);
	glVertex3f(0, 1, 0);
	glVertex3f(0.0, 0.7, 0.1);
	glVertex3f(0, 1, 0);
	glVertex3f(0.0, 0.7, -0.1);
	glVertex3f(0, 1, 0);

	glColor3f(0, 0, 1);
	glVertex3f(0, 0, 0);
	glVertex3f(0, 0, 1);
	glVertex3f(0.0, -0.1, 0.7);
	glVertex3f(0, 0, 1);
	glVertex3f(0.0, 0.1, 0.7);
	glVertex3f(0, 0, 1);
	glVertex3f(0.1, 0.0, 0.7);
	glVertex3f(0, 0, 1);
	glVertex3f(-0.1, 0.0, 0.7);
	glVertex3f(0, 0, 1);

	glEnd();
}



///////////////////////////////////////////////////////////////////////////////
// Prevod textu z textoveho formatu do grafickeho
///////////////////////////////////////////////////////////////////////////////
void drawBitmapText(char *string, float x, float y, float z) 
{  
	char *c;
	glRasterPos3f(x, y, z);

	for (c = string; *c != '\0'; c++)
	{
		glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *c);
	}
}




///////////////////////////////////////////////////////////////////////////////
// Textove menu v pravej casti obrazovky
///////////////////////////////////////////////////////////////////////////////
void rightHUD() 
{  
	glColor3f(0.10, 0.10, 0.45);

	int widthHUD = windowWidth - 350;

	drawBitmapText("Press C to switch to camera mode", widthHUD, 30, 0);
	drawBitmapText("Press G to on/off grid view", widthHUD, 45, 0);
	drawBitmapText("Press K to on/off cross view", widthHUD, 60, 0);
	drawBitmapText("Camera mode behaviors:", widthHUD, 90, 0);
	drawBitmapText("Press Up Arrow to move forwards", widthHUD + 15, 105, 0);
	drawBitmapText("Press Down Arrow to move backwards", widthHUD + 15, 120, 0);
	drawBitmapText("Press Left Arrow to strafe left", widthHUD + 15, 135, 0);
	drawBitmapText("Press Right Arrow to strafe right", widthHUD + 15, 150, 0);
	drawBitmapText("Press Page Up/Down to move up and down", widthHUD + 15, 165, 0);
	drawBitmapText("Press F5/F6 to on/off CSG view", widthHUD + 15, 180, 0);
	drawBitmapText("Press F12 to create a snapshot", widthHUD + 15, 195, 0);
	drawBitmapText("Press Escape to quit", widthHUD + 15, 210, 0);
	drawBitmapText("Press F2 to hide help", widthHUD, 240, 0);
}




///////////////////////////////////////////////////////////////////////////////
// Textove menu v lavej casti obrazovky
///////////////////////////////////////////////////////////////////////////////
void leftHUD() 
{  
	char spr[100];

	glColor3f(0.10, 0.10, 0.45);

	drawBitmapText("Camera", 30, 30, 0);
	sprintf(spr, "Position: x:%f y:%f z:%f", xpos, ypos, zpos);
	drawBitmapText(spr, 45, 45, 0);
	sprintf(spr, "UpDown rotation: %f", yrot);
	drawBitmapText(spr, 45, 60, 0);
	sprintf(spr, "LeftRight rotation: %f", xrot);
	drawBitmapText(spr, 45, 75, 0);
	drawBitmapText("Mouse", 30, 105, 0);
	sprintf(spr, "Position: x:%d y:%d", mousePosX, mousePosY);
	drawBitmapText(spr, 45, 120, 0);
	drawBitmapText("Press F1 to hide help", 30, 150, 0);
}




///////////////////////////////////////////////////////////////////////////////
// Vykreslenie sceny
///////////////////////////////////////////////////////////////////////////////
void draw()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION); 

	glLoadIdentity();

	gluOrtho2D(0, windowWidth, windowHeight, 0);
	glMatrixMode(GL_MODELVIEW); 

	if( hud_1_on )
	{
		rightHUD();
	}

	if( hud_2_on )
	{
		leftHUD();
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (GLfloat)windowWidth / (GLfloat)windowHeight, 1.0, 1000.0);
	glMatrixMode(GL_MODELVIEW);


	glPushMatrix();


	if( camera_on )
	{
        
		camera();

		stat_xpos = xpos;
		stat_ypos = ypos;
		stat_zpos = zpos;
		stat_xrot = xrot;
		stat_yrot = yrot;
	}
	else
	{
		glRotatef(stat_xrot, 1.0, 0.0, 0.0);
		glRotatef(stat_yrot, 0.0, 1.0, 0.0);
		glTranslated(-stat_xpos, -stat_ypos, -stat_zpos);

		xpos = stat_xpos;
		ypos = stat_ypos;
		zpos = stat_zpos;
		xrot = stat_xrot;
		yrot = stat_yrot; 
	}

	if( cross_on )
	{
		cross();
	}

	glEnable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);

    if( csg_on_all )
    {
         if( csg_on )
         {
			OpenCSG::render(prim);

			glDepthFunc(GL_EQUAL);
			glShadeModel(GL_SMOOTH);

			for (std::vector<OpenCSG::Primitive*>::const_iterator d = prim.begin(); d != prim.end(); ++d) 
			{
				(*d)->render();
			}

			glShadeModel(GL_SMOOTH);
			glDepthFunc(GL_LESS);
        }
        else
        {
            for(int t = 0; t < 100; t++)
            {
                OpenCSG::render(prim_pipe[t]);
    
    			glDepthFunc(GL_EQUAL);
    			glShadeModel(GL_SMOOTH);
    
    			for (std::vector<OpenCSG::Primitive*>::const_iterator d = prim_pipe[t].begin(); d != prim_pipe[t].end(); ++d) 
    			{
    				(*d)->render();
    			}
    
    			glShadeModel(GL_SMOOTH);
    			glDepthFunc(GL_LESS); 
            }
        }
    }
    else
    {
            OpenCSG::render(prim);

			glDepthFunc(GL_EQUAL);
			glShadeModel(GL_SMOOTH);

			for (std::vector<OpenCSG::Primitive*>::const_iterator d = prim.begin(); d != prim.end(); ++d) 
			{
				(*d)->render();
			}
			
			for(int t = 0; t < 100; t++)
            {
                OpenCSG::render(prim_pipe[t]);
    
    			for (std::vector<OpenCSG::Primitive*>::const_iterator d = prim_pipe[t].begin(); d != prim_pipe[t].end(); ++d) 
    			{
    				(*d)->render();
    			}
            }

			glShadeModel(GL_SMOOTH);
			glDepthFunc(GL_LESS);
    }
        
        
    glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);


	glTranslatef(0, 0, 0);

	if( grid_on )
	{
		grid();
	}

	glPopMatrix();


	glutSwapBuffers();
}




///////////////////////////////////////////////////////////////////////////////
// Prisposobenie zmeny velkosti okna
///////////////////////////////////////////////////////////////////////////////
void reshape(int w, int h) 
{
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);
	glMatrixMode(GL_PROJECTION);

	glLoadIdentity();
	gluPerspective(60, (GLfloat)w / (GLfloat)h, 1.0, 1000.0);
	glMatrixMode(GL_MODELVIEW);

	windowWidth = w;
	windowHeight = h;
}




///////////////////////////////////////////////////////////////////////////////
// Reakcia klavesy pre ovladanie kamery, prienikov a zachytenie obrazu
///////////////////////////////////////////////////////////////////////////////
void processSpecialKeys(int key, int x, int y) 
{

	switch(key)
	{
	case GLUT_KEY_PAGE_UP:  ypos += 1;
		break;
	case GLUT_KEY_PAGE_DOWN: ypos -= 1;
		break;   
	case GLUT_KEY_UP:
		{
			float xrotrad, yrotrad;
			yrotrad = (yrot / 180 * PI);
			xrotrad = (xrot / 180 * PI);
			xpos += float(sin(yrotrad));
			zpos -= float(cos(yrotrad));
			ypos -= float(sin(xrotrad));
		}
		break;
	case GLUT_KEY_DOWN:  
		{
			float xrotrad, yrotrad;
			yrotrad = (yrot / 180 * PI);
			xrotrad = (xrot / 180 * PI);
			xpos -= float(sin(yrotrad));
			zpos += float(cos(yrotrad));
			ypos += float(sin(xrotrad));
		}
		break;
	case GLUT_KEY_RIGHT:
		{
			float yrotrad;
			yrotrad = (yrot / 180 * PI);
			xpos += float(cos(yrotrad)) * 0.5;
			zpos += float(sin(yrotrad)) * 0.5;
		}
		break;
	case GLUT_KEY_LEFT:
		{
			float yrotrad;
			yrotrad = (yrot / 180 * PI);
			xpos -= float(cos(yrotrad)) * 0.5;
			zpos -= float(sin(yrotrad)) * 0.5;
		}
		break;
	case GLUT_KEY_F12:
		{
			char snapFile[50];
			char messageFile[50];

			sprintf(snapFile, ".\\snapshots\\snapshot_%d.tga", snapCounter++);

			saveScreenGrab(snapFile);

			sprintf(messageFile, "Snapshot has created in file snapshot_%d.tga.", snapCounter);
			MessageBox(NULL, messageFile, "Snapshot", MB_ICONINFORMATION);
		}
		break;
	case GLUT_KEY_F5:
		if( csg_on == false)
		{
			csg_on = true;
		}
		else
		{
			csg_on = false;
		}
		break;
	case GLUT_KEY_F6:
		if( csg_on_all == false)
		{
			csg_on_all = true;
		}
		else
		{
			csg_on_all = false;
		}
		break;


	case GLUT_KEY_F2:
		if( hud_1_on == false)
		{
			hud_1_on = true;
		}
		else
		{
			hud_1_on = false;
		}
		break;


	case GLUT_KEY_F1:
		if( hud_2_on == false)
		{
			hud_2_on = true;
		}
		else
		{
			hud_2_on = false;
		}
		break;

	default: break;
	}

}



///////////////////////////////////////////////////////////////////////////////
// Reakcia klavesy na prepinanie mriezky, osoveho kriza, kamery a ukoncenia programu
///////////////////////////////////////////////////////////////////////////////
void keyboard(unsigned char key, int x, int y)
{

	switch(key)
	{
	case 'g':
		if( grid_on == false)
		{
			grid_on = true;
		}
		else
		{
			grid_on = false;
		}
		break;
	case 'k':
		if( cross_on == false)
		{
			cross_on = true;
		}
		else
		{
			cross_on = false;
		}
		break;
	case 'c':
		if( camera_on == false)
		{
			camera_on = true;
			glutSetCursor(GLUT_CURSOR_NONE);
		}
		else
		{
			camera_on = false;
			glutSetCursor(GLUT_CURSOR_INHERIT);

		}
		break;

	case 27:
		exit(0);
		break;

	default: break;

	}	

}



///////////////////////////////////////////////////////////////////////////////
// Zachytavanie pohybu mysi pre ovladanie kamery
///////////////////////////////////////////////////////////////////////////////
void mouseMovement(int x, int y) 
{
	int diffx = x - lastx;
	int diffy = y - lasty;
	lastx = x;
	lasty = y;
	xrot += (float) diffy;
	yrot += (float) diffx;

	mousePosX = x;
	mousePosY = y;
}



///////////////////////////////////////////////////////////////////////////////
// Nacitanie parametrov rur zo suboru, ich prvotne vykreslenie, zapis log suboru
///////////////////////////////////////////////////////////////////////////////
void preLoader(void)
{
	FILE *fr;
	int c;


	fr = fopen(".\\pipe.txt", "r");

	while((c = getc(fr)) != EOF)
	{
		fscanf(fr, " %f %f %f %f %f %f %f %f", &x_1f[countCones], &z_1f[countCones], &y_1f[countCones], &r_1f[countCones], &x_2f[countCones], &z_2f[countCones], &y_2f[countCones], &r_2f[countCones]);

		countCones++;
	}

	fclose(fr);


//CSG pre prvu cast


    for(int idd = 0; idd < 100; idd++)
		{
			id_prim[idd] = 0;
		}
		
		
	clearPrimitives();

	setCone(0, x_1f[0], z_1f[0], y_1f[0], r_1f[0], x_2f[0], z_2f[0], y_2f[0], r_1f[0]);
	setConePlus(1, x_1f[0], z_1f[0], y_1f[0], r_2f[0], x_2f[0], z_2f[0], y_2f[0], r_2f[0]);


	prim.push_back(new OpenCSG::DisplayListPrimitive(id_prim[0], OpenCSG::Intersection, 1));
	prim.push_back(new OpenCSG::DisplayListPrimitive(id_prim[1], OpenCSG::Subtraction, 1));

	for(int j = 1; j < countCones; j++)
	{
		setCone(j+1, x_1f[j], z_1f[j], y_1f[j], r_1f[j], x_2f[j], z_2f[j], y_2f[j], r_2f[j]);
	}

	prim.push_back(new OpenCSG::DisplayListPrimitive(id_prim[0], OpenCSG::Intersection, 1));

	for(int pp = 2; pp < countCones+1; pp++)
	{
		prim.push_back(new OpenCSG::DisplayListPrimitive(id_prim[pp], OpenCSG::Subtraction, 1));
	}



//CSG pre druhu cast

    for(int e = 0; e < countCones+1; e++)
    {
        prim_pipe[e].push_back(new OpenCSG::DisplayListPrimitive(id_prim[e+2], OpenCSG::Intersection, 1));
	    prim_pipe[e].push_back(new OpenCSG::DisplayListPrimitive(id_prim[0], OpenCSG::Subtraction, 1));    
    }



	FILE *fw;

	fw = fopen("load_pipe.log", "w");

	fprintf(fw, "Loaded objects - Pipe Layout\n\n\n");

	for(int j = 0; j < countCones; j++)
	{
		if(j == 0)
		{
			fprintf(fw, "Tube\n");
			fprintf(fw, "x1_%d=%f z1_%d=%f y1_%d=%f r1_%d=%f x2_%d=%f z2_%d=%f y2_%d=%f r2_%d=%f\n\n", j+1, x_1f[j],j+1, z_1f[j],j+1, y_1f[j],j+1, r_1f[j],j+1, x_2f[j],j+1, z_2f[j],j+1, y_2f[j],j+1, r_2f[j]);     
		}
		else
		{     
			fprintf(fw, "Cylinder(Cone)[%d]\n", j);
			fprintf(fw, "x1_%d=%f z1_%d=%f y1_%d=%f r1_%d=%f x2_%d=%f z2_%d=%f y2_%d=%f r2_%d=%f\n\n", j+1, x_1f[j],j+1, z_1f[j],j+1, y_1f[j],j+1, r_1f[j],j+1, x_2f[j],j+1, z_2f[j],j+1, y_2f[j],j+1, r_2f[j]);
		}
	}
	fclose(fw);
}




///////////////////////////////////////////////////////////////////////////////
// Reakcia menu poloziek na PopUp menu
///////////////////////////////////////////////////////////////////////////////
void menu(int value) 
{

	switch (value) 
	{
	case EXIT: exit(0);
		break;
	case ALGO_AUTOMATIC: OpenCSG::setOption(OpenCSG::AlgorithmSetting, OpenCSG::Automatic);
		break;
	case GF_STANDARD:    OpenCSG::setOption(OpenCSG::AlgorithmSetting, OpenCSG::Goldfeather);
		OpenCSG::setOption(OpenCSG::DepthComplexitySetting, OpenCSG::NoDepthComplexitySampling);
		break;
	case GF_DC:          OpenCSG::setOption(OpenCSG::AlgorithmSetting, OpenCSG::Goldfeather);
		OpenCSG::setOption(OpenCSG::DepthComplexitySetting, OpenCSG::DepthComplexitySampling);
		break;
	case GF_OQ:          OpenCSG::setOption(OpenCSG::AlgorithmSetting, OpenCSG::Goldfeather);
		OpenCSG::setOption(OpenCSG::DepthComplexitySetting, OpenCSG::OcclusionQuery);
		break;
	case SCS_STANDARD:   OpenCSG::setOption(OpenCSG::AlgorithmSetting, OpenCSG::SCS);
		OpenCSG::setOption(OpenCSG::DepthComplexitySetting, OpenCSG::NoDepthComplexitySampling);
		break;
	case SCS_DC:         OpenCSG::setOption(OpenCSG::AlgorithmSetting, OpenCSG::SCS);
		OpenCSG::setOption(OpenCSG::DepthComplexitySetting, OpenCSG::DepthComplexitySampling);
		break;
	case SCS_OQ:         OpenCSG::setOption(OpenCSG::AlgorithmSetting, OpenCSG::SCS);
		OpenCSG::setOption(OpenCSG::DepthComplexitySetting, OpenCSG::OcclusionQuery);
		break;
	default: break;
	}

}




///////////////////////////////////////////////////////////////////////////////
// Nacitanie suborov z externeho programu PipeLayoutSetup a inicializacia funkcii GLUT
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
	int c, countPls = 0;
	char pls_main[25][255], pls_editor[50][255];
	
    float clr_background_R, clr_background_G, clr_background_B;
    int edtShininess, edtOpacity, lgt0On, lgt1On;
    float clrAmbientM_R, clrAmbientM_G, clrAmbientM_B, clrDiffuseM_R, clrDiffuseM_G, clrDiffuseM_B, clrSpecularM_R, clrSpecularM_G, clrSpecularM_B, clrEmissionM_R, clrEmissionM_G, clrEmissionM_B;
    float lgt0PosX, lgt0PosY, lgt0PosZ, lgt1PosX, lgt1PosY, lgt1PosZ;
    float lgt0Ambient_R, lgt0Ambient_G, lgt0Ambient_B, lgt0Diffuse_R, lgt0Diffuse_G, lgt0Diffuse_B, lgt0Specular_R, lgt0Specular_G, lgt0Specular_B;
    float lgt1Ambient_R, lgt1Ambient_G, lgt1Ambient_B, lgt1Diffuse_R, lgt1Diffuse_G, lgt1Diffuse_B, lgt1Specular_R, lgt1Specular_G, lgt1Specular_B;

    int windowWidth, windowHeight; 
    char *texture_name;

	Sleep(1500);

	FILE *fr;

	fr = fopen("pls_main.ini", "r");

	while((c = getc(fr)) != EOF)
	{
		fscanf(fr, "%s", pls_main[countPls]);
		countPls++;
	}
	fclose(fr);


	fr = fopen("pls_editor.ini", "r");

	countPls = 0;

	while((c = getc(fr)) != EOF)
	{

		fscanf(fr, "%s", pls_editor[countPls]);
		countPls++;
	}
	fclose(fr);


	windowWidth = atoi(pls_main[0]);
	windowHeight = atoi(pls_main[1]);

	windowPosX = atoi(pls_main[2]); 
	windowPosY = atoi(pls_main[3]);

	stat_xpos = atof(pls_main[4]); 
	stat_ypos = atof(pls_main[5]); 
	stat_zpos = atof(pls_main[6]); 
	stat_xrot = atof(pls_main[7]); 
	stat_yrot = atof(pls_main[8]);

	clr_background_R = convertRGB( atoi(pls_main[9]) );
	clr_background_G = convertRGB( atoi(pls_main[10]) );
	clr_background_B = convertRGB( atoi(pls_main[11]) );

	clr_grid_R = convertRGB( atoi(pls_main[12]) );
	clr_grid_G = convertRGB( atoi(pls_main[13]) );
	clr_grid_B = convertRGB( atoi(pls_main[14]) );

	texture_name = pls_editor[0];
	clrAmbientM_R = convertRGB( atoi(pls_editor[1]) );
	clrAmbientM_G = convertRGB( atoi(pls_editor[2]) );
	clrAmbientM_B = convertRGB( atoi(pls_editor[3]) );

	clrDiffuseM_R = convertRGB( atoi(pls_editor[4]) );
	clrDiffuseM_G = convertRGB( atoi(pls_editor[5]) );
	clrDiffuseM_B = convertRGB( atoi(pls_editor[6]) );

	clrSpecularM_R = convertRGB( atoi(pls_editor[7]) );
	clrSpecularM_G = convertRGB( atoi(pls_editor[8]) );
	clrSpecularM_B = convertRGB( atoi(pls_editor[9]) );

	clrEmissionM_R = convertRGB( atoi(pls_editor[10]) );
	clrEmissionM_G = convertRGB( atoi(pls_editor[11]) );
	clrEmissionM_B = convertRGB( atoi(pls_editor[12]) );

	edtShininess = atoi( pls_editor[13] );
	edtOpacity = atoi( pls_editor[14] );

	lgt0PosX = convertRGB( atoi(pls_editor[15]) );
	lgt0PosY = convertRGB( atoi(pls_editor[16]) );
	lgt0PosZ = convertRGB( atoi(pls_editor[17]) );

	lgt0Ambient_R = convertRGB( atoi(pls_editor[18]) );
	lgt0Ambient_G = convertRGB( atoi(pls_editor[19]) );
	lgt0Ambient_B = convertRGB( atoi(pls_editor[20]) );

	lgt0Diffuse_R = convertRGB( atoi(pls_editor[21]) );
	lgt0Diffuse_G = convertRGB( atoi(pls_editor[22]) );
	lgt0Diffuse_B = convertRGB( atoi(pls_editor[23]) );

	lgt0Specular_R = convertRGB( atoi(pls_editor[24]) );
	lgt0Specular_G = convertRGB( atoi(pls_editor[25]) );
	lgt0Specular_B = convertRGB( atoi(pls_editor[26]) );

	lgt0On = atoi(pls_editor[27]);

	lgt1PosX = convertRGB( atoi(pls_editor[28]) );
	lgt1PosY = convertRGB( atoi(pls_editor[29]) );
	lgt1PosZ = convertRGB( atoi(pls_editor[30]) );

	lgt1Ambient_R = convertRGB( atoi(pls_editor[31]) );
	lgt1Ambient_G = convertRGB( atoi(pls_editor[32]) );
	lgt1Ambient_B = convertRGB( atoi(pls_editor[33]) );

	lgt1Diffuse_R = convertRGB( atoi(pls_editor[34]) );
	lgt1Diffuse_G = convertRGB( atoi(pls_editor[35]) );
	lgt1Diffuse_B = convertRGB( atoi(pls_editor[36]) );

	lgt1Specular_R = convertRGB( atoi(pls_editor[37]) );
	lgt1Specular_G = convertRGB( atoi(pls_editor[38]) );
	lgt1Specular_B = convertRGB( atoi(pls_editor[39]) );

	lgt1On = atoi(pls_editor[40]);


	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);

                 
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(windowPosX, windowPosY);

	glutCreateWindow("Pipe Layout");
	initConst();
	
	init(clr_background_R, clr_background_G, clr_background_B, 
    lgt1Ambient_R, lgt1Ambient_G, lgt1Ambient_B, lgt1Diffuse_R, 
    lgt1Diffuse_G, lgt1Diffuse_B, lgt1Specular_R, lgt1Specular_G, 
    lgt1Specular_B, lgt0Ambient_R, lgt0Ambient_G, lgt0Ambient_B, 
    lgt0Diffuse_R, lgt0Diffuse_G, lgt0Diffuse_B, lgt0Specular_R, 
    lgt0Specular_G, lgt0Specular_B, lgt0PosX, lgt0PosY, lgt0PosZ, 
    lgt1PosX, lgt1PosY, lgt1PosZ, clrAmbientM_R, clrAmbientM_G, 
    clrAmbientM_B, clrDiffuseM_R, clrDiffuseM_G, clrDiffuseM_B, 
    clrSpecularM_R, clrSpecularM_G, clrSpecularM_B, clrEmissionM_R, 
    clrEmissionM_G, clrEmissionM_B,
    edtShininess, edtOpacity, lgt0On, lgt1On,
    texture_name);


	int err = glewInit();

	if (GLEW_OK != err) 
	{
		printf("%d", glewGetErrorString(err));
		return 1;
	} 


	int menuAlgorithm = glutCreateMenu(menu);
	glutAddMenuEntry("Automatic", ALGO_AUTOMATIC);
	glutAddMenuEntry("Goldfeather standard",GF_STANDARD);
	glutAddMenuEntry("Goldfeather depth complexity sampling", GF_DC);
	glutAddMenuEntry("Goldfeather occlusion query", GF_OQ);
	glutAddMenuEntry("SCS standard", SCS_STANDARD);
	glutAddMenuEntry("SCS depth complexity sampling", SCS_DC);
	glutAddMenuEntry("SCS occlusion query", SCS_OQ);

	glutCreateMenu(menu);
	glutAddSubMenu("CSG Algorithms", menuAlgorithm);
	glutAddMenuEntry("------------------", -1);
	glutAddMenuEntry("Exit", EXIT);

	glutAttachMenu(GLUT_RIGHT_BUTTON);

	preLoader();

	glutDisplayFunc(draw);
	glutIdleFunc(draw);
	glutReshapeFunc(reshape);
	glutPassiveMotionFunc(mouseMovement); 
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(processSpecialKeys);

	glutMainLoop();

	return 0;
}
