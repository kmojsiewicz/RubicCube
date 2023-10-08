#ifndef CUBE_H
#define CUBE_H

#include <QVector2D>
#include <QVector3D>
#include <QtOpenGL>

#define cube_size                3
#define cube_mid_pos             (cube_size / 2)
#define ALMOST_ZERO              1e-6
#define ELEMENTS_OF(array)       (sizeof(array)/sizeof(array[0]))
#define MAKECOLOR(nColor)        m_rgb[nColor].r, m_rgb[nColor].g, m_rgb[nColor].b

#define RotateR()                rotateXSection(cube_size-1, FALSE, TRUE)
#define RotateRCCW()             rotateXSection(cube_size-1, TRUE, TRUE)
#define RotateL()                rotateXSection(0, TRUE, TRUE)
#define RotateLCCW()             rotateXSection(0, FALSE, TRUE)
#define RotateF()                rotateZSection(cube_size-1, FALSE, TRUE)
#define RotateFCCW()             rotateZSection(cube_size-1, TRUE, TRUE)
#define RotateB()                rotateZSection(0, TRUE, TRUE)
#define RotateBCCW()             rotateZSection(0, FALSE, TRUE)
#define RotateD()                rotateYSection(0, TRUE, TRUE)
#define RotateDCCW()             rotateYSection(0, FALSE, TRUE)
#define RotateU()                rotateYSection(cube_size-1, FALSE, TRUE)
#define RotateUCCW()             rotateYSection(cube_size-1, TRUE, TRUE)


class OGLWidget;

const struct {                                                                  // RGB combination to obtain the color of each side of the cube
    unsigned char r, g, b;
    } m_rgb[] = {
        {255,  0,  0,  },                                                       // Red
        {  0, 255,   0,},                                                       // Green
        {  0,   0, 255,},                                                       // Blue
        {255,   0, 255,},                                                       // Purple
        {255, 127,   0,},                                                       // Orange
        {255, 255,   0,},                                                       // Yellow
        {  0,   0,   0,},                                                       // Black
        {255, 255, 255,},                                                       // White
    };

typedef enum {
    ROT_R,
    ROT_RCCW,
    ROT_L,
    ROT_LCCW,
    ROT_F,
    ROT_FCCW,
    ROT_B,
    ROT_BCCW,
    ROT_D,
    ROT_DCCW,
    ROT_U,
    ROT_UCCW
} ROTATIONS;

typedef enum {
    RED,
    GREEN,
    BLUE,
    PURPLE,
    ORANGE,
    YELLOW,
    BLACK,
    WHITE,
} SIDECOLOR;

typedef enum {
   SD_RIGHT,
   SD_LEFT,
   SD_TOP,
   SD_BOTTOM,
   SD_FRONT,
   SD_BACK,
} SIDE;

class BYTEVEC {
public:
    char x,y,z;
    BYTEVEC() {};
    BYTEVEC(char xx, char yy, char zz)   {x=xx; y=yy; z=zz;};
};

class PT3D {
public:
    double x,y,z;
    PT3D()                                    {};
    PT3D(double xx, double yy, double zz)     {x=xx; y=yy; z=zz;};
};

class PT2D {
public:
    double x,y;
    PT2D()                                    {};
    PT2D(double xx, double yy)                {x=xx; y=yy;};
    PT2D& operator =(const PT3D& pt)          {x=pt.x; y=pt.y; return *this;};
};

typedef void (*func_t)();

class TCubePiece {
protected:
    float m_fRotationAngle;
    QVector3D m_vRotation;
public:
    SIDECOLOR m_nSideColor[6];
    TCubePiece(BYTEVEC posHome);
    void setRotation(float fAngle, QVector3D vRotation) { m_fRotationAngle=fAngle; m_vRotation=vRotation; };
    void clrRotation(void)                              { m_fRotationAngle=0; };
    void rotateX(bool bCW);
    void rotateY(bool bCW);
    void rotateZ(bool bCW);
    void draw(float x,float y,float z);
};

class TCube {
protected:
    bool blueEdgeOrientation;
    int secondLayerBottomRotations;
    QVector<ROTATIONS> moves;

public:
    OGLWidget *widget;

    TCube(OGLWidget *widget);
    ~TCube();
    TCubePiece* m_pPieces[cube_size][cube_size][cube_size];
    void reset(void);
    void random(void);
    bool rotate(GLdouble* mxProjection, GLdouble* mxModel, GLint* nViewPort,
                int wndSizeX, int wndSizeY, int ptMouseWndX, int ptMouseWndY, int ptLastMouseWndX, int ptLastMouseWndY, OGLWidget *widget);
    void rotateXSection(UINT8 nSection, BOOL bCW, BOOL bAnimate);
    void rotateYSection(UINT8 nSection, BOOL bCW, BOOL bAnimate);
    void rotateZSection(UINT8 nSection, BOOL bCW, BOOL bAnimate);
    void animateRotation(TCubePiece* piece[], int ctPieces, QVector3D v, float fAngle);
    void draw(void);
    bool check(void);
    SIDE findWhiteCrossSide(void);
    bool checkWhiteCross(SIDE whiteCrossSide);
    bool checkWhiteCrossCorners(SIDE whiteCrossSide);
    bool checkSecondLayer(SIDE whiteCrossSide);
    bool checkBlueCross(SIDE whiteCrossSide);
    bool checkEdgePermutationOfBlueCross(SIDE whiteCrossSide);
    bool checkPermutationOfBlueCorners(SIDE whiteCrossSide);
    bool checkOrientationOfBlueCorners(SIDE whiteCrossSide);
    void whiteCrossSideToTop(SIDE whiteCrossSide);
    void arrangeWhiteCross(void);
    void arrangeWhiteCrossCorners(void);
    void arrangeSecondLayer(void);
    void arrangeBlueCross(void);
    void arrangeEdgePermutationOfBlueCross(void);
    void permutationOfBlueCorners(void);
    void orientationOfBlueCorners(void);
    bool solve(void);
};

#endif // CUBE_H
