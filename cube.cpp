#include "cube.h"
#include "oglwidget.h"

#include <QtOpenGL>
#include <GL/GLU.h>
#include <QtGlobal>
#include <QThread>
#include <algorithm>
#include <cfloat>

float g_fMinMouseLength = 0.5f;                                                 // the minimum length of the mouse displacement vector needed to rotate the cube section
float g_fMaxMouseAngle = 135.0f;                                                // how close the mouse's displacement vector must be to the cube to rotate the cube
UINT  g_nRotationSteps = 150;                                                   // number of animation frames at 90 degrees ankle rotation

inline float toDegs(float fRadians)     { return fRadians*360/(2*M_PI); };

//---------------------------------------------------------------------------
QString cubeSideToString(SIDE s)
{
    switch (s) {
    case SD_RIGHT  : return "RIGHT";
    case SD_LEFT   : return "LEFT";
    case SD_TOP    : return "TOP";
    case SD_BOTTOM : return "BOTTOM";
    case SD_FRONT  : return "FRONT";
    case SD_BACK   : return "BACK";
    }

    return "";
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
float Vec2DDot(QVector2D& v1, QVector2D& v2)
{
    return v1.x()*v2.x() + v1.y()*v2.y();
}

//---------------------------------------------------------------------------
float Vec2DAngle(QVector2D& v1, QVector2D& v2)                                  // Get angle in degrees between unit vectors v1 and v2
{
    float d;
    d = Vec2DDot(v1,v2);
    if (d<-1.0f || d>1.0f) return 0.0/0.0;                                      // return NaN
    return toDegs((float)acos(d));
}

//---------------------------------------------------------------------------
// Checking which side of the line a given point is located
// The function returns: 1 if the point is on the right, 0 if the point is on the line, -1 if the point is on the left
//---------------------------------------------------------------------------
int lineTest(const PT2D& ptLineStart, const PT2D& ptLineEnd, const int ptX, const int ptY)
{
    double dx = ptLineEnd.x - ptLineStart.x;
    double dy = ptLineEnd.y - ptLineStart.y;
    if (fabs(dx)>fabs(dy)) {
        double yOnline = dy/dx * (ptX - ptLineStart.x) + ptLineStart.y;
        return ptY >  yOnline ? 1 : (ptY < yOnline ? -1 : 0);
    }
    else {
        double xOnline = dx/dy * (ptY - ptLineStart.y) + ptLineStart.x;
        return ptX >  xOnline ? 1 : (ptX < xOnline ? -1 : 0);
    }
}

//---------------------------------------------------------------------------
// The function checks whether the given point fits into the given quadrilateral
//---------------------------------------------------------------------------
bool poly4InsideTest(const PT2D* ptCorners, const int ptX, const int ptY)
{
    int nResult1, nResult2;
    nResult1 = lineTest(ptCorners[0], ptCorners[1], ptX, ptY);
    nResult2 = lineTest(ptCorners[2], ptCorners[3], ptX, ptY);
    if (((nResult1 > 0) && (nResult2 > 0)) || ((nResult1 < 0) && (nResult2 < 0))) return FALSE;
    nResult1 = lineTest(ptCorners[0], ptCorners[3], ptX, ptY);
    nResult2 = lineTest(ptCorners[1], ptCorners[2], ptX, ptY);
    if (((nResult1 > 0) && (nResult2 > 0)) || ((nResult1 < 0) && (nResult2 < 0))) return FALSE;
    return TRUE;
}

//---------------------------------------------------------------------------
// The function checks whether the given point is located in the given quadrilateral (in this case, the z coordinate is ignored)
//---------------------------------------------------------------------------
BOOL poly4InsideTest(PT3D* pt3Corners, const int ptX, const int ptY)
{
    PT2D pt2Corners[4];
    for (int i=0; i<4; i++) pt2Corners[i] = pt3Corners[i];
    return poly4InsideTest(pt2Corners, ptX, ptY);
}

//---------------------------------------------------------------------------
// The function checks in which section of the cube on the X axis the given point is located
//---------------------------------------------------------------------------
UINT8 getXsection(PT2D* ptCorners, const int ptX, const int ptY)
{
    int i, rc[cube_size];
    double dx, dy;
    PT2D ptLineStart, ptLineEnd;
    dx = (ptCorners[1].x - ptCorners[0].x)/cube_size;                           // divide the given segment into the number of sections
    dy = (ptCorners[1].y - ptCorners[0].y)/cube_size;
    //Now that we know point isn't in the center strip, test a line down the center
    ptLineStart.x = ptCorners[0].x;                                             // we calculate the first section
    ptLineStart.y = ptCorners[0].y;
    ptLineEnd.x   = ptCorners[3].x;
    ptLineEnd.y   = ptCorners[3].y;
    for (i=0; i<cube_size; i++) {
        rc[i] = lineTest(ptLineStart, ptLineEnd, ptX, ptY);                     // perform a point position test for all received segments
        ptLineStart.x += dx;                                                    // we calculate the next section
        ptLineStart.y += dy;
        ptLineEnd.x   += dx;
        ptLineEnd.y   += dy;
    }
    for (i=0; i<cube_size; i++) if (rc[i]==0) return i;                         // we check whether the point was located on one of the segments
    for (i=0; i<cube_size-1; i++) if (rc[i]!=rc[i+1]) return i;                 // we check whether the point lies between some segments by checking the signs
    return cube_size-1;
}

//---------------------------------------------------------------------------
// The function checks in which section of the cube on the X axis the given point is located
// In this case, the 3-dimensional coordinates are changed to 2-dimensional coordinates (z coordinates are ignored)
//---------------------------------------------------------------------------
UINT8 getXsection(PT3D* pt3Corners, const int ptX, const int ptY)
{
    PT2D pt2Corners[4];
    for (int i=0; i<4; i++) pt2Corners[i] = pt3Corners[i];
    return getXsection(pt2Corners, ptX, ptY);
}

//---------------------------------------------------------------------------
// The function checks in which section of the cube on the Y axis the given point is located
//---------------------------------------------------------------------------
UINT8 getYsection(PT2D* ptCorners, const int ptX, const int ptY)
{
    int i, rc[cube_size];
    double dx, dy;
    PT2D ptLineStart, ptLineEnd;
    dx = (ptCorners[2].x - ptCorners[1].x)/cube_size;                           // divide the given segment into the number of sections
    dy = (ptCorners[2].y - ptCorners[1].y)/cube_size;
    ptLineStart.x = ptCorners[0].x;                                             // we calculate the first section
    ptLineStart.y = ptCorners[0].y;
    ptLineEnd.x   = ptCorners[1].x;
    ptLineEnd.y   = ptCorners[1].y;
    for (i=0; i<cube_size; i++) {
        rc[i] = lineTest(ptLineStart, ptLineEnd, ptX, ptY);                     // perform a point position test for all received segments
        ptLineStart.x += dx;                                                    // we calculate the next section
        ptLineStart.y += dy;
        ptLineEnd.x   += dx;
        ptLineEnd.y   += dy;
    }
    for (i=0; i<cube_size; i++) if (rc[i]==0) return i;                         // we check whether the point was located on one of the segments
    for (i=0; i<cube_size-1; i++) if (rc[i]!=rc[i+1]) return i;                 // we check whether the point lies between some segments by checking the signs
    return cube_size-1;
}
//---------------------------------------------------------------------------
// The function checks in which section of the cube on the Y axis the given point is located
// In this case, the 3-dimensional coordinates are changed to 2-dimensional coordinates (z coordinates are ignored)
//---------------------------------------------------------------------------
UINT8 getYsection(PT3D* pt3Corners, const int ptX, const int ptY)
{
    PT2D pt2Corners[4];
    for (int i=0; i<4; i++) pt2Corners[i] = pt3Corners[i];
    return getYsection(pt2Corners, ptX, ptY);
}

//---------------------------------------------------------------------------
// Creating the smallest piece of cube
// The sides of such a piece are checked and marked with color accordingly
// The pieces inside the cube will be marked in black
//---------------------------------------------------------------------------
TCubePiece::TCubePiece(BYTEVEC posHome)
{
    m_fRotationAngle = 0;
    m_vRotation.setX(0); m_vRotation.setY(0); m_vRotation.setZ(0);

    m_nSideColor[SD_RIGHT]  = posHome.x== (cube_size-1) ? WHITE : BLACK;
    m_nSideColor[SD_LEFT]   = posHome.x== 0             ? BLUE  : BLACK;
    m_nSideColor[SD_TOP]    = posHome.y== (cube_size-1) ? GREEN : BLACK;
    m_nSideColor[SD_BOTTOM] = posHome.y== 0             ? ORANGE : BLACK;
    m_nSideColor[SD_FRONT]  = posHome.z== (cube_size-1) ? RED    : BLACK;
    m_nSideColor[SD_BACK]   = posHome.z== 0             ? YELLOW : BLACK;
}

//---------------------------------------------------------------------------
void TCubePiece::rotateX(bool bCW)                                              // rotation of a piece of cube on the X axis
{
    SIDECOLOR nTmp;
    if (bCW) {                                                                  // or clockwise rotation
        nTmp                    = m_nSideColor[SD_TOP];
        m_nSideColor[SD_TOP]    = m_nSideColor[SD_BACK];
        m_nSideColor[SD_BACK]   = m_nSideColor[SD_BOTTOM];
        m_nSideColor[SD_BOTTOM] = m_nSideColor[SD_FRONT];
        m_nSideColor[SD_FRONT]  = nTmp;
    }
    else {
        nTmp                    = m_nSideColor[SD_TOP];
        m_nSideColor[SD_TOP]    = m_nSideColor[SD_FRONT];
        m_nSideColor[SD_FRONT]  = m_nSideColor[SD_BOTTOM];
        m_nSideColor[SD_BOTTOM] = m_nSideColor[SD_BACK];
        m_nSideColor[SD_BACK]   = nTmp;
    }
}

//---------------------------------------------------------------------------
void TCubePiece::rotateY(bool bCW)                                              // rotation of a piece of cube on the Y axis
{
    SIDECOLOR nTmp;
    if (bCW) {                                                                  // or clockwise rotation
        nTmp                    = m_nSideColor[SD_FRONT];
        m_nSideColor[SD_FRONT]  = m_nSideColor[SD_LEFT];
        m_nSideColor[SD_LEFT]   = m_nSideColor[SD_BACK];
        m_nSideColor[SD_BACK]   = m_nSideColor[SD_RIGHT];
        m_nSideColor[SD_RIGHT]  = nTmp;
    }
    else {
        nTmp                    = m_nSideColor[SD_FRONT];
        m_nSideColor[SD_FRONT]  = m_nSideColor[SD_RIGHT];
        m_nSideColor[SD_RIGHT]  = m_nSideColor[SD_BACK];
        m_nSideColor[SD_BACK]   = m_nSideColor[SD_LEFT];
        m_nSideColor[SD_LEFT]   = nTmp;
    }
}

//---------------------------------------------------------------------------
void TCubePiece::rotateZ(bool bCW)                                              // rotation of a piece of cube on the Z axis
{
    SIDECOLOR nTmp;
    if (bCW) {                                                                  // or clockwise rotation
        nTmp                    = m_nSideColor[SD_TOP];
        m_nSideColor[SD_TOP]    = m_nSideColor[SD_RIGHT];
        m_nSideColor[SD_RIGHT]  = m_nSideColor[SD_BOTTOM];
        m_nSideColor[SD_BOTTOM] = m_nSideColor[SD_LEFT];
        m_nSideColor[SD_LEFT]   = nTmp;
    }
    else {
        nTmp                    = m_nSideColor[SD_TOP];
        m_nSideColor[SD_TOP]    = m_nSideColor[SD_LEFT];
        m_nSideColor[SD_LEFT]   = m_nSideColor[SD_BOTTOM];
        m_nSideColor[SD_BOTTOM] = m_nSideColor[SD_RIGHT];
        m_nSideColor[SD_RIGHT]  = nTmp;
    }
}

//---------------------------------------------------------------------------
void TCubePiece::draw(float x,float y,float z)                                  // drawing a piece of cube
{
    glPushMatrix();                                                             // first we rotate such a piece of cube (needed for animation)
    if (m_fRotationAngle) {
        glRotatef(m_fRotationAngle, m_vRotation.x(), m_vRotation.y(), m_vRotation.z());
    }
    glTranslatef(x,y,z);                                                        // and then we start drawing in the right place
    glBegin(GL_QUADS);
        glColor3ub(MAKECOLOR(m_nSideColor[SD_RIGHT]));
        glVertex3f( 0.5f, 0.5f,-0.5f);					                        // Top Right Of The Quad (Right)
        glVertex3f( 0.5f, 0.5f, 0.5f);					                        // Top Left Of The Quad (Right)
        glVertex3f( 0.5f,-0.5f, 0.5f);					                        // Bottom Left Of The Quad (Right)
        glVertex3f( 0.5f,-0.5f,-0.5f);					                        // Bottom Right Of The Quad (Right)
        glColor3ub(MAKECOLOR(m_nSideColor[SD_BOTTOM]));
        glVertex3f( 0.5f,-0.5f, 0.5f);					                        // Top Right Of The Quad (Bottom)
        glVertex3f(-0.5f,-0.5f, 0.5f);					                        // Top Left Of The Quad (Bottom)
        glVertex3f(-0.5f,-0.5f,-0.5f);					                        // Bottom Left Of The Quad (Bottom)
        glVertex3f( 0.5f,-0.5f,-0.5f);					                        // Bottom Right Of The Quad (Bottom)
        glColor3ub(MAKECOLOR(m_nSideColor[SD_FRONT]));
        glVertex3f( 0.5f, 0.5f, 0.5f);					                        // Top Right Of The Quad (Front)
        glVertex3f(-0.5f, 0.5f, 0.5f);					                        // Top Left Of The Quad (Front)
        glVertex3f(-0.5f,-0.5f, 0.5f);					                        // Bottom Left Of The Quad (Front)
        glVertex3f( 0.5f,-0.5f, 0.5f);					                        // Bottom Right Of The Quad (Front)
        glColor3ub(MAKECOLOR(m_nSideColor[SD_TOP]));
        glVertex3f( 0.5f, 0.5f,-0.5f);					                        // Top Right Of The Quad (Top)
        glVertex3f(-0.5f, 0.5f,-0.5f);					                        // Top Left Of The Quad (Top)
        glVertex3f(-0.5f, 0.5f, 0.5f);					                        // Bottom Left Of The Quad (Top)
        glVertex3f( 0.5f, 0.5f, 0.5f);					                        // Bottom Right Of The Quad (Top)
        glColor3ub(MAKECOLOR(m_nSideColor[SD_LEFT]));
        glVertex3f(-0.5f, 0.5f, 0.5f);					                        // Top Right Of The Quad (Left)
        glVertex3f(-0.5f, 0.5f,-0.5f);					                        // Top Left Of The Quad (Left)
        glVertex3f(-0.5f,-0.5f,-0.5f);					                        // Bottom Left Of The Quad (Left)
        glVertex3f(-0.5f,-0.5f, 0.5f);					                        // Bottom Right Of The Quad (Left)
        glColor3ub(MAKECOLOR(m_nSideColor[SD_BACK]));
        glVertex3f( 0.5f,-0.5f,-0.5f);					                        // Top Right Of The Quad (Back)
        glVertex3f(-0.5f,-0.5f,-0.5f);					                        // Top Left Of The Quad (Back)
        glVertex3f(-0.5f, 0.5f,-0.5f);					                        // Bottom Left Of The Quad (Back)
        glVertex3f( 0.5f, 0.5f,-0.5f);					                        // Bottom Right Of The Quad (Back)
    glEnd();
    glColor3ub(MAKECOLOR(BLACK));
    if (m_nSideColor[SD_RIGHT]!=BLACK) {                                        // we check whether the color of the right side of the cube is not black
        glBegin(GL_LINE_LOOP);                                                  // if it is not ro, draw a square border
        glVertex3f( 0.5f, 0.5f,-0.5f);					                        // Top Right Of The Quad (Right)
        glVertex3f( 0.5f, 0.5f, 0.5f);					                        // Top Left Of The Quad (Right)
        glVertex3f( 0.5f,-0.5f, 0.5f);					                        // Bottom Left Of The Quad (Right)
        glVertex3f( 0.5f,-0.5f,-0.5f);					                        // Bottom Right Of The Quad (Right)
        glEnd();
    }
    if (m_nSideColor[SD_BOTTOM]!=BLACK) {                                       // we check whether the color of the underside of the piece of cube is not black
        glBegin(GL_LINE_LOOP);                                                  // if it is not ro, draw a square border
        glVertex3f( 0.5f,-0.5f, 0.5f);					                        // Top Right Of The Quad (Bottom)
        glVertex3f(-0.5f,-0.5f, 0.5f);					                        // Top Left Of The Quad (Bottom)
        glVertex3f(-0.5f,-0.5f,-0.5f);					                        // Bottom Left Of The Quad (Bottom)
        glVertex3f( 0.5f,-0.5f,-0.5f);					                        // Bottom Right Of The Quad (Bottom)
        glEnd();
    }
    if (m_nSideColor[SD_FRONT]!=BLACK) {                                        // we check whether the color of the front side of the cube piece is not black
        glBegin(GL_LINE_LOOP);                                                  // if it is not ro, draw a square border
        glVertex3f( 0.5f, 0.5f, 0.5f);					                        // Top Right Of The Quad (Front)
        glVertex3f(-0.5f, 0.5f, 0.5f);					                        // Top Left Of The Quad (Front)
        glVertex3f(-0.5f,-0.5f, 0.5f);					                        // Bottom Left Of The Quad (Front)
        glVertex3f( 0.5f,-0.5f, 0.5f);					                        // Bottom Right Of The Quad (Front)
        glEnd();
    }
    if (m_nSideColor[SD_TOP]!=BLACK) {                                          // we check whether the color of the upper side of the cube piece is not black
        glBegin(GL_LINE_LOOP);                                                  // if it is not ro, draw a square border
        glVertex3f( 0.5f, 0.5f,-0.5f);					                        // Top Right Of The Quad (Top)
        glVertex3f(-0.5f, 0.5f,-0.5f);					                        // Top Left Of The Quad (Top)
        glVertex3f(-0.5f, 0.5f, 0.5f);					                        // Bottom Left Of The Quad (Top)
        glVertex3f( 0.5f, 0.5f, 0.5f);					                        // Bottom Right Of The Quad (Top)
        glEnd();
    }
    if (m_nSideColor[SD_LEFT]!=BLACK) {                                         // we check whether the color of the left side of the cube is not black
        glBegin(GL_LINE_LOOP);                                                  // if it is not ro, draw a square border
        glVertex3f(-0.5f, 0.5f, 0.5f);					                        // Top Right Of The Quad (Left)
        glVertex3f(-0.5f, 0.5f,-0.5f);					                        // Top Left Of The Quad (Left)
        glVertex3f(-0.5f,-0.5f,-0.5f);					                        // Bottom Left Of The Quad (Left)
        glVertex3f(-0.5f,-0.5f, 0.5f);					                        // Bottom Right Of The Quad (Left)
        glEnd();
    }
    if (m_nSideColor[SD_BACK]!=BLACK) {                                         // we check whether the color of the back side of the piece of cube is not black
        glBegin(GL_LINE_LOOP);                                                  // if it is not ro, draw a square border
        glVertex3f( 0.5f,-0.5f,-0.5f);					                        // Top Right Of The Quad (Back)
        glVertex3f(-0.5f,-0.5f,-0.5f);					                        // Top Left Of The Quad (Back)
        glVertex3f(-0.5f, 0.5f,-0.5f);					                        // Bottom Left Of The Quad (Back)
        glVertex3f( 0.5f, 0.5f,-0.5f);					                        // Bottom Right Of The Quad (Back)
        glEnd();
    }
    glPopMatrix();
}

//---------------------------------------------------------------------------
TCube::TCube(OGLWidget *widget)
{
    this->widget = widget;
    memset(m_pPieces, 0, sizeof(TCubePiece*)*cube_size*cube_size*cube_size);    // we create zero pointers to the pieces of the cube
    reset();
    //Random();
}

//---------------------------------------------------------------------------
TCube::~TCube()
{
    for (int x=0; x<cube_size; x++) {
        for (int y=0; y<cube_size; y++) {
            for (int z=0; z<cube_size; z++) {
                if (m_pPieces[x][y][z]) delete m_pPieces[x][y][z];              // we create the appropriate piece of the cube and save its index in the piece matrix
            }
        }
    }
}

//---------------------------------------------------------------------------
void TCube::reset(void)                                                         // cube reset - all colors arranged
{
    for (int x=0; x<cube_size; x++) {
        for (int y=0; y<cube_size; y++) {
            for (int z=0; z<cube_size; z++) {
                if (m_pPieces[x][y][z]) delete m_pPieces[x][y][z];              // If there were any pieces of cube, remove them
                m_pPieces[x][y][z] = new TCubePiece(BYTEVEC(x,y,z));            // create new cube pieces in the correct position
            }
        }
    }
}

//---------------------------------------------------------------------------
void TCube::random(void)                                                        // randomly rearranging the cube
{
    bool bCW;
    UINT8 nSection, nAxis;

    QTime time = QTime::currentTime();
    qsrand((uint)time.msec());
    for (int i=0; i<100; i++) {                                                 // We randomly rearrange the sections of the cube 100 times
        bCW = (bool)(qrand() % 2);                                              // we randomly choose whether the rotation should be clockwise or not
        nSection =  qrand() % cube_size;                                        // randomly choose a section number
        nAxis = qrand() % 3;                                                    // randomly choose axis
        if (nAxis==0) rotateXSection(nSection, bCW, FALSE);                     // we translate the cube sections on the X axis
        else if (nAxis==1) rotateYSection(nSection, bCW, FALSE);                // we translate the cube sections on the Y axis
        else rotateZSection(nSection, bCW, FALSE);                              // we translate the cube sections on the Z axis
    }

    blueEdgeOrientation = false;
}

//---------------------------------------------------------------------------
// Cube section rotation based on mouse direction and display window size
//---------------------------------------------------------------------------
bool TCube::rotate(GLdouble* mxProjection, GLdouble* mxModel, GLint* nViewPort,
            int wndSizeX, int wndSizeY, int ptMouseWndX, int ptMouseWndY, int ptLastMouseWndX, int ptLastMouseWndY, OGLWidget *widget)
{
    int i;
    double fTmp, fMinZ = DBL_MAX;
    float xMin=FLT_MAX, yMin=FLT_MAX;
    float xMax=FLT_MIN, yMax=FLT_MIN;
    int ptMouseX=ptMouseWndX, ptMouseY=wndSizeY-ptMouseWndY;                    // We change the y direction of the mouse to match the display coordinates
    int ptLastMouseX=ptLastMouseWndX, ptLastMouseY=wndSizeY-ptLastMouseWndY;

    (void)wndSizeX;
    this->widget = widget;
    SIDE nSide = (SIDE)-1;
    // First, we check whether the mouse is on the cube or not
    // We save all edge points of the designed cube into a variable
    PT3D vCubeCorner[8];
    // Front wall
    gluProject( cube_size/2.0, cube_size/2.0, cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[0].x, &vCubeCorner[0].y, &vCubeCorner[0].z);
    gluProject( cube_size/2.0,-cube_size/2.0, cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[1].x, &vCubeCorner[1].y, &vCubeCorner[1].z);
    gluProject(-cube_size/2.0,-cube_size/2.0, cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[2].x, &vCubeCorner[2].y, &vCubeCorner[2].z);
    gluProject(-cube_size/2.0, cube_size/2.0, cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[3].x, &vCubeCorner[3].y, &vCubeCorner[3].z);
    // Back wall
    gluProject( cube_size/2.0, cube_size/2.0,-cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[4].x, &vCubeCorner[4].y, &vCubeCorner[4].z);
    gluProject( cube_size/2.0,-cube_size/2.0,-cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[5].x, &vCubeCorner[5].y, &vCubeCorner[5].z);
    gluProject(-cube_size/2.0,-cube_size/2.0,-cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[6].x, &vCubeCorner[6].y, &vCubeCorner[6].z);
    gluProject(-cube_size/2.0, cube_size/2.0,-cube_size/2.0, mxModel, mxProjection, nViewPort, &vCubeCorner[7].x, &vCubeCorner[7].y, &vCubeCorner[7].z);

    for (i=0; i<8; i++) {                                                       // we calculate the minimum and maximum X and Y coordinates to check the mouse position
        xMin = std::min(xMin, (float)vCubeCorner[i].x);
        yMin = std::min(yMin, (float)vCubeCorner[i].y);
        xMax = std::max(xMax, (float)vCubeCorner[i].x);
        yMax = std::max(yMax, (float)vCubeCorner[i].y);
    }
    if (!(xMin<=ptLastMouseX && ptLastMouseX<=xMax &&
          yMin<=ptLastMouseY && ptLastMouseY<=yMax)) {                          // we check whether the mouse pointer is on the cube
        return FALSE;                                                           // if not, we leave
    }
    // We build an array consisting of all the edges of the cube
    PT3D vCorner[6][4] = {
        {vCubeCorner[5], vCubeCorner[1], vCubeCorner[0], vCubeCorner[4]},       // Right
        {vCubeCorner[6], vCubeCorner[2], vCubeCorner[3], vCubeCorner[7]},       // Left
        {vCubeCorner[7], vCubeCorner[4], vCubeCorner[0], vCubeCorner[3]},       // Up
        {vCubeCorner[6], vCubeCorner[5], vCubeCorner[1], vCubeCorner[2]},       // Bottom
        {vCubeCorner[2], vCubeCorner[1], vCubeCorner[0], vCubeCorner[3]},       // Front
        {vCubeCorner[6], vCubeCorner[5], vCubeCorner[4], vCubeCorner[7]},       // Rear
        };

    // we check whether the mouse pointer was on any side of the cube
    for (i=0; i<6; i++) {
        if (poly4InsideTest(vCorner[i], ptLastMouseX, ptLastMouseY)) {
            fTmp = std::min(std::min(std::min(vCorner[i][0].z, vCorner[i][1].z), vCorner[i][2].z), vCorner[i][3].z);
            if (fTmp < fMinZ) {
                nSide = (SIDE)i;
                fMinZ = fTmp;
            }
        }
    }

    if ((int)nSide==-1) return FALSE;                                           // the mouse pointer was not on any side of the cube

    // Once we have checked the side of the cube, we now check the direction of rotation of the section
    QVector2D vX((float)(vCorner[nSide][1].x - vCorner[nSide][0].x),(float)(vCorner[nSide][1].y - vCorner[nSide][0].y));
    QVector2D vY((float)(vCorner[nSide][2].x - vCorner[nSide][1].x),(float)(vCorner[nSide][2].y - vCorner[nSide][1].y));
    QVector2D vMouse((float)(ptMouseX-ptLastMouseX),(float)(ptMouseY-ptLastMouseY));    // we calculate the mouse displacement vector
    if (vMouse.length() < g_fMinMouseLength) return FALSE;                      // we check whether the mouse movement is not too small, if so, we cancel this movement
    vX.normalize();                                                             // we check the direction of movement by checking
    vY.normalize();                                                             // mouse movement vector
    vMouse.normalize();                                                         // and the X/Y vector of the cube side

    float xDiff, yDiff;
    xDiff = Vec2DAngle(vX, vMouse); if (_isnan(xDiff)) return FALSE;
    yDiff = Vec2DAngle(vY, vMouse); if (_isnan(yDiff)) return FALSE;
    float minDiff = (float)std::min(std::min(std::min(fabs(xDiff), fabs(yDiff)), fabs(xDiff-180)), fabs(yDiff-180));
    if (minDiff > g_fMaxMouseAngle) return FALSE;                                   // If the kata turns out to be too large to qualify the direction of rotation, we cancel it

    // Now we check which section of the cube should be rotated
    UINT8 nSection;
    minDiff += ALMOST_ZERO;                                                     // we increase the value to compare with the original value
    if (fabs(xDiff) <= minDiff) {                                               // we check whether the angle of the mouse movement vector is consistent with any of the angles of the page vector
       nSection = getYsection(vCorner[nSide], ptLastMouseX, ptLastMouseY);      // we calculate the section number to be rotated
       switch (nSide) {
          case SD_FRONT:    rotateYSection(nSection, TRUE, TRUE);  break;
          case SD_BACK:     rotateYSection(nSection, FALSE, TRUE); break;
          case SD_LEFT:     rotateYSection(nSection, TRUE, TRUE);  break;
          case SD_RIGHT:    rotateYSection(nSection, FALSE, TRUE); break;
          case SD_TOP:      rotateZSection(nSection, FALSE, TRUE); break;
          case SD_BOTTOM:   rotateZSection(nSection, TRUE, TRUE);  break;
       }
    }
    else if (fabs(xDiff-180) <= minDiff) {                                      // we check whether the angle of the mouse movement vector is consistent with any of the angles of the page vector
       nSection = getYsection(vCorner[nSide], ptLastMouseX, ptLastMouseY);      // we calculate the section number to be rotated
       switch (nSide) {
          case SD_FRONT:    rotateYSection(nSection, FALSE, TRUE); break;
          case SD_BACK:     rotateYSection(nSection, TRUE, TRUE);  break;
          case SD_LEFT:     rotateYSection(nSection, FALSE, TRUE); break;
          case SD_RIGHT:    rotateYSection(nSection, TRUE, TRUE);  break;
          case SD_TOP:      rotateZSection(nSection, TRUE, TRUE);  break;
          case SD_BOTTOM:   rotateZSection(nSection, FALSE, TRUE); break;
       }
    }
    else if (fabs(yDiff) <= minDiff) {                                          // we check whether the angle of the mouse movement vector is consistent with any of the angles of the page vector
       nSection = getXsection(vCorner[nSide], ptLastMouseX, ptLastMouseY);      // we calculate the section number to be rotated
       switch (nSide) {
          case SD_FRONT:    rotateXSection(nSection, FALSE, TRUE); break;
          case SD_BACK:     rotateXSection(nSection, TRUE,  TRUE); break;
          case SD_LEFT:     rotateZSection(nSection, FALSE, TRUE); break;
          case SD_RIGHT:    rotateZSection(nSection, TRUE, TRUE);  break;
          case SD_TOP:      rotateXSection(nSection, TRUE,  TRUE); break;
          case SD_BOTTOM:   rotateXSection(nSection, FALSE, TRUE); break;
       }
    }
    else if (fabs(yDiff-180) <= minDiff) {                                      // we check whether the angle of the mouse movement vector is consistent with any of the angles of the page vector
       nSection = getXsection(vCorner[nSide], ptLastMouseX, ptLastMouseY);      // we calculate the section number to be rotated
       switch (nSide) {
          case SD_FRONT:    rotateXSection(nSection, TRUE,  TRUE); break;
          case SD_BACK:     rotateXSection(nSection, FALSE, TRUE); break;
          case SD_LEFT:     rotateZSection(nSection, TRUE, TRUE);  break;
          case SD_RIGHT:    rotateZSection(nSection, FALSE, TRUE); break;
          case SD_TOP:      rotateXSection(nSection, FALSE, TRUE); break;
          case SD_BOTTOM:   rotateXSection(nSection, TRUE,  TRUE); break;
       }
    }
    else return false;
    return TRUE;
}

//---------------------------------------------------------------------------
void TCube::rotateXSection(UINT8 nSection, BOOL bCW, BOOL bAnimate)             // rotation of the cube sections on the X axis
{
    int i, j, x=nSection, y, z, size;
    TCubePiece* TmpPiece[cube_size-1];
    float fAngle = bCW ? 90.0f : -90.0f;
    if (nSection>=cube_size) return;
    TCubePiece* pieces[cube_size*cube_size];
    for (i=0, y=0; y<cube_size; y++)
        for (z=0; z<cube_size; z++) pieces[i++]=m_pPieces[x][y][z];             // we remember which pieces of the cube will be rotated
    if (bAnimate) animateRotation(pieces, ELEMENTS_OF(pieces), QVector3D(1,0,0), fAngle); // we animate the rotation of these pieces
    for (i=0; i<(int)ELEMENTS_OF(pieces); i++) pieces[i]->rotateX(bCW);         // We rotate each piece separately
    if (bCW) {                                                                  // or rotation of the cube sections clockwise
        size = cube_size-1;                                                     // we move the pieces of the cube section clockwise
        for (j=0; j<cube_size/2; j++) {
            for (i=0; i<size; i++) TmpPiece[i] = m_pPieces[x][j][i+j];
            for (i=0; i<size; i++) m_pPieces[x][j][i+j] = m_pPieces[x][i+j][cube_size-1-j];
            for (i=0; i<size; i++) m_pPieces[x][i+j][cube_size-1-j] = m_pPieces[x][cube_size-1-j][cube_size-1-j-i];
            for (i=0; i<size; i++) m_pPieces[x][cube_size-1-j][cube_size-1-j-i] = m_pPieces[x][cube_size-1-j-i][j];
            for (i=0; i<size; i++) m_pPieces[x][cube_size-1-j-i][j] = TmpPiece[i];
            size -= 2;
        }
    }
    else {                                                                      // we move the pieces of the cube section counterclockwise
        size = cube_size-1;
        for (j=0; j<cube_size/2; j++) {
            for (i=0; i<size; i++) TmpPiece[i] = m_pPieces[x][j][i+j];
            for (i=0; i<size; i++) m_pPieces[x][j][i+j] = m_pPieces[x][cube_size-1-j-i][j];
            for (i=0; i<size; i++) m_pPieces[x][cube_size-1-j-i][j] = m_pPieces[x][cube_size-1-j][cube_size-1-j-i];
            for (i=0; i<size; i++) m_pPieces[x][cube_size-1-j][cube_size-1-j-i] = m_pPieces[x][i+j][cube_size-1-j];
            for (i=0; i<size; i++) m_pPieces[x][i+j][cube_size-1-j] = TmpPiece[i];
            size -= 2;
        }
    }
}

//---------------------------------------------------------------------------
void TCube::rotateYSection(UINT8 nSection, BOOL bCW, BOOL bAnimate)             // rotation of the cube sections on the Y axis
{
    int i, j, x, y=nSection, z, size;
    TCubePiece* TmpPiece[cube_size-1];
    float fAngle = bCW ? 90.0f : -90.0f;
    if (nSection>=cube_size) return;
    TCubePiece* pieces[cube_size*cube_size];
    for (i=0, x=0; x<cube_size; x++)
        for (z=0; z<cube_size; z++) pieces[i++]=m_pPieces[x][y][z];             // we remember which pieces of the cube will be rotated
    if (bAnimate) animateRotation(pieces, ELEMENTS_OF(pieces), QVector3D(0,1,0), fAngle); // we animate the rotation of these pieces
    for (i=0; i<(int)ELEMENTS_OF(pieces); i++) pieces[i]->rotateY(bCW);         // We rotate each piece separately
    if (bCW) {                                                                  // or rotation of the cube sections clockwise
        size = cube_size-1;                                                     // we move the pieces of the cube section clockwise
        for (j=0; j<cube_size/2; j++) {
            for (i=0; i<size; i++) TmpPiece[i] = m_pPieces[j][y][i+j];
            for (i=0; i<size; i++) m_pPieces[j][y][i+j] = m_pPieces[cube_size-1-j-i][y][j];
            for (i=0; i<size; i++) m_pPieces[cube_size-1-j-i][y][j] = m_pPieces[cube_size-1-j][y][cube_size-1-j-i];
            for (i=0; i<size; i++) m_pPieces[cube_size-1-j][y][cube_size-1-j-i] = m_pPieces[i+j][y][cube_size-1-j];
            for (i=0; i<size; i++) m_pPieces[i+j][y][cube_size-1-j] = TmpPiece[i];
            size -= 2;
        }
    }
    else {                                                                      // we move the pieces of the cube section counterclockwise
        size = cube_size-1;
        for (j=0; j<cube_size/2; j++) {
            for (i=0; i<size; i++) TmpPiece[i] = m_pPieces[j][y][i+j];
            for (i=0; i<size; i++) m_pPieces[j][y][i+j] = m_pPieces[i+j][y][cube_size-1-j];
            for (i=0; i<size; i++) m_pPieces[i+j][y][cube_size-1-j] = m_pPieces[cube_size-1-j][y][cube_size-1-j-i];
            for (i=0; i<size; i++) m_pPieces[cube_size-1-j][y][cube_size-1-j-i] = m_pPieces[cube_size-1-j-i][y][j];
            for (i=0; i<size; i++) m_pPieces[cube_size-1-j-i][y][j] = TmpPiece[i];
            size -= 2;
        }
    }
}

//---------------------------------------------------------------------------
void TCube::rotateZSection(UINT8 nSection, BOOL bCW, BOOL bAnimate)             // rotation of the cube sections on the Z axis
{
    int i, j, x, y, z=nSection, size;
    TCubePiece* TmpPiece[cube_size-1];
    float fAngle = bCW ? 90.0f : -90.0f;
    if (nSection>=cube_size) return;
    TCubePiece* pieces[cube_size*cube_size];
    for (i=0, x=0; x<cube_size; x++)
        for (y=0; y<cube_size; y++) pieces[i++]=m_pPieces[x][y][z];             // we remember which pieces of the cube will be rotated
    if (bAnimate) animateRotation(pieces, ELEMENTS_OF(pieces), QVector3D(0,0,1), fAngle); // we animate the rotation of these pieces
    for (i=0; i<(int)ELEMENTS_OF(pieces); i++) pieces[i]->rotateZ(bCW);         // We rotate each piece separately
    if (bCW) {                                                                  // or rotation of the cube sections clockwise
        size = cube_size-1;                                                     // we move the pieces of the cube section clockwise
        for (j=0; j<cube_size/2; j++) {
            for (i=0; i<size; i++) TmpPiece[i] = m_pPieces[j][i+j][z];
            for (i=0; i<size; i++) m_pPieces[j][i+j][z] = m_pPieces[i+j][cube_size-1-j][z];
            for (i=0; i<size; i++) m_pPieces[i+j][cube_size-1-j][z] = m_pPieces[cube_size-1-j][cube_size-1-j-i][z];
            for (i=0; i<size; i++) m_pPieces[cube_size-1-j][cube_size-1-j-i][z] = m_pPieces[cube_size-1-j-i][j][z];
            for (i=0; i<size; i++) m_pPieces[cube_size-1-j-i][j][z] = TmpPiece[i];
            size -= 2;
        }
    }
    else {                                                                      // we move the pieces of the cube section counterclockwise
        size = cube_size-1;
        for (j=0; j<cube_size/2; j++) {
            for (i=0; i<size; i++) TmpPiece[i] = m_pPieces[j][i+j][z];
            for (i=0; i<size; i++) m_pPieces[j][i+j][z] = m_pPieces[cube_size-1-j-i][j][z];
            for (i=0; i<size; i++) m_pPieces[cube_size-1-j-i][j][z] = m_pPieces[cube_size-1-j][cube_size-1-j-i][z];
            for (i=0; i<size; i++) m_pPieces[cube_size-1-j][cube_size-1-j-i][z] = m_pPieces[i+j][cube_size-1-j][z];
            for (i=0; i<size; i++) m_pPieces[i+j][cube_size-1-j][z] = TmpPiece[i];
            size -= 2;
        }
    }
}

//---------------------------------------------------------------------------
void TCube::animateRotation(TCubePiece* piece[], int ctPieces, QVector3D v, float fAngle)
{
    int i,x;

    if (g_nRotationSteps <= 1) return;
    for (i=0; (UINT)i<g_nRotationSteps; i++) {                                  // execute individual animation frames in a loop
        float fRotAngle = fAngle * i/g_nRotationSteps;                          // calculate the appropriate angle of inclination of the cube section
        for (int x=0; x<ctPieces; x++) piece[x]->setRotation(fRotAngle, v);     // and write down the angle for all pieces in this section of the cube
        if (widget) {
            widget->updateGL();
        }
    }
    for (x=0; x<ctPieces; x++) piece[x]->clrRotation();                         // at the end of the animation, reset the inclination angles of all pieces from the rotated cube section
}

//---------------------------------------------------------------------------
void TCube::draw(void)                                                          // drawing the whole cube
{
    int x, y, z;
    float posx, posy, posz;
    posx = -(cube_size-1)/2.0;                                                  // We initially count the x positions for a piece of the cube
    for (x=0; x<cube_size; x++) {
        posy = -(cube_size-1)/2.0;                                              // We initially count the y positions for a piece of the cube
        for (y=0; y<cube_size; y++) {
            posz = -(cube_size-1)/2.0;                                          // We initially count the z positions for a piece of the cube
            for (z=0; z<cube_size; z++) {
                m_pPieces[x][y][z]->draw(posx, posy, posz);                     // draw the appropriate piece of the cube in the designated position
                posz += 1.0;                                                    // increase position z piece of cubez
            }
            posy += 1.0;                                                        // increase position y piece of cubez
        }
        posx += 1.0;                                                            // increase position x piece of cube
    }
}

//---------------------------------------------------------------------------
bool TCube::check(void)                                                         // checks whether the cube has been solved
{
    int x, y, z;
    SIDECOLOR sidecolor;
    x=(cube_size-1);
    sidecolor = m_pPieces[x][0][0]->m_nSideColor[SD_RIGHT];
    for (y=0; y<cube_size; y++)
        for (z=0; z<cube_size; z++) if (m_pPieces[x][y][z]->m_nSideColor[SD_RIGHT]!=sidecolor) return false;
    x=0;
    sidecolor = m_pPieces[x][0][0]->m_nSideColor[SD_LEFT];
    for (y=0; y<cube_size; y++)
        for (z=0; z<cube_size; z++) if (m_pPieces[x][y][z]->m_nSideColor[SD_LEFT]!=sidecolor) return false;
    y=(cube_size-1);
    sidecolor = m_pPieces[0][y][0]->m_nSideColor[SD_TOP];
    for (x=0; x<cube_size; x++)
        for (z=0; z<cube_size; z++) if (m_pPieces[x][y][z]->m_nSideColor[SD_TOP]!=sidecolor) return false;
    y=0;
    sidecolor = m_pPieces[0][y][0]->m_nSideColor[SD_BOTTOM];
    for (x=0; x<cube_size; x++)
        for (z=0; z<cube_size; z++) if (m_pPieces[x][y][z]->m_nSideColor[SD_BOTTOM]!=sidecolor) return false;
    z=(cube_size-1);
    sidecolor = m_pPieces[0][0][z]->m_nSideColor[SD_FRONT];
    for (x=0; x<cube_size; x++)
        for (y=0; y<cube_size; y++) if (m_pPieces[x][y][z]->m_nSideColor[SD_FRONT]!=sidecolor) return false;
    z=0;
    sidecolor = m_pPieces[0][0][z]->m_nSideColor[SD_BACK];
    for (x=0; x<cube_size; x++)
        for (y=0; y<cube_size; y++) if (m_pPieces[x][y][z]->m_nSideColor[SD_BACK]!=sidecolor) return false;
    return true;
}

//---------------------------------------------------------------------------
SIDE TCube::findWhiteCrossSide(void)
{
    if (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] == WHITE) return SD_RIGHT;
    if (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] == WHITE) return SD_LEFT;
    if (m_pPieces[cube_mid_pos][cube_size-1][cube_mid_pos]->m_nSideColor[SD_TOP] == WHITE) return SD_TOP;
    if (m_pPieces[cube_mid_pos][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM] == WHITE) return SD_BOTTOM;
    if (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) return SD_FRONT;
    if (m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] == WHITE) return SD_BACK;
    return (SIDE)-1;
}

//---------------------------------------------------------------------------
bool TCube::checkWhiteCross(SIDE whiteCrossSide)
{
    switch (whiteCrossSide) {
    case SD_RIGHT  :
        if (m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        break;
    case SD_LEFT   :
        if (m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT] != WHITE) return false;
        if (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT] != WHITE) return false;
        if (m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT] != WHITE) return false;
        if (m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT] != WHITE) return false;
        break;
    case SD_TOP    :
        if (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_TOP] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_TOP] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_TOP] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] != WHITE) return false;
        break;
    case SD_BOTTOM :
        if (m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        break;
    case SD_FRONT  :
        if (m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        break;
    case SD_BACK   :
        if (m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        if (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        break;
    }
    return true;
}

//---------------------------------------------------------------------------
bool TCube::checkWhiteCrossCorners(SIDE whiteCrossSide)
{
    switch (whiteCrossSide) {
    case SD_RIGHT  :
        if (m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_RIGHT] != WHITE) return false;
        break;
    case SD_LEFT   :
        if (m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_LEFT] != WHITE) return false;
        if (m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_LEFT] != WHITE) return false;
        if (m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT] != WHITE) return false;
        if (m_pPieces[0][0][0]->m_nSideColor[SD_LEFT] != WHITE) return false;
        break;
    case SD_TOP    :
        if (m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_TOP] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_TOP] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] != WHITE) return false;
        if (m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] != WHITE) return false;
        break;
    case SD_BOTTOM :
        if (m_pPieces[0][0][0]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        if (m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_BOTTOM] != WHITE) return false;
        break;
    case SD_FRONT  :
        if (m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        if (m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        if (m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT] != WHITE) return false;
        break;
    case SD_BACK   :
        if (m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        if (m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        if (m_pPieces[0][0][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        if (m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BACK] != WHITE) return false;
        break;
    }
    return true;
}

//---------------------------------------------------------------------------
bool TCube::checkSecondLayer(SIDE whiteCrossSide)
{
    SIDECOLOR color;
    (void)whiteCrossSide;

    color = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    if (color != m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT]) return false;
    if (color != m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT]) return false;

    color = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    if (color != m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT]) return false;
    if (color != m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT]) return false;

    color = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
    if (color != m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) return false;
    if (color != m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) return false;

    color = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    if (color != m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK]) return false;
    if (color != m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK]) return false;

    return true;
}

//---------------------------------------------------------------------------
bool TCube::checkBlueCross(SIDE whiteCrossSide)
{
    (void)whiteCrossSide;
    if (BLUE != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) return false;
    if (BLUE != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) return false;
    if (BLUE != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) return false;
    if (BLUE != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) return false;

    return true;
}

//---------------------------------------------------------------------------
bool TCube::checkEdgePermutationOfBlueCross(SIDE whiteCrossSide)
{
    (void)whiteCrossSide;
    if (m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) return false;
    if (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) return false;
    if (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) return false;
    if (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT]) return false;

    return true;
}

//---------------------------------------------------------------------------
bool TCube::checkPermutationOfBlueCorners(SIDE whiteCrossSide)
{
    SIDECOLOR color1, color2, color3;
    SIDECOLOR colorBottom, colorLeft, colorBack, colorRight, colorFront;

    (void)whiteCrossSide;
    colorBottom = BLUE;
    colorLeft   = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    colorBack   = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    colorRight  = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    colorFront  = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];

    color1 = m_pPieces[0][0][0]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][0]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][0]->m_nSideColor[SD_BACK];

    if (!((color1 == colorBottom) || (color1 == colorLeft) || (color1 == colorBack))) return false;
    if (!((color2 == colorBottom) || (color2 == colorLeft) || (color2 == colorBack))) return false;
    if (!((color3 == colorBottom) || (color3 == colorLeft) || (color3 == colorBack))) return false;

    color1 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_RIGHT];
    color3 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BACK];

    if (!((color1 == colorBottom) || (color1 == colorRight) || (color1 == colorBack))) return false;
    if (!((color2 == colorBottom) || (color2 == colorRight) || (color2 == colorBack))) return false;
    if (!((color3 == colorBottom) || (color3 == colorRight) || (color3 == colorBack))) return false;

    color1 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_RIGHT];
    color3 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if (!((color1 == colorBottom) || (color1 == colorRight) || (color1 == colorFront))) return false;
    if (!((color2 == colorBottom) || (color2 == colorRight) || (color2 == colorFront))) return false;
    if (!((color3 == colorBottom) || (color3 == colorRight) || (color3 == colorFront))) return false;

    color1 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if (!((color1 == colorBottom) || (color1 == colorLeft) || (color1 == colorFront))) return false;
    if (!((color2 == colorBottom) || (color2 == colorLeft) || (color2 == colorFront))) return false;
    if (!((color3 == colorBottom) || (color3 == colorLeft) || (color3 == colorFront))) return false;

    return true;
}

//---------------------------------------------------------------------------
bool TCube::checkOrientationOfBlueCorners(SIDE whiteCrossSide)
{
    SIDECOLOR color1, color2, color3;
    SIDECOLOR colorBottom, colorLeft, colorBack, colorRight, colorFront;

    (void)whiteCrossSide;
    colorBottom = BLUE;
    colorLeft   = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    colorBack   = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    colorRight  = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    colorFront  = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];

    color1 = m_pPieces[0][0][0]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][0]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][0]->m_nSideColor[SD_BACK];

    if (color1 != colorBottom) return false;
    if (color2 != colorLeft) return false;
    if (color3 != colorBack) return false;

    color1 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_RIGHT];
    color3 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BACK];

    if (color1 != colorBottom) return false;
    if (color2 != colorRight) return false;
    if (color3 != colorBack) return false;

    color1 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_RIGHT];
    color3 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if (color1 != colorBottom) return false;
    if (color2 != colorRight) return false;
    if (color3 != colorFront) return false;

    color1 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if (color1 != colorBottom) return false;
    if (color2 != colorLeft) return false;
    if (color3 != colorFront) return false;

    return true;
}

//---------------------------------------------------------------------------
void TCube::whiteCrossSideToTop(SIDE whiteCrossSide)
{
    switch (whiteCrossSide) {
    case SD_RIGHT  : rotateZSection(cube_mid_pos, TRUE, TRUE); break;
    case SD_LEFT   : rotateZSection(cube_mid_pos, FALSE, TRUE); break;
    case SD_TOP    : break;
    case SD_BOTTOM : rotateXSection(cube_mid_pos, TRUE, TRUE); break;
    case SD_FRONT  : rotateXSection(cube_mid_pos, FALSE, TRUE); break;
    case SD_BACK   : rotateXSection(cube_mid_pos, TRUE, TRUE); break;
    }
}

//---------------------------------------------------------------------------
void TCube::arrangeWhiteCross(void)
{
    bool RotDownSide = false;
    SIDECOLOR color;

    // SD_BOTTOM
    if (m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM] == WHITE) {            // bottom
        color = m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT];
        if (color == m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) {
            moves.append(ROT_F);
            moves.append(ROT_F);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM] == WHITE) {                      // up
        color = m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK];
        if (color == m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK]) {
            moves.append(ROT_B);
            moves.append(ROT_B);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM] == WHITE) {            // right
        color = m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT];
        if (color == m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
            moves.append(ROT_R);
            moves.append(ROT_R);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM] == WHITE) {                      // left
        color = m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT];
        if (color == m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
            moves.append(ROT_L);
            moves.append(ROT_L);
            return;
        }
        else {
            RotDownSide = true;
        }
    }

    if (RotDownSide) {
        moves.append(ROT_D);
        return;
    }

    // SD_TOP
    if (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] == WHITE) {     // bottom
        color = m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT];
        if (color != m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) {
            moves.append(ROT_F);
            moves.append(ROT_F);
            return;
        }
    }
    if (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_TOP] == WHITE) {               // up
        color = m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK];
        if (color != m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK]) {
            moves.append(ROT_B);
            moves.append(ROT_B);
            return;
        }
    }
    if (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_TOP] == WHITE) {     // right
        color = m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT];
        if (color != m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
            moves.append(ROT_R);
            moves.append(ROT_R);
            return;
        }
    }
    if (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_TOP] == WHITE) {               // left
        color = m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT];
        if (color != m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
            moves.append(ROT_L);
            moves.append(ROT_L);
            return;
        }
    }

    // SD_RIGHT
    if (m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT] == WHITE) {             // bottom
        moves.append(ROT_R); moves.append(ROT_F); moves.append(ROT_D); moves.append(ROT_FCCW);
        return;
    }
    if (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT] == WHITE) {   // up
        moves.append(ROT_R); moves.append(ROT_BCCW); moves.append(ROT_D); moves.append(ROT_B);
        return;
    }
    if (m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT] == WHITE) {             // right
        color = m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK];
        if (color == m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK]) {
            moves.append(ROT_B);
        }
        else {
            moves.append(ROT_BCCW);
            moves.append(ROT_D);
            moves.append(ROT_B);
        }
        return;
    }
    if (m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT] == WHITE) {   // left
        color = m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
        if (color == m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) {
            moves.append(ROT_FCCW);
        }
        else {
            moves.append(ROT_F);
            moves.append(ROT_D);
            moves.append(ROT_FCCW);
        }
        return;
    }

    // SD_LEFT
    if (m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT] == WHITE) {                        // bottom
        moves.append(ROT_L); moves.append(ROT_B); moves.append(ROT_D); moves.append(ROT_BCCW);
        return;
    }
    if (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT] == WHITE) {              // up
        moves.append(ROT_L); moves.append(ROT_FCCW); moves.append(ROT_D); moves.append(ROT_F);
        return;
    }
    if (m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT] == WHITE) {              // right
        color = m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
        if (color == m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) {
            moves.append(ROT_F);
        }
        else {
            moves.append(ROT_FCCW);
            moves.append(ROT_D);
            moves.append(ROT_F);
        }
        return;
    }
    if (m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT] == WHITE) {                        // left
        color = m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK];
        if (color == m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK]) {
            moves.append(ROT_BCCW);
        }
        else {
            moves.append(ROT_B);
            moves.append(ROT_D);
            moves.append(ROT_BCCW);
        }
        return;
    }

    // SD_FRONT
    if (m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) {             // bottom
        moves.append(ROT_F); moves.append(ROT_L); moves.append(ROT_D); moves.append(ROT_LCCW);
        return;
    }
    if (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) {   // up
        moves.append(ROT_F); moves.append(ROT_RCCW); moves.append(ROT_D); moves.append(ROT_R);
        return;
    }
    if (m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) {   // right
        color = m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT];
        if (color == m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
            moves.append(ROT_R);
        }
        else {
            moves.append(ROT_RCCW);
            moves.append(ROT_D);
            moves.append(ROT_R);
        }
        return;
    }
    if (m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) {             // left
        color = m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT];
        if (color == m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
            moves.append(ROT_LCCW);
        }
        else {
            moves.append(ROT_L);
            moves.append(ROT_D);
            moves.append(ROT_LCCW);
        }
        return;
    }

    // SD_BACK
    if (m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK] == WHITE) {                        // bottom
        moves.append(ROT_B); moves.append(ROT_R); moves.append(ROT_D); moves.append(ROT_RCCW);
        return;
    }
    if (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK] == WHITE) {              // up
        moves.append(ROT_B); moves.append(ROT_LCCW); moves.append(ROT_D); moves.append(ROT_L);
        return;
    }
    if (m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK] == WHITE) {                        // right
        color = m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT];
        if (color == m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
            moves.append(ROT_L);
        }
        else {
            moves.append(ROT_LCCW);
            moves.append(ROT_D);
            moves.append(ROT_L);
        }
        return;
    }
    if (m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK] == WHITE) {              // left
        color = m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT];
        if (color == m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
            moves.append(ROT_RCCW);
        }
        else {
            moves.append(ROT_R);
            moves.append(ROT_D);
            moves.append(ROT_RCCW);
        }
        return;
    }

}

//---------------------------------------------------------------------------
void TCube::arrangeWhiteCrossCorners(void)
{
    SIDECOLOR color;
    bool RotDownSide = false;

    secondLayerBottomRotations = 0;

    // SD_RIGHT
    color = m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_RIGHT];
    if ((color == WHITE) ||
        ((m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT] != color)))  {  //TL
        moves.append(ROT_RCCW); moves.append(ROT_DCCW); moves.append(ROT_R);
        return;
    }
    color = m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_RIGHT];
    if ((color == WHITE) ||
        ((m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT] != color)))  {  //TR
        moves.append(ROT_R); moves.append(ROT_D); moves.append(ROT_RCCW);
        return;
    }
    if (m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_RIGHT] == WHITE) {                            // BR
        color = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BACK];
        if (color == (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK])) {
            moves.append(ROT_DCCW); moves.append(ROT_BCCW); moves.append(ROT_D); moves.append(ROT_B);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_RIGHT] == WHITE) {                  // BL
        color = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_FRONT];
        if (color == (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT])) {
            moves.append(ROT_D); moves.append(ROT_F); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
            return;
        }
        else {
            RotDownSide = true;
        }
    }

    // SD_LEFT
    color = m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_LEFT];
    if ((color == WHITE) ||
        ((m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT] != color)))  {             // TL
        moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_L);
        return;
    }
    color = m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_LEFT];
    if ((color == WHITE) ||
        ((m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT] != color)))  {             // TR
        moves.append(ROT_L); moves.append(ROT_D); moves.append(ROT_LCCW);
        return;
    }
    if (m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT] == WHITE) {                             // BR
        color = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT];
        if (color == (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT])) {
            moves.append(ROT_DCCW); moves.append(ROT_FCCW); moves.append(ROT_D); moves.append(ROT_F);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[0][0][0]->m_nSideColor[SD_LEFT] == WHITE) {                                       // BL
        color = m_pPieces[0][0][0]->m_nSideColor[SD_BACK];
        if (color == (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK])) {
            moves.append(ROT_D); moves.append(ROT_B); moves.append(ROT_DCCW); moves.append(ROT_BCCW);
            return;
        }
        else {
            RotDownSide = true;
        }
    }

    // SD_FRONT
    color = m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT];
    if ((color == WHITE) ||
        ((m_pPieces[0][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT] != color)))  {  // TL
        moves.append(ROT_FCCW); moves.append(ROT_DCCW); moves.append(ROT_F);
        return;
    }
    color = m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT];
    if ((color == WHITE) ||
        ((m_pPieces[cube_size-1][cube_size-1][cube_size-1]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[cube_mid_pos][cube_size-1][cube_size-1]->m_nSideColor[SD_FRONT] != color)))  {  // TR
        moves.append(ROT_F); moves.append(ROT_D); moves.append(ROT_FCCW);
        return;
    }
    if (m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) {                  // BR
        color = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_RIGHT];
        if (color == (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT])) {
            moves.append(ROT_DCCW); moves.append(ROT_RCCW); moves.append(ROT_D); moves.append(ROT_R);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT] == WHITE) {                            // BL
        color = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT];
        if (color == (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
            moves.append(ROT_D); moves.append(ROT_L); moves.append(ROT_DCCW); moves.append(ROT_LCCW);
            return;
        }
        else {
            RotDownSide = true;
        }
    }

    // SD_BACK
    color = m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_BACK];
    if ((color == WHITE) ||
        ((m_pPieces[cube_size-1][cube_size-1][0]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK] != color)))  {             // TL
        moves.append(ROT_BCCW); moves.append(ROT_DCCW); moves.append(ROT_B);
        return;
    }
    color = m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_BACK];
    if ((color == WHITE) ||
        ((m_pPieces[0][cube_size-1][0]->m_nSideColor[SD_TOP] == WHITE) &&
         (m_pPieces[cube_mid_pos][cube_size-1][0]->m_nSideColor[SD_BACK] != color)))  {             // TR
        moves.append(ROT_B); moves.append(ROT_D); moves.append(ROT_BCCW);
        return;
    }
    if (m_pPieces[0][0][0]->m_nSideColor[SD_BACK] == WHITE) {                                       // BR
        color = m_pPieces[0][0][0]->m_nSideColor[SD_LEFT];
        if (color == (m_pPieces[0][cube_size-1][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
            moves.append(ROT_DCCW); moves.append(ROT_LCCW); moves.append(ROT_D); moves.append(ROT_L);
            return;
        }
        else {
            RotDownSide = true;
        }
    }
    if (m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BACK] == WHITE) {                             // BL
        color = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_RIGHT];
        if (color == (m_pPieces[cube_size-1][cube_size-1][cube_mid_pos]->m_nSideColor[SD_RIGHT])) {
            moves.append(ROT_D); moves.append(ROT_R); moves.append(ROT_DCCW); moves.append(ROT_RCCW);
            return;
        }
        else {
            RotDownSide = true;
        }
    }

    if (RotDownSide) {
        moves.append(ROT_D);
        return;
    }

    // SD_BOTTOM
    if (m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_BOTTOM] == WHITE) {                           // TL
        moves.append(ROT_FCCW); moves.append(ROT_D); moves.append(ROT_F);
        return;
    }
    if (m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_BOTTOM] == WHITE) {                 // TR
        moves.append(ROT_F); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
        return;
    }
    if (m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BOTTOM] == WHITE) {                           // BR
        moves.append(ROT_R); moves.append(ROT_D); moves.append(ROT_RCCW);
        return;
    }
    if (m_pPieces[0][0][0]->m_nSideColor[SD_BOTTOM] == WHITE) {                                     // BL
        moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_L);
        return;
    }
}

//---------------------------------------------------------------------------
void TCube::arrangeSecondLayer(void)
{
    SIDECOLOR color;
    SIDECOLOR color2;
    bool RotDownSide = false;

    color = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];

    if (secondLayerBottomRotations > 3) {
        if (color != m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT]) {
            moves.append(ROT_DCCW); moves.append(ROT_BCCW); moves.append(ROT_D); moves.append(ROT_B);
            moves.append(ROT_RCCW); moves.append(ROT_B); moves.append(ROT_R); moves.append(ROT_BCCW);
            return;
        }
        if (color != m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT]) {
            moves.append(ROT_D); moves.append(ROT_F); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
            moves.append(ROT_R); moves.append(ROT_FCCW); moves.append(ROT_RCCW); moves.append(ROT_F);
            return;
        }
    }


    if (color == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
        color = m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM];
        if (color != BLUE) {
            if (color == m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK]) {
                moves.append(ROT_DCCW); moves.append(ROT_BCCW); moves.append(ROT_D); moves.append(ROT_B);
                moves.append(ROT_RCCW); moves.append(ROT_B); moves.append(ROT_R); moves.append(ROT_BCCW);
                return;
            }
            if (color == m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) {
                moves.append(ROT_D); moves.append(ROT_F); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
                moves.append(ROT_R); moves.append(ROT_FCCW); moves.append(ROT_RCCW); moves.append(ROT_F);
                return;
            }
        }
    }
    else {
        RotDownSide = true;
    }

    color = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    if (color == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
        color = m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM];
        if (color != BLUE) {
            if (color == m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) {
                moves.append(ROT_DCCW); moves.append(ROT_FCCW); moves.append(ROT_D); moves.append(ROT_F);
                moves.append(ROT_LCCW); moves.append(ROT_F); moves.append(ROT_L); moves.append(ROT_FCCW);
                return;
            }
            if (color == m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK]) {
                moves.append(ROT_D); moves.append(ROT_B); moves.append(ROT_DCCW); moves.append(ROT_BCCW);
                moves.append(ROT_L); moves.append(ROT_BCCW); moves.append(ROT_LCCW); moves.append(ROT_B);
                return;
            }
        }
    }
    else {
        RotDownSide = true;
    }

    color = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
    if (color == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) {
        color = m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
        if (color != BLUE) {
            if (color == m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
                moves.append(ROT_DCCW); moves.append(ROT_RCCW); moves.append(ROT_D); moves.append(ROT_R);
                moves.append(ROT_FCCW); moves.append(ROT_R); moves.append(ROT_F); moves.append(ROT_RCCW);
                return;
            }
            if (color == m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
                moves.append(ROT_D); moves.append(ROT_L); moves.append(ROT_DCCW); moves.append(ROT_LCCW);
                moves.append(ROT_F); moves.append(ROT_LCCW); moves.append(ROT_FCCW); moves.append(ROT_L);
                return;
            }
        }
    }
    else {
        RotDownSide = true;
    }

    color = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    if (color == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) {
        color = m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM];
        if (color != BLUE) {
            if (color == m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT]) {
                moves.append(ROT_DCCW); moves.append(ROT_LCCW); moves.append(ROT_D); moves.append(ROT_L);
                moves.append(ROT_BCCW); moves.append(ROT_L); moves.append(ROT_B); moves.append(ROT_LCCW);
                return;
            }
            if (color == m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT]) {
                moves.append(ROT_D); moves.append(ROT_R); moves.append(ROT_DCCW); moves.append(ROT_RCCW);
                moves.append(ROT_B); moves.append(ROT_RCCW); moves.append(ROT_BCCW); moves.append(ROT_R);
                return;
            }
        }
    }
    else {
        RotDownSide = true;
    }


    color = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    color2 = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    if (((color == m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK]) &&
         (color2 == m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT])) ||
        ((color == m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT]) &&
         (color2 != m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK]))) {
        moves.append(ROT_DCCW); moves.append(ROT_BCCW); moves.append(ROT_D); moves.append(ROT_B);
        moves.append(ROT_RCCW); moves.append(ROT_B); moves.append(ROT_R); moves.append(ROT_BCCW);
        return;
    }
    color2 = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
    if (((color == m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) &&
         (color2 == m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT])) ||
        ((color == m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT]) &&
         (color2 != m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]))) {
        moves.append(ROT_D); moves.append(ROT_F); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
        moves.append(ROT_R); moves.append(ROT_FCCW); moves.append(ROT_RCCW); moves.append(ROT_F);
        return;
    }

    color = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    color2 = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
    if (((color == m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) &&
         (color2 == m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT]))  ||
        ((color == m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT]) &&
         (color2 != m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]))) {
        moves.append(ROT_DCCW); moves.append(ROT_FCCW); moves.append(ROT_D); moves.append(ROT_F);
        moves.append(ROT_LCCW); moves.append(ROT_F); moves.append(ROT_L); moves.append(ROT_FCCW);
        return;
    }
    color2 = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    if (((color == m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK]) &&
         (color2 == m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT]))  ||
        ((color == m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT]) &&
         (color2 != m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK]))) {
        moves.append(ROT_D); moves.append(ROT_B); moves.append(ROT_DCCW); moves.append(ROT_BCCW);
        moves.append(ROT_L); moves.append(ROT_BCCW); moves.append(ROT_LCCW); moves.append(ROT_B);
        return;
    }

    color = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];
    color2 = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    if (((color == m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT]) &&
         (color2 == m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]))  ||
        ((color == m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) &&
         (color2 != m_pPieces[cube_size-1][cube_mid_pos][cube_size-1]->m_nSideColor[SD_RIGHT]))) {
        moves.append(ROT_DCCW); moves.append(ROT_RCCW); moves.append(ROT_D); moves.append(ROT_R);
        moves.append(ROT_FCCW); moves.append(ROT_R); moves.append(ROT_F); moves.append(ROT_RCCW);
        return;
    }
    color2 = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    if (((color == m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT]) &&
         (color2 == m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]))  ||
        ((color == m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT]) &&
         (color2 != m_pPieces[0][cube_mid_pos][cube_size-1]->m_nSideColor[SD_LEFT]))) {
        moves.append(ROT_D); moves.append(ROT_L); moves.append(ROT_DCCW); moves.append(ROT_LCCW);
        moves.append(ROT_F); moves.append(ROT_LCCW); moves.append(ROT_FCCW); moves.append(ROT_L);
        return;
    }

    color = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    color2 = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    if (((color == m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT]) &&
         (color2 == m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK]))  ||
        ((color == m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_BACK]) &&
         (color2 != m_pPieces[0][cube_mid_pos][0]->m_nSideColor[SD_LEFT]))) {
        moves.append(ROT_DCCW); moves.append(ROT_LCCW); moves.append(ROT_D); moves.append(ROT_L);
        moves.append(ROT_BCCW); moves.append(ROT_L); moves.append(ROT_B); moves.append(ROT_LCCW);
        return;
    }
    color2 = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    if (((color == m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT]) &&
         (color2 == m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK]))  ||
        ((color == m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_BACK]) &&
         (color2 != m_pPieces[cube_size-1][cube_mid_pos][0]->m_nSideColor[SD_RIGHT]))) {
        moves.append(ROT_D); moves.append(ROT_R); moves.append(ROT_DCCW); moves.append(ROT_RCCW);
        moves.append(ROT_B); moves.append(ROT_RCCW); moves.append(ROT_BCCW); moves.append(ROT_R);
        return;
    }

    if (RotDownSide) {
        moves.append(ROT_D);
        secondLayerBottomRotations++;
        return;
    }
}

//---------------------------------------------------------------------------
void TCube::arrangeBlueCross(void)
{
    if ((BLUE != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_F); moves.append(ROT_L); moves.append(ROT_D);
        moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
        return;
    }

    if ((BLUE == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_F); moves.append(ROT_L); moves.append(ROT_D);
        moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
        return;
    }

    if ((BLUE != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_L); moves.append(ROT_B); moves.append(ROT_D);
        moves.append(ROT_BCCW); moves.append(ROT_DCCW); moves.append(ROT_LCCW);
        return;
    }

    if ((BLUE == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_R); moves.append(ROT_F); moves.append(ROT_D);
        moves.append(ROT_FCCW); moves.append(ROT_DCCW); moves.append(ROT_RCCW);
        return;
    }

    if ((BLUE != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_B); moves.append(ROT_R); moves.append(ROT_D);
        moves.append(ROT_RCCW); moves.append(ROT_DCCW); moves.append(ROT_BCCW);
        return;
    }

    if ((BLUE != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_F); moves.append(ROT_L); moves.append(ROT_D);
        moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
        return;
    }

    if ((BLUE == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_BOTTOM]) &&
        (BLUE != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_BOTTOM])) {

        moves.append(ROT_L); moves.append(ROT_B); moves.append(ROT_D);
        moves.append(ROT_BCCW); moves.append(ROT_DCCW); moves.append(ROT_LCCW);
        return;
    }
}

//---------------------------------------------------------------------------
void TCube::arrangeEdgePermutationOfBlueCross(void)
{
    int cnt = 0;

    if (m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) cnt++;
    if (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) cnt++;
    if (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) cnt++;
    if (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT]) cnt++;

    if (cnt < 2) {
        moves.append(ROT_D);
        return;
    }

    if ((m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) &&
        (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) &&
        (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) &&
        (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
        moves.append(ROT_F); moves.append(ROT_D); moves.append(ROT_FCCW); moves.append(ROT_D);
        moves.append(ROT_F); moves.append(ROT_D); moves.append(ROT_D); moves.append(ROT_FCCW); moves.append(ROT_D);
        return;
    }

    if ((m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) &&
        (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) &&
        (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) &&
        (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
        moves.append(ROT_L); moves.append(ROT_D); moves.append(ROT_LCCW); moves.append(ROT_D);
        moves.append(ROT_L); moves.append(ROT_D); moves.append(ROT_D); moves.append(ROT_LCCW); moves.append(ROT_D);
        return;
    }

    if ((m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) &&
        (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) &&
        (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) &&
        (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
        moves.append(ROT_B); moves.append(ROT_D); moves.append(ROT_BCCW); moves.append(ROT_D);
        moves.append(ROT_B); moves.append(ROT_D); moves.append(ROT_D); moves.append(ROT_BCCW); moves.append(ROT_D);
        return;
    }

    if ((m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) &&
        (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) &&
        (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) &&
        (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
        moves.append(ROT_R); moves.append(ROT_D); moves.append(ROT_RCCW); moves.append(ROT_D);
        moves.append(ROT_R); moves.append(ROT_D); moves.append(ROT_D); moves.append(ROT_RCCW); moves.append(ROT_D);
        return;
    }

    if ((m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] == m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) &&
        (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] == m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) &&
        (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] != m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) &&
        (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] != m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
        moves.append(ROT_L); moves.append(ROT_D); moves.append(ROT_LCCW); moves.append(ROT_D);
        moves.append(ROT_L); moves.append(ROT_D); moves.append(ROT_D); moves.append(ROT_LCCW);
        return;
    }

    if ((m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK] != m_pPieces[cube_mid_pos][0][0]->m_nSideColor[SD_BACK]) &&
        (m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT] != m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT]) &&
        (m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT] == m_pPieces[cube_size-1][0][cube_mid_pos]->m_nSideColor[SD_RIGHT]) &&
        (m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT] == m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT])) {
        moves.append(ROT_F); moves.append(ROT_D); moves.append(ROT_FCCW); moves.append(ROT_D);
        moves.append(ROT_F); moves.append(ROT_D); moves.append(ROT_D); moves.append(ROT_FCCW);
        return;
    }

}

//---------------------------------------------------------------------------
void TCube::permutationOfBlueCorners(void)
{
    SIDECOLOR color1, color2, color3;
    SIDECOLOR colorBottom, colorLeft, colorBack, colorRight, colorFront;

    colorBottom = BLUE;
    colorLeft   = m_pPieces[0][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_LEFT];
    colorBack   = m_pPieces[cube_mid_pos][cube_mid_pos][0]->m_nSideColor[SD_BACK];
    colorRight  = m_pPieces[cube_size-1][cube_mid_pos][cube_mid_pos]->m_nSideColor[SD_RIGHT];
    colorFront  = m_pPieces[cube_mid_pos][cube_mid_pos][cube_size-1]->m_nSideColor[SD_FRONT];

    color1 = m_pPieces[0][0][0]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][0]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][0]->m_nSideColor[SD_BACK];

    if (((color1 == colorBottom) || (color1 == colorLeft) || (color1 == colorBack)) &&
        ((color2 == colorBottom) || (color2 == colorLeft) || (color2 == colorBack)) &&
        ((color3 == colorBottom) || (color3 == colorLeft) || (color3 == colorBack))) {
        moves.append(ROT_D); moves.append(ROT_F); moves.append(ROT_DCCW); moves.append(ROT_BCCW);
        moves.append(ROT_D); moves.append(ROT_FCCW); moves.append(ROT_DCCW); moves.append(ROT_B);
        return;
    }

    color1 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_RIGHT];
    color3 = m_pPieces[cube_size-1][0][0]->m_nSideColor[SD_BACK];

    if (((color1 == colorBottom) || (color1 == colorRight) || (color1 == colorBack)) &&
        ((color2 == colorBottom) || (color2 == colorRight) || (color2 == colorBack)) &&
        ((color3 == colorBottom) || (color3 == colorRight) || (color3 == colorBack))) {
        moves.append(ROT_D); moves.append(ROT_R); moves.append(ROT_DCCW); moves.append(ROT_LCCW);
        moves.append(ROT_D); moves.append(ROT_RCCW); moves.append(ROT_DCCW); moves.append(ROT_L);
        return;
    }

    color1 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_RIGHT];
    color3 = m_pPieces[cube_size-1][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if (((color1 == colorBottom) || (color1 == colorRight) || (color1 == colorFront)) &&
        ((color2 == colorBottom) || (color2 == colorRight) || (color2 == colorFront)) &&
        ((color3 == colorBottom) || (color3 == colorRight) || (color3 == colorFront))) {
        moves.append(ROT_D); moves.append(ROT_B); moves.append(ROT_DCCW); moves.append(ROT_FCCW);
        moves.append(ROT_D); moves.append(ROT_BCCW); moves.append(ROT_DCCW); moves.append(ROT_F);
        return;
    }

    color1 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if (((color1 == colorBottom) || (color1 == colorLeft) || (color1 == colorFront)) &&
        ((color2 == colorBottom) || (color2 == colorLeft) || (color2 == colorFront)) &&
        ((color3 == colorBottom) || (color3 == colorLeft) || (color3 == colorFront))) {
        moves.append(ROT_D); moves.append(ROT_L); moves.append(ROT_DCCW); moves.append(ROT_RCCW);
        moves.append(ROT_D); moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_R);
        return;
    }

    moves.append(ROT_D); moves.append(ROT_L); moves.append(ROT_DCCW); moves.append(ROT_RCCW);
    moves.append(ROT_D); moves.append(ROT_LCCW); moves.append(ROT_DCCW); moves.append(ROT_R);
}

//---------------------------------------------------------------------------
void TCube::orientationOfBlueCorners(void)
{
    SIDECOLOR color1, color2, color3;
    SIDECOLOR colorBottom, colorLeft, colorFront;

    colorBottom = BLUE;
    colorLeft   = m_pPieces[0][0][cube_mid_pos]->m_nSideColor[SD_LEFT];
    colorFront  = m_pPieces[cube_mid_pos][0][cube_size-1]->m_nSideColor[SD_FRONT];

    color1 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_BOTTOM];
    color2 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_LEFT];
    color3 = m_pPieces[0][0][cube_size-1]->m_nSideColor[SD_FRONT];

    if ((color1 != colorBottom) || (color2 != colorLeft) || (color3 != colorFront)) {
        moves.append(ROT_LCCW); moves.append(ROT_UCCW); moves.append(ROT_L); moves.append(ROT_U);
        return;
    }

    moves.append(ROT_D);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool TCube::solve(void)
{
    if (moves.length()) {
        switch (moves.at(0)) {
        case ROT_R    : RotateR(); break;
        case ROT_RCCW : RotateRCCW(); break;
        case ROT_L    : RotateL(); break;
        case ROT_LCCW : RotateLCCW(); break;
        case ROT_F    : RotateF(); break;
        case ROT_FCCW : RotateFCCW(); break;
        case ROT_B    : RotateB(); break;
        case ROT_BCCW : RotateBCCW(); break;
        case ROT_D    : RotateD(); break;
        case ROT_DCCW : RotateDCCW(); break;
        case ROT_U    : RotateU(); break;
        case ROT_UCCW : RotateUCCW(); break;
        }

        moves.erase(moves.begin()+0);
        return false;
    }

    if (blueEdgeOrientation) {
        orientationOfBlueCorners();
        return check();
    }

    SIDE whiteCrossSide = findWhiteCrossSide();

    if (!checkWhiteCross(whiteCrossSide)) {
        widget->setSolvingInterval(10);
        if (whiteCrossSide != SD_TOP) whiteCrossSideToTop(whiteCrossSide);
        else arrangeWhiteCross();
    }
    else {
        if (!checkWhiteCrossCorners(whiteCrossSide)) {
            widget->setSolvingInterval(10);
            arrangeWhiteCrossCorners();
        }
        else {
            if (!checkSecondLayer(whiteCrossSide)) {
                widget->setSolvingInterval(10);
                arrangeSecondLayer();
            }
            else {
                if (!checkBlueCross(whiteCrossSide)) {
                    arrangeBlueCross();
                }
                else {
                    if (!checkEdgePermutationOfBlueCross(whiteCrossSide)) {
                        arrangeEdgePermutationOfBlueCross();
                    }
                    else {
                        if (!checkPermutationOfBlueCorners(whiteCrossSide)) {
                            widget->setSolvingInterval(500);
                            permutationOfBlueCorners();
                        }
                        else {
                            if (!checkOrientationOfBlueCorners(whiteCrossSide)) {
                                widget->setSolvingInterval(500);
                                blueEdgeOrientation = true;
                                orientationOfBlueCorners();
                            }
                        }
                    }
                }
            }
        }

    }

    return check();
}
//---------------------------------------------------------------------------
