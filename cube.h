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

#define RotateR()                RotateXSection(cube_size-1, FALSE, TRUE)
#define RotateRCCW()             RotateXSection(cube_size-1, TRUE, TRUE)
#define RotateL()                RotateXSection(0, TRUE, TRUE)
#define RotateLCCW()             RotateXSection(0, FALSE, TRUE)
#define RotateF()                RotateZSection(cube_size-1, FALSE, TRUE)
#define RotateFCCW()             RotateZSection(cube_size-1, TRUE, TRUE)
#define RotateB()                RotateZSection(0, TRUE, TRUE)
#define RotateBCCW()             RotateZSection(0, FALSE, TRUE)
#define RotateD()                RotateYSection(0, TRUE, TRUE)
#define RotateDCCW()             RotateYSection(0, FALSE, TRUE)
#define RotateU()                RotateYSection(cube_size-1, FALSE, TRUE)
#define RotateUCCW()             RotateYSection(cube_size-1, TRUE, TRUE)


class OGLWidget;

const struct {                                                                  //kominacja RGB dla uzyskania koloru kazdej ze stron kostki
    unsigned char r, g, b;
    } m_rgb[] = {
        {255,  0,  0,  },                                                       //Red
        {  0, 255,   0,},                                                       //Green
        {  0,   0, 255,},                                                       //Blue
        {255,   0, 255,},                                                       //Purple
        {255, 127,   0,},                                                       //Orange
        {255, 255,   0,},                                                       //Yellow
        {  0,   0,   0,},                                                       //Black
        {255, 255, 255,},                                                       //White
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
    void SetRotation(float fAngle, QVector3D vRotation) { m_fRotationAngle=fAngle; m_vRotation=vRotation; };
    void ClrRotation(void)                              { m_fRotationAngle=0; };
    void RotateX(bool bCW);
    void RotateY(bool bCW);
    void RotateZ(bool bCW);
    void Draw(float x,float y,float z);
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
    void Reset(void);
    void Random(void);
    bool Rotate(GLdouble* mxProjection, GLdouble* mxModel, GLint* nViewPort,
                int wndSizeX, int wndSizeY, int ptMouseWndX, int ptMouseWndY, int ptLastMouseWndX, int ptLastMouseWndY, OGLWidget *widget);
    void RotateXSection(UINT8 nSection, BOOL bCW, BOOL bAnimate);
    void RotateYSection(UINT8 nSection, BOOL bCW, BOOL bAnimate);
    void RotateZSection(UINT8 nSection, BOOL bCW, BOOL bAnimate);
    void AnimateRotation(TCubePiece* piece[], int ctPieces, QVector3D v, float fAngle);
    void Draw(void);
    bool Check(void);
    SIDE FindWhiteCrossSide(void);
    bool CheckWhiteCross(SIDE whiteCrossSide);
    bool CheckWhiteCrossCorners(SIDE whiteCrossSide);
    bool CheckSecondLayer(SIDE whiteCrossSide);
    bool CheckBlueCross(SIDE whiteCrossSide);
    bool CheckEdgePermutationOfBlueCross(SIDE whiteCrossSide);
    bool CheckPermutationOfBlueCorners(SIDE whiteCrossSide);
    bool CheckOrientationOfBlueCorners(SIDE whiteCrossSide);
    void WhiteCrossSideToTop(SIDE whiteCrossSide);
    void ArrangeWhiteCross(void);
    void ArrangeWhiteCrossCorners(void);
    void ArrangeSecondLayer(void);
    void ArrangeBlueCross(void);
    void ArrangeEdgePermutationOfBlueCross(void);
    void PermutationOfBlueCorners(void);
    void OrientationOfBlueCorners(void);
    bool Solve(void);
};

#endif // CUBE_H
